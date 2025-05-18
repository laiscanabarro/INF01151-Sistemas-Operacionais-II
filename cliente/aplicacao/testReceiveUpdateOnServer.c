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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;//
pthread_mutex_t mutex5 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCurSen= PTHREAD_MUTEX_INITIALIZER;//
pthread_mutex_t mutexTasksToServer= PTHREAD_MUTEX_INITIALIZER;//
char diretorio[200];
int nTasksToServer=0;
int nTasks=0;
int nCurrentOrRecentTasksRecv=0;
int nCurrentOrRecentTasksSended;
struct Task{
    notification_t notification;
    time_t  time;
    int executing;

};
struct Task *pendingTasks;
struct Task *pendingTasksToServer;
struct Task *currentOrRecentTasksRecv;
struct Task *currentOrRecentTasksSended;


void insertTaskToEnd(
    struct Task **taskArray,       // Vetor de tarefas
    int *taskCount,                // Número atual de tarefas
    const char *fileName,          // Nome do arquivo
    const char *ancientFileName,   // Nome antigo do arquivo
    notification_type_t type,      // Tipo de modificação
    int executing                 // Indicador de execução (0 ou 1)
) {
    // Realoca memória para adicionar uma nova tarefa no final
    struct Task *newArray = realloc(*taskArray, (*taskCount + 1) * sizeof(struct Task));
    if (newArray == NULL) {
        perror("Erro ao realocar memória para as tarefas");
        return;
    }

    // Atualiza o ponteiro para o vetor reallocado
    *taskArray = newArray;

    // Preenche os campos da nova tarefa
    struct Task *newTask = &(*taskArray)[*taskCount];
    strncpy(newTask->notification.fileName, fileName, sizeof(newTask->notification.fileName) - 1);
    strncpy(newTask->notification.ancientFileName, ancientFileName, sizeof(newTask->notification.ancientFileName) - 1);
    newTask->notification.type = type;
    newTask->time = time(NULL);
    newTask->executing = executing;

    // Atualiza a contagem de tarefas
    (*taskCount)++;
}



