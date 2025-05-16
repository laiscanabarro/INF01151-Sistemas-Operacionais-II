#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "../comunication/comunicationClient.h"
#include <dirent.h>   // Para manipulação de diretórios
#include <sys/types.h> // Para tipos como DIR
//#include "../controle/controle.h"
#include <pthread.h>
#define PORTA 8080
#define IP "127.0.0.1"

// Definição e inicialização do mutex (uma única vez)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char diretorio[200];

int nTasks=0;
struct pendingTask{
    notification_t notification;
    time_t  time;

};
struct pendingTask *pendingTasks;


// Função para obter o tempo atual
time_t obterTempoAtual() {
    return time(NULL);
}

// Função para inserir novas tarefas no vetor pendingTasks
void insertNewTasks(notification_t *notifications, int num_notifications, struct pendingTask **pendingTasks, int *nTasks) {
    if (num_notifications <= 0) return;

    // Obter o tempo atual uma única vez
    time_t tempoAtual = obterTempoAtual();

    // Vetor temporário para armazenar as novas tarefas
    struct pendingTask *novasTarefas = malloc(num_notifications * sizeof(struct pendingTask));
    if (novasTarefas == NULL) {
        perror("Erro ao alocar memória para novas tarefas");
        return;
    }

    // Preencher o vetor temporário com as novas notificações
    for (int i = 0; i < num_notifications; i++) {
        novasTarefas[i].notification = notifications[i];
        novasTarefas[i].time = tempoAtual;
    }

    // Realocar o vetor original para acomodar as novas tarefas
    struct pendingTask *novoVetor = realloc(*pendingTasks, (*nTasks + num_notifications) * sizeof(struct pendingTask));
    if (novoVetor == NULL) {
        perror("Erro ao realocar memória para pendingTasks");
        free(novasTarefas);
        return;
    }

    // Atualizar o ponteiro para o vetor reallocado
    *pendingTasks = novoVetor;

    // Copiar as novas tarefas para o final do vetor original
    memcpy(&((*pendingTasks)[*nTasks]), novasTarefas, num_notifications * sizeof(struct pendingTask));

    // Atualizar o número total de tarefas
    *nTasks += num_notifications;

    // Liberar o vetor temporário
    free(novasTarefas);
}


// Função para obter o tempo atual em formato legível
void imprimirTempo(time_t tempo) {
    struct tm *tm_info = localtime(&tempo);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s", buffer);
}

// Função para obter o nome da notificação a partir do enum
const char* obterNomeNotificacao(notification_type_t tipo) {
    switch (tipo) {
        case UPDATED_FILE: return "UPDATED_FILE";
        case RENAMED_FILE: return "RENAMED_FILE";
        case REMOVED_FILE: return "REMOVED_FILE";
        default: return "UNKNOWN";
    }
}
static void removeTaskAt(int idx) {
    for (int k = idx; k < nTasks - 1; k++) {
        pendingTasks[k] = pendingTasks[k + 1];
    }
    nTasks--;
}

// Procura o índice da próxima tarefa para o mesmo arquivo, começando em start+1
static int findNextForFile(const char *fileName, int start) {
    for (int i = start + 1; i < nTasks; i++) {
        if (strcmp(pendingTasks[i].notification.fileName, fileName) == 0 ||
            (pendingTasks[i].notification.type == RENAMED_FILE &&
             strcmp(pendingTasks[i].notification.ancientFileName, fileName) == 0)) {
            return i;
        }
    }
    return -1;
}

// Troca UPDATE A e RENAME A→B de lugar e faz o UPDATE usar B
void filterTasksAux(void) {
    for (int i = 0; i < nTasks; i++) {
        notification_t *ni = &pendingTasks[i].notification;
        if (ni->type == UPDATED_FILE) {
            char origName[200];
            strcpy(origName, ni->fileName);

            int j = findNextForFile(origName, i);
            if (j != -1) {
                notification_t *nj = &pendingTasks[j].notification;
                // só prossegue se for um RENAME A→B
                if (nj->type == RENAMED_FILE
                    && strcmp(nj->ancientFileName, origName) == 0) {
                    // guarda cópias
                    struct pendingTask taskU = pendingTasks[i];
                    struct pendingTask taskR = pendingTasks[j];

                    // ajusta o UPDATE para usar o nome B
                    strcpy(taskU.notification.fileName, taskR.notification.fileName);
                    taskU.notification.ancientFileName[0] = '\0';

                    // swap: coloca RENAME na posição i e UPDATE em j
                    pendingTasks[i] = taskR;
                    pendingTasks[j] = taskU;
                }
            }
        }
    }
}