// Função para obter o tempo atual
time_t obterTempoAtual() {
    return time(NULL);
}
int getIndex(struct Task **tasks, int *nTasks, const char *fileName, const char *ancientFileName, notification_type_t type, int executing) {
    for (int i = 0; i < *nTasks; i++) {
        // Verifica o nome do arquivo
        if (strcmp((*tasks)[i].notification.fileName, fileName) == 0 &&
            // Verifica o nome antigo (se não for NULL)
            (ancientFileName == NULL || strcmp((*tasks)[i].notification.ancientFileName, ancientFileName) == 0) &&
            // Verifica o tipo de notificação
            (*tasks)[i].notification.type == type &&
            // Verifica se está em execução
            (*tasks)[i].executing == executing) {
            return i;
        }
    }
    return -1;  // Não encontrado
}
// Função para inserir novas tarefas no vetor pendingTasks
void insertNewTasks(notification_t *notifications, int num_notifications, struct Task **pendingTasks, int *nTasks) {
    if (num_notifications <= 0) return;

    // Obter o tempo atual uma única vez
    time_t tempoAtual = obterTempoAtual();

    // Vetor temporário para armazenar as novas tarefas
    struct Task *novasTarefas = malloc(num_notifications * sizeof(struct Task));
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
    struct Task *novoVetor = realloc(*pendingTasks, (*nTasks + num_notifications) * sizeof(struct Task));
    if (novoVetor == NULL) {
        perror("Erro ao realocar memória para pendingTasks");
        free(novasTarefas);
        return;
    }

    // Atualizar o ponteiro para o vetor reallocado
    *pendingTasks = novoVetor;

    // Copiar as novas tarefas para o final do vetor original
    memcpy(&((*pendingTasks)[*nTasks]), novasTarefas, num_notifications * sizeof(struct Task));

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
static void removeTaskAt(struct Task **tasks, int *nTasks, int idx) {
    for (int k = idx; k < *nTasks - 1; k++) {
        (*tasks)[k] = (*tasks)[k + 1];
    }
    (*nTasks)--;
}

// Procura o índice da próxima tarefa para o mesmo arquivo, começando em start+1
static int findNextForFile(struct Task *tasks, int nTasks, const char *fileName, int start) {
    for (int i = start + 1; i < nTasks; i++) {
        if (strcmp(tasks[i].notification.fileName, fileName) == 0 ||
            (tasks[i].notification.type == RENAMED_FILE &&
             strcmp(tasks[i].notification.ancientFileName, fileName) == 0)) {
            return i;
        }
    }
    return -1;
}

// Troca UPDATE A e RENAME A→B de lugar e faz o UPDATE usar B
void filterTasksAux(struct Task *Tasks, int *nTasks) {
    for (int i = 0; i < *nTasks; i++) {
        notification_t *ni = &Tasks[i].notification;
        if (ni->type == UPDATED_FILE) {
            char origName[200];
            strcpy(origName, ni->fileName);

            int j = findNextForFile(Tasks, *nTasks, origName, i);
            if (j != -1) {
                notification_t *nj = &Tasks[j].notification;
                if (nj->type == RENAMED_FILE && strcmp(nj->ancientFileName, origName) == 0) {
                    struct Task taskU = Tasks[i];
                    struct Task taskR = Tasks[j];

                    strcpy(taskU.notification.fileName, taskR.notification.fileName);
                    taskU.notification.ancientFileName[0] = '\0';

                    Tasks[i] = taskR;
                    Tasks[j] = taskU;
                }
            }
        }
    }
}


void filterTasks(struct Task *Tasks, int *nTasks) {
    int i = 0;
    while (i < *nTasks) {
        notification_t *notif = &Tasks[i].notification;
        const char *name = (notif->type == RENAMED_FILE && notif->ancientFileName[0] != '\0')
                            ? notif->ancientFileName
                            : notif->fileName;

        if (notif->type == UPDATED_FILE || notif->type == REMOVED_FILE) {
            int lastType = notif->type;
            int j = findNextForFile(Tasks, *nTasks, name, i);
            while (j != -1 && (Tasks[j].notification.type == UPDATED_FILE || Tasks[j].notification.type == REMOVED_FILE)) {
                lastType = Tasks[j].notification.type;
                removeTaskAt(&Tasks, nTasks,j);
                j = findNextForFile(Tasks, *nTasks, name, i);
            }
            Tasks[i].notification.type = lastType;
            i++;
        } else if (notif->type == RENAMED_FILE) {
            char origem[200], destino[200];
            strcpy(origem, notif->ancientFileName);
            strcpy(destino, notif->fileName);

            int j = findNextForFile(Tasks, *nTasks, destino, i);
            while (j != -1 && Tasks[j].notification.type == RENAMED_FILE) {
                strcpy(destino, Tasks[j].notification.fileName);
                removeTaskAt(&Tasks, nTasks,j);
                j = findNextForFile(Tasks, *nTasks, destino, i);
            }
            strcpy(Tasks[i].notification.ancientFileName, origem);
            strcpy(Tasks[i].notification.fileName, destino);
            i++;
        } else {
            i++;
        }
    }
}
int isSameNotification(notification_t *a, notification_t *b) {
    return (strcmp(a->fileName, b->fileName) == 0 &&
            strcmp(a->ancientFileName, b->ancientFileName) == 0 &&
            a->type == b->type);
}

void removeDuplicateTasks(struct Task *tasks1, int *size1, struct Task *tasks2, int size2) {
    for (int i = 0; i < *size1; i++) {
        for (int j = 0; j < size2; j++) {
            if (isSameNotification(&tasks1[i].notification, &tasks2[j].notification)) {
                removeTaskAt(&tasks1, size1, i);
                i--; // Recuar o índice para verificar a nova tarefa na posição atual
                break;
            }
        }
    }
}

// Função que será executada pela nova thread

void* minhaThread1(void* arg) {
    
    notification_t notifications[300];
    while (1) {
        
        usleep(300000);  // Aguardar meio segundo (500 milissegundos)
        
          pthread_mutex_lock(&mutex);
        int num_notifications = receiveLastSecondNotificationFromServer(notifications, PORTA, IP);
         
       
       
        insertNewTasks(notifications, num_notifications, &pendingTasks, &nTasks);
        printf("tasks %d \n\n", nTasks);
        
        pthread_mutex_lock(&mutexCurSen);
        removeDuplicateTasks(pendingTasks, &nTasks, currentOrRecentTasksSended, nCurrentOrRecentTasksSended);
        pthread_mutex_unlock(&mutexCurSen);
        //filtra tarefas repetidas ou que se sobrepoem
        
        filterTasks(pendingTasks, &nTasks);
        filterTasksAux(pendingTasks, &nTasks);
        filterTasks(pendingTasks, &nTasks);
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
        fflush(stdout); 
 // Garantir que a saída seja exibida imediatamente
       
    }
    return NULL;
    
}

void deleteFile(char * nome_arquivo){
    char caminho_completo[300];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    // Exemplo de uso
    pthread_mutex_lock(&mutex3);
   
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", REMOVED_FILE, 1);
    
    pthread_mutex_unlock(&mutex3);
    remove(caminho_completo);
     
    pthread_mutex_lock(&mutex3);
    int j=getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", REMOVED_FILE, 1);
    currentOrRecentTasksRecv[j].executing=0;
    currentOrRecentTasksRecv[j].time=time(NULL);
    pthread_mutex_unlock(&mutex3);

    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks,0);
    pthread_mutex_unlock(&mutex);
    

}
void renameFile(char * nome_arquivo_novo,char * nome_arquivo_antigo){
    char caminho_completo_novo[300];
    snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", diretorio, nome_arquivo_novo);
    char caminho_completo_antigo[300];
    snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", diretorio, nome_arquivo_antigo);
    pthread_mutex_lock(&mutex3);
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    pthread_mutex_unlock(&mutex3);
    rename(caminho_completo_antigo, caminho_completo_novo);
       
    pthread_mutex_lock(&mutex3);
    int j=getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    currentOrRecentTasksRecv[j].executing=0;
    currentOrRecentTasksRecv[j].time=time(NULL);
    pthread_mutex_unlock(&mutex3);

    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks,0);
    pthread_mutex_unlock(&mutex);

}
void updateFile(char * nome_arquivo){
   
    char caminho_completo[500];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    pthread_mutex_lock(&mutex3);
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", UPDATED_FILE, 1);
    pthread_mutex_unlock(&mutex3);
    receiveNewFileFromServer(nome_arquivo,diretorio,PORTA,IP);

    pthread_mutex_lock(&mutex3);
    int j=getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", UPDATED_FILE, 1);
    currentOrRecentTasksRecv[j].executing=0;
    currentOrRecentTasksRecv[j].time=time(NULL);
    pthread_mutex_unlock(&mutex3);
    
    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks,0);
    
    pthread_mutex_unlock(&mutex);
   

}

void* minhaThread2(void* arg) {
    
    while(1){
        
        
       
        usleep(100000);
        
        pthread_mutex_lock(&mutex);
        
        
        if(nTasks!=0&&getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, pendingTasks[0].notification.fileName, pendingTasks[0].notification.ancientFileName,
        pendingTasks[0].notification.type, 0)==-1){
          
            struct Task task= pendingTasks[0];
            pthread_mutex_unlock(&mutex);
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
        else{
            pthread_mutex_unlock(&mutex);
        }
        
        


    }
    

}

void* minhaThread3(void* arg) {
    
    while(1){
   
    usleep(500000);
    notification_t notifications[300];
    int num_notifications = receiveLastSecondLocalNotification(notifications,diretorio);
   
    
    pthread_mutex_lock(&mutexTasksToServer);
        insertNewTasks(notifications, num_notifications, &pendingTasksToServer, &nTasksToServer);
        // Imprimir todas as tarefas pendentes

        pthread_mutex_lock(&mutex3);
        removeDuplicateTasks(pendingTasksToServer, &nTasksToServer, currentOrRecentTasksRecv, nCurrentOrRecentTasksRecv);
        pthread_mutex_unlock(&mutex3);

        filterTasks(pendingTasksToServer, &nTasksToServer);
        filterTasksAux(pendingTasksToServer, &nTasksToServer);
        filterTasks(pendingTasksToServer, &nTasksToServer);
    pthread_mutex_unlock(&mutexTasksToServer);
        
        

       
        /*
        // Imprimir todas as tarefas pendentes
        printf("\n== Lista de Tarefas Pendentes (%d tarefas) ==\n", nTasksToServer);
        for (int i = 0; i < nTasksToServer; i++) {
            printf("1 taskToServer %d", nTasksToServer);
            printf("Arquivo: %s | Tipo: %s", 
                   pendingTasksToServer[i].notification.fileName, 
                   obterNomeNotificacao(pendingTasksToServer[i].notification.type));

            if (pendingTasksToServer[i].notification.type == RENAMED_FILE) {
                printf(" | Nome Anterior: %s", pendingTasksToServer[i].notification.ancientFileName);
            }
            printf("1 taskToServer %d", nTasksToServer);
            printf(" | Tempo: ");
            imprimirTempo(pendingTasksToServer[i].time);
            printf("\n");
            printf("1 taskToServer %d", nTasksToServer);
        }*/
        fflush(stdout); 
       
    }

     
    

}
void* minhaThread4(void* arg) {

    while (1) {
       
        usleep(300000);  // Atraso de 100 milissegundos (0.1 segundo)

        time_t now = time(NULL);  // Obter o tempo atual
        pthread_mutex_lock(&mutex3);
        for (int i = 0; i < nCurrentOrRecentTasksRecv; i++) {
            double diff = difftime(now, currentOrRecentTasksRecv[i].time);

            // Verificar se o tempo é maior que 1.5 segundos e executing é 0
            if (diff > 1.5 && currentOrRecentTasksRecv[i].executing == 0) {
                removeTaskAt(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, i);
                i--;  // Ajusta o índice após a remoção
            }
        }
        /*
        printf("\n== Lista de Tarefas Pendentes (%d tarefas) ==\n", nCurrentOrRecentTasksRecv);
        for (int i = 0; i <nCurrentOrRecentTasksRecv; i++) {
            printf("Arquivo: %s | Tipo: %s", 
                  currentOrRecentTasksRecv[i].notification.fileName, 
                   obterNomeNotificacao(currentOrRecentTasksRecv[i].notification.type));

            if (currentOrRecentTasksRecv[i].notification.type == RENAMED_FILE) {
                printf(" | Nome Anterior: %s", currentOrRecentTasksRecv[i].notification.ancientFileName);
            }

            printf(" | Tempo: ");
            imprimirTempo(currentOrRecentTasksRecv[i].time);
            printf("\n");
        }
        fflush(stdout); */
        pthread_mutex_unlock(&mutex3);
        
    }
    return NULL;
}
void updateFileServer(char * nome_arquivo){
   
    char caminho_completo[500];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    pthread_mutex_lock(& mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", UPDATED_FILE, 1);
    pthread_mutex_unlock(& mutexCurSen);

    sendNewFileToServer(nome_arquivo,diretorio,PORTA,IP);

    pthread_mutex_lock(& mutexCurSen);
    int j=getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", UPDATED_FILE, 1);
    currentOrRecentTasksSended[j].executing=0;
    currentOrRecentTasksSended[j].time=time(NULL);
   
    pthread_mutex_unlock(& mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer,0);
    
    pthread_mutex_unlock(&mutexTasksToServer);
   

}
void removeFileServer(char * nome_arquivo){
   
    char caminho_completo[500];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    pthread_mutex_lock(& mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", REMOVED_FILE, 1);
    pthread_mutex_unlock(& mutexCurSen);
    removeFileInServer(nome_arquivo,diretorio,PORTA,IP);

    pthread_mutex_lock(& mutexCurSen);
    int j=getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", REMOVED_FILE, 1);
    currentOrRecentTasksSended[j].executing=0;
    currentOrRecentTasksSended[j].time=time(NULL);
    pthread_mutex_unlock(& mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer,0);
    
    pthread_mutex_unlock(&mutexTasksToServer);
   

}

void renameFileServer(char * nome_arquivo_novo,char * nome_arquivo_antigo){
   
    pthread_mutex_lock(& mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    pthread_mutex_unlock(& mutexCurSen);

    updateFileName(nome_arquivo_novo,nome_arquivo_antigo,diretorio,PORTA,  IP);

    pthread_mutex_lock(& mutexCurSen);
    int j=getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    currentOrRecentTasksSended[j].executing=0;
    currentOrRecentTasksSended[j].time=time(NULL);
    pthread_mutex_unlock(& mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer,0);
    
    pthread_mutex_unlock(&mutexTasksToServer);
   

}
void* minhaThread5(void* arg) {
    
    while(1){
        
        
       
        usleep(100000);
        pthread_mutex_lock(&mutexTasksToServer);
        
        
        
        if(nTasksToServer!=0&&getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, pendingTasksToServer[0].notification.fileName, pendingTasksToServer[0].notification.ancientFileName,
        pendingTasksToServer[0].notification.type, 0)==-1){
          
            struct Task task= pendingTasksToServer[0];
            pthread_mutex_unlock(&mutexTasksToServer);
        switch (task.notification.type)
        {
        case 0:
           
            updateFileServer(task.notification.fileName);
            break;
         case 1:
            renameFileServer(task.notification.fileName,task.notification.ancientFileName);
            break;
         case 2:
            removeFileServer(task.notification.fileName);
            break;
        
        default:
            break;
        }
        
        



        }
        pthread_mutex_unlock(&mutexTasksToServer);
        
        


    }
    

}
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
    printf("arquivos %d \n\n", num_arquivos);
    insertNewTasks(notificacoes, num_arquivos, &pendingTasks, &nTasks);
    printf("tasks %d \n\n", nTasks);
    free(notificacoes);
    
}
void* minhaThread6(void* arg) {

    while (1) {
       
        usleep(300000);  // Atraso de 100 milissegundos (0.1 segundo)

        time_t now = time(NULL);  // Obter o tempo atual
        pthread_mutex_lock(&mutexCurSen);
        for (int i = 0; i < nCurrentOrRecentTasksSended; i++) {
            double diff = difftime(now, currentOrRecentTasksSended[i].time);

            // Verificar se o tempo é maior que 1.5 segundos e executing é 0
            if (diff > 1.5 && currentOrRecentTasksSended[i].executing == 0) {
                removeTaskAt(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, i);
                i--;  // Ajusta o índice após a remoção
            }
        }
        /*
        printf("\n== Lista de Tarefas Pendentes (%d tarefas) ==\n", nCurrentOrRecentTasksRecv);
        for (int i = 0; i <nCurrentOrRecentTasksRecv; i++) {
            printf("Arquivo: %s | Tipo: %s", 
                  currentOrRecentTasksRecv[i].notification.fileName, 
                   obterNomeNotificacao(currentOrRecentTasksRecv[i].notification.type));

            if (currentOrRecentTasksRecv[i].notification.type == RENAMED_FILE) {
                printf(" | Nome Anterior: %s", currentOrRecentTasksRecv[i].notification.ancientFileName);
            }

            printf(" | Tempo: ");
            imprimirTempo(currentOrRecentTasksRecv[i].time);
            printf("\n");
        }
        fflush(stdout); */
        pthread_mutex_unlock(&mutexCurSen);
        
    }
    return NULL;
}

int main() {
    
    printf("Digite o nome do diretorio do cleinte: \n");
    
    fgets(diretorio,200,stdin);
    //printf("passou");
    diretorio[strlen(diretorio)-1]='\0';
    
    iniciaDiretorioCliente();
    pthread_t thread1,thread2,thread3,thread4,thread5,thread6;  // Identificador da thread
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
    // Criando a nova thread
    if (pthread_create(&thread3, NULL, minhaThread3, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }
    // Criando a nova thread
    if (pthread_create(&thread4, NULL, minhaThread4, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }
    // Criando a nova thread
    if (pthread_create(&thread5, NULL, minhaThread5, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }
     // Criando a nova thread
    if (pthread_create(&thread6, NULL, minhaThread6, &id) != 0) {
        perror("Erro ao criar a thread");
        return 1;
    }

    // Aguarda a conclusão da thread
    pthread_join(thread1, NULL);
    printf("Thread principal: Thread finalizada!\n");
    

    return 0;
}