void filterTasks(void) {
    int i = 0;
    while (i < nTasks) {
        notification_t *notif = &pendingTasks[i].notification;
        const char *name = (notif->type == RENAMED_FILE && notif->ancientFileName[0] != '\0')
                            ? notif->ancientFileName
                            : notif->fileName;

        // 1) Sequência de U/D
        if (notif->type == UPDATED_FILE || notif->type == REMOVED_FILE) {
            int lastType = notif->type;  // tipo da última U ou D encontrada
            int j = findNextForFile(name, i);
            while (j != -1 &&
                  (pendingTasks[j].notification.type == UPDATED_FILE ||
                   pendingTasks[j].notification.type == REMOVED_FILE)) {
                lastType = pendingTasks[j].notification.type;
                removeTaskAt(j);
                // não incrementa j, pois vetor encurtou: next fica em mesma posição j
                j = findNextForFile(name, i);
            }
            // agora lastType tem U ou D final -> substituímos o de i
            pendingTasks[i].notification.type = lastType;
            i++;
        }
        // 2) Sequência de R encadeados
        else if (notif->type == RENAMED_FILE) {
            // coleta a cadeia de renames
            char origem[200], destino[200];
            strcpy(origem, notif->ancientFileName);
            strcpy(destino, notif->fileName);

            int j = findNextForFile(destino, i);
            while (j != -1 && pendingTasks[j].notification.type == RENAMED_FILE) {
                // concatenar rename: origem -> pendingTasks[j].notification.fileName
                strcpy(destino, pendingTasks[j].notification.fileName);
                removeTaskAt(j);
                j = findNextForFile(destino, i);
            }
            // atualiza o i-ésimo para origem->destino
            strcpy(pendingTasks[i].notification.ancientFileName, origem);
            strcpy(pendingTasks[i].notification.fileName, destino);
            i++;
        }
        else {
            // algum outro tipo (se houver), apenas avança
            i++;
        }
    }
}

// Função que será executada pela nova thread
void* minhaThread1(void* arg) {
    notification_t notifications[300];
    while (1) {
        int min = 100000;
        int max = 250000;
        int aleatorio = min + rand() % (max - min + 1);
        usleep(aleatorio);  // Aguardar meio segundo (500 milissegundos)
        int num_notifications = receiveLastSecondNotificationFromServer(notifications, PORTA, IP);
        pthread_mutex_lock(&mutex);
        insertNewTasks(notifications, num_notifications, &pendingTasks, &nTasks);
        //filtra tarefas repetidas ou que se sobrepoem
        
        filterTasks();
        filterTasksAux();
        filterTasks();
        pthread_mutex_unlock(&mutex);
        // Imprimir todas as tarefas pendentes
        printf("\n== Lista de Tarefas Pendentes (%d tarefas) ==\n", nTasks);
        for (int i = 0; i < nTasks; i++) {
            printf("Arquivo: %s | Tipo: %s", 
                   pendingTasks[i].notification.fileName, 
                   obterNomeNotificacao(pendingTasks[i].notification.type));

            if (pendingTasks[i].notification.type == RENAMED_FILE) {
                printf(" | Nome Anterior: %s", pendingTasks[i].notification.ancientFileName);
            }

            printf(" | Tempo: ");
            imprimirTempo(pendingTasks[i].time);
            printf("\n");
        }
        fflush(stdout);  // Garantir que a saída seja exibida imediatamente
    }

    return NULL;
}
void deleteFile(char * nome_arquivo){
    char caminho_completo[300];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    remove(caminho_completo);
    pthread_mutex_lock(&mutex);
    removeTaskAt(0);
    pthread_mutex_unlock(&mutex);

}
void renameFile(char * nome_arquivo_novo,char * nome_arquivo_antigo){
    char caminho_completo_novo[300];
    snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", diretorio, nome_arquivo_novo);
    char caminho_completo_antigo[300];
    snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", diretorio, nome_arquivo_antigo);
    
    pthread_mutex_lock(&mutex);
    removeTaskAt(0);
    pthread_mutex_unlock(&mutex);
    rename(caminho_completo_antigo, caminho_completo_novo);

}
void updateFile(char * nome_arquivo){
    char caminho_completo[500];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    receiveNewFileFromServer(nome_arquivo,diretorio,PORTA,IP);
    pthread_mutex_lock(&mutex);
    removeTaskAt(0);
    pthread_mutex_unlock(&mutex);

}
void* minhaThread2(void* arg) {
    while(1){
        if(nTasks!=0){
            struct pendingTask task= pendingTasks[0];
        switch (task.notification.type)
        {
        case 0:
            updateFile(task.notification.fileName);
            break;
         case 1:
            renameFile(task.notification.fileName,task.notification.ancientFileName);
            break;
         case 2:
            deleteFile(task.notification.fileName);
            break;
        
        default:
            break;
        }



        }
        


    }
    

}

/*
void* minhaThread(void* arg) {
    notification_t notifications[300];
     printf("ver novas modificiacoes \n");
        fflush(stdout);
    
    while(1){
        
        usleep(500000);
        
        int num_notifications = receiveLastSecondNotificationFromServer(notifications, PORTA, "127.0.0.1");
        if(num_notifications==0){
            printf("sem mofificacoes no ultimo segundo");
            printf("\n");
            fflush(stdout);
        }
        
        else{
            for (int i = 0; i < num_notifications; i++) {
        printf("Arquivo: %s | Tipo: %d", notifications[i].fileName, notifications[i].type);
        if (notifications[i].type == RENAMED_FILE) {
            printf(" | Nome Anterior: %s", notifications[i].ancientFileName);
        }
        printf("\n");


        }
        printf("modificacoes nesse segundo \n");
        fflush(stdout);
        }
        
    }



    
    
    return 0;
}*/


#include <dirent.h>   // Para manipulação de diretórios
#include <sys/types.h> // Para tipos como DIR

void iniciaDiretorioCliente() {
    DIR *dir;
    struct dirent *entry;

    // 1. Limpar o diretório do cliente
    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretório do cliente");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construir o caminho completo para o arquivo
        char caminho_completo[500];
        snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, entry->d_name);

        // Remover o arquivo
        if (remove(caminho_completo) != 0) {
            perror("Erro ao deletar arquivo");
        }
    }
    closedir(dir);
    
    // 2. Obter os nomes dos arquivos que estão no servidor
    char **arquivosServidor;
    int num_arquivos = receiveFileListFromServer(&arquivosServidor, PORTA, IP);
    
    if (num_arquivos < 0) {
        perror("Erro ao obter lista de arquivos do servidor");
        return;
    }

    // 3. Inserir as tarefas no vetor pendingTasks (usando mutex)
    pthread_mutex_lock(&mutex);
   
    notification_t *notificacoes = malloc(num_arquivos * sizeof(notification_t));
    if (notificacoes == NULL) {
        perror("Erro ao alocar memória para notificações");
        pthread_mutex_unlock(&mutex);
        return;
    }
    
    for (int i = 0; i < num_arquivos; i++) {
        strcpy(notificacoes[i].fileName, arquivosServidor[i]);
        notificacoes[i].type = UPDATED_FILE;
    }
    
    insertNewTasks(notificacoes, num_arquivos, &pendingTasks, &nTasks);

    free(notificacoes);
    pthread_mutex_unlock(&mutex);
}


int main() {
    
    printf("Digite o nome do diretorio do cleinte: \n");
    
    fgets(diretorio,200,stdin);
    //printf("passou");
    diretorio[strlen(diretorio)-1]='\0';
    
    iniciaDiretorioCliente();
    pthread_t thread1,thread2;  // Identificador da thread
    int id = 1;

    // Criando a nova thread
    if (pthread_create(&thread1, NULL, minhaThread1, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }
    // Criando a nova thread
    if (pthread_create(&thread2, NULL, minhaThread2, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }

    // Aguarda a conclusão da thread
    pthread_join(thread1, NULL);
    printf("Thread principal: Thread finalizada!\n");
    

    return 0;
}
