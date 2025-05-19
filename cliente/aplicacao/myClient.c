#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <arpa/inet.h>
#include "../comunication/comunicationClient.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_PATH_SIZE 500

typedef struct {
    char username[50];
    char sync_dir_path[MAX_PATH_SIZE];
    char server_ip[16];
    int server_port;
    int running;
} client_info_t;

client_info_t client_info;

// Múltiplos mutexes para controle de acesso concorrente
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;         
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;         
pthread_mutex_t mutex5 = PTHREAD_MUTEX_INITIALIZER;         
pthread_mutex_t mutexCurSen = PTHREAD_MUTEX_INITIALIZER;    
pthread_mutex_t mutexTasksToServer = PTHREAD_MUTEX_INITIALIZER; 

// Contadores de tarefas para os diferentes vetores
int nTasks = 0;                      
int nTasksToServer = 0;              
int nCurrentOrRecentTasksRecv = 0;   
int nCurrentOrRecentTasksSended = 0; 

// Estrutura de tarefas com flag de execução
struct Task {
    notification_t notification;
    time_t time;
    int executing;  
};

// Múltiplos vetores de tarefas para diferentes propósitos
struct Task *pendingTasks;               
struct Task *pendingTasksToServer;       
struct Task *currentOrRecentTasksRecv;   
struct Task *currentOrRecentTasksSended; 

// Função para verificar se o diretório existe, se não, criar

int check_and_create_directory(const char *path) {
    DIR *dir = opendir(path);
    if (dir) {
        closedir(dir);
        printf("Diretório %s já existe.\n", path);
        return 0;
    } else {
        if (mkdir(path, 0755) == 0) {
            printf("Diretório %s criado com sucesso.\n", path);
            return 0;
        } else {
            perror("Erro ao criar diretório");
            return 1;
        }
    }
}

// Função para verificar/criar o diretório do cliente
int setup_client_directory(char *username) {
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        fprintf(stderr, "Não foi possível obter o diretório home do usuário.\n");
        return 1;
    }
    
    char env_var_name[100];
    snprintf(env_var_name, sizeof(env_var_name), "SYNC_DIR_%s", username);
    
    char *env_dir = getenv(env_var_name);
    if (env_dir) {
        strncpy(client_info.sync_dir_path, env_dir, MAX_PATH_SIZE);
        printf("Usando diretório de sincronização definido na variável de ambiente: %s\n", client_info.sync_dir_path);
    } else {
        snprintf(client_info.sync_dir_path, MAX_PATH_SIZE, "%s/sync_dir_%s", home_dir, username);
        printf("Diretório de sincronização não definido em variável de ambiente.\n");
        printf("Usando diretório padrão: %s\n", client_info.sync_dir_path);
    }
    
    return check_and_create_directory(client_info.sync_dir_path);
}

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

// Funções para manipulação de arquivos
void updateFile(char *nome_arquivo) {
    char caminho_completo[MAX_PATH_SIZE];
    int written = snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", client_info.sync_dir_path, nome_arquivo);
    if (written < 0 || written >= sizeof(caminho_completo)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, nome_arquivo);
        return;
    }
    
    pthread_mutex_lock(&mutex3);
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", UPDATED_FILE, 1);
    pthread_mutex_unlock(&mutex3);
    
    receiveNewFileFromServer(nome_arquivo, client_info.sync_dir_path, client_info.server_port, client_info.server_ip);
    
    pthread_mutex_lock(&mutex3);
    int j = getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", UPDATED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksRecv[j].executing = 0;
    }
    pthread_mutex_unlock(&mutex3);
    
    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks, 0);
    pthread_mutex_unlock(&mutex);
}

void renameFile(char *nome_arquivo_novo, char *nome_arquivo_antigo) {
    char caminho_completo_novo[MAX_PATH_SIZE];
    int written;
    written = snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", client_info.sync_dir_path, nome_arquivo_novo);
    if (written < 0 || written >= sizeof(caminho_completo_novo)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, nome_arquivo_novo);
        return;
    }
    
    char caminho_completo_antigo[MAX_PATH_SIZE];
    written = snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", client_info.sync_dir_path, nome_arquivo_antigo);
    if (written < 0 || written >= sizeof(caminho_completo_antigo)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, nome_arquivo_antigo);
        return;
    }

    pthread_mutex_lock(&mutex3);
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    pthread_mutex_unlock(&mutex3);
    
    rename(caminho_completo_antigo, caminho_completo_novo);
    
    pthread_mutex_lock(&mutex3);
    int j = getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksRecv[j].executing = 0;
    }
    pthread_mutex_unlock(&mutex3);
    
    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks, 0);
    pthread_mutex_unlock(&mutex);
}

void deleteFile(char *nome_arquivo) {
    char caminho_completo[MAX_PATH_SIZE];
    int written = snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", client_info.sync_dir_path, nome_arquivo);
    if (written < 0 || written >= sizeof(caminho_completo)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, nome_arquivo);
        return;
    }
    
    pthread_mutex_lock(&mutex3);
    insertTaskToEnd(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", REMOVED_FILE, 1);
    pthread_mutex_unlock(&mutex3);
    
    if (remove(caminho_completo) != 0) {
        perror("Erro ao deletar arquivo");
        return;
    }
    
    pthread_mutex_lock(&mutex3);
    int j = getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, nome_arquivo, "", REMOVED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksRecv[j].executing = 0;
    }
    pthread_mutex_unlock(&mutex3);
    
    pthread_mutex_lock(&mutex);
    removeTaskAt(&pendingTasks, &nTasks, 0);
    pthread_mutex_unlock(&mutex);
}

// Funções para enviar alterações ao servidor
void updateFileServer(char *nome_arquivo) {
    char caminho_completo[MAX_PATH_SIZE];
    int written = snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", client_info.sync_dir_path, nome_arquivo);
    if (written < 0 || written >= sizeof(caminho_completo)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, nome_arquivo);
        return;
    }
    
    pthread_mutex_lock(&mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", UPDATED_FILE, 1);
    pthread_mutex_unlock(&mutexCurSen);
    
    sendNewFileToServer(nome_arquivo, client_info.sync_dir_path, client_info.server_port, client_info.server_ip);
    
    pthread_mutex_lock(&mutexCurSen);
    int j = getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", UPDATED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksSended[j].executing = 0;
    }
    pthread_mutex_unlock(&mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer, 0);
    pthread_mutex_unlock(&mutexTasksToServer);
}

void removeFileServer(char *nome_arquivo) {
    pthread_mutex_lock(&mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", REMOVED_FILE, 1);
    pthread_mutex_unlock(&mutexCurSen);
    
    removeFileInServer(nome_arquivo, client_info.sync_dir_path, client_info.server_port, client_info.server_ip);
    
    pthread_mutex_lock(&mutexCurSen);
    int j = getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo, "", REMOVED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksSended[j].executing = 0;
    }
    pthread_mutex_unlock(&mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer, 0);
    pthread_mutex_unlock(&mutexTasksToServer);
}

void renameFileServer(char *nome_arquivo_novo, char *nome_arquivo_antigo) {
    pthread_mutex_lock(&mutexCurSen);
    insertTaskToEnd(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    pthread_mutex_unlock(&mutexCurSen);
    
    updateFileName(nome_arquivo_novo, nome_arquivo_antigo, client_info.sync_dir_path, client_info.server_port, client_info.server_ip);
    
    pthread_mutex_lock(&mutexCurSen);
    int j = getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, nome_arquivo_novo, nome_arquivo_antigo, RENAMED_FILE, 1);
    if (j != -1) {
        currentOrRecentTasksSended[j].executing = 0;
    }
    pthread_mutex_unlock(&mutexCurSen);
    
    pthread_mutex_lock(&mutexTasksToServer);
    removeTaskAt(&pendingTasksToServer, &nTasksToServer, 0);
    pthread_mutex_unlock(&mutexTasksToServer);
}

// Thread para receber notificações do servidor
void* sync_receive_thread(void* arg) {
    notification_t notifications[300];
    
    while (client_info.running) {
        int min = 100000;
        int max = 250000;
        int aleatorio = min + rand() % (max - min + 1);
        usleep(aleatorio);
        
        pthread_mutex_lock(&mutex5);
        pthread_mutex_lock(&mutex);
        
        int num_notifications = receiveLastSecondNotificationFromServer(notifications, client_info.server_port, client_info.server_ip);
        
        insertNewTasks(notifications, num_notifications, &pendingTasks, &nTasks);
        
        pthread_mutex_lock(&mutexCurSen);
        removeDuplicateTasks(pendingTasks, &nTasks, currentOrRecentTasksSended, nCurrentOrRecentTasksSended);
        pthread_mutex_unlock(&mutexCurSen);
        
        // Filtra tarefas repetidas ou que se sobrepõem
        filterTasks(pendingTasks, &nTasks);
        filterTasksAux(pendingTasks, &nTasks);
        filterTasks(pendingTasks, &nTasks);
        
        pthread_mutex_unlock(&mutex);
        pthread_mutex_unlock(&mutex5);
    }
    
    return NULL;
}

// Thread para processar tarefas pendentes do servidor
void* sync_process_thread(void* arg) {
    while (client_info.running) {
        pthread_mutex_lock(&mutex5);
        
        if (nTasks != 0 && getIndex(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, 
                                   pendingTasks[0].notification.fileName, 
                                   pendingTasks[0].notification.ancientFileName,
                                   pendingTasks[0].notification.type, 0) == -1) {
            
            struct Task task = pendingTasks[0];
            
            switch (task.notification.type) {
                case UPDATED_FILE:
                    updateFile(task.notification.fileName);
                    break;
                case RENAMED_FILE:
                    renameFile(task.notification.fileName, task.notification.ancientFileName);
                    break;
                case REMOVED_FILE:
                    deleteFile(task.notification.fileName);
                    break;
                default:
                    break;
            }
        }
        
        pthread_mutex_unlock(&mutex5);
        usleep(100000);
    }
    
    return NULL;
}

// Thread para monitorar alterações locais
void* local_monitor_thread(void* arg) {
    while (client_info.running) {
        usleep(500000);
        
        notification_t notifications[300];
        int num_notifications = receiveLastSecondLocalNotification(notifications, client_info.sync_dir_path);
        
        pthread_mutex_lock(&mutexTasksToServer);
        
        insertNewTasks(notifications, num_notifications, &pendingTasksToServer, &nTasksToServer);
        
        pthread_mutex_lock(&mutex3);
        removeDuplicateTasks(pendingTasksToServer, &nTasksToServer, currentOrRecentTasksRecv, nCurrentOrRecentTasksRecv);
        pthread_mutex_unlock(&mutex3);
        
        filterTasks(pendingTasksToServer, &nTasksToServer);
        filterTasksAux(pendingTasksToServer, &nTasksToServer);
        filterTasks(pendingTasksToServer, &nTasksToServer);
        
        pthread_mutex_unlock(&mutexTasksToServer);
    }
    
    return NULL;
}

// Thread para enviar alterações locais ao servidor
void* local_send_thread(void* arg) {
    while (client_info.running) {
        usleep(100000);
        
        if (nTasksToServer != 0 && getIndex(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, 
                                           pendingTasksToServer[0].notification.fileName, 
                                           pendingTasksToServer[0].notification.ancientFileName,
                                           pendingTasksToServer[0].notification.type, 0) == -1) {
            
            struct Task task = pendingTasksToServer[0];
            
            switch (task.notification.type) {
                case UPDATED_FILE:
                    updateFileServer(task.notification.fileName);
                    break;
                case RENAMED_FILE:
                    renameFileServer(task.notification.fileName, task.notification.ancientFileName);
                    break;
                case REMOVED_FILE:
                    removeFileServer(task.notification.fileName);
                    break;
                default:
                    break;
            }
        }
    }
    
    return NULL;
}

// Thread para limpar tarefas antigas
void* cleanup_thread(void* arg) {
    while (client_info.running) {
        usleep(300000);
        
        time_t now = time(NULL);
        
        pthread_mutex_lock(&mutex3);
        for (int i = 0; i < nCurrentOrRecentTasksRecv; i++) {
            double diff = difftime(now, currentOrRecentTasksRecv[i].time);
            
            // Verificar se o tempo é maior que 1.5 segundos e executing é 0
            if (diff > 1.5 && currentOrRecentTasksRecv[i].executing == 0) {
                removeTaskAt(&currentOrRecentTasksRecv, &nCurrentOrRecentTasksRecv, i);
                i--;  // Ajusta o índice após a remoção
            }
        }
        pthread_mutex_unlock(&mutex3);
        
        pthread_mutex_lock(&mutexCurSen);
        for (int i = 0; i < nCurrentOrRecentTasksSended; i++) {
            double diff = difftime(now, currentOrRecentTasksSended[i].time);
            
            // Verificar se o tempo é maior que 1.5 segundos e executing é 0
            if (diff > 1.5 && currentOrRecentTasksSended[i].executing == 0) {
                removeTaskAt(&currentOrRecentTasksSended, &nCurrentOrRecentTasksSended, i);
                i--;  // Ajusta o índice após a remoção
            }
        }
        pthread_mutex_unlock(&mutexCurSen);
    }
    
    return NULL;
}

// Funções de interface com o usuário

// Função para listar arquivos no diretório do cliente
void list_client_files() {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[MAX_PATH_SIZE];
    
    printf("\n--- Arquivos no diretório do cliente ---\n");
    
    dir = opendir(client_info.sync_dir_path);
    if (!dir) {
        perror("Erro ao abrir diretório");
        return;
    }
    
    printf("%-30s %-20s %-20s %-20s\n", "Nome", "Modificação", "Acesso", "Criação/Alteração");
    printf("%-30s %-20s %-20s %-20s\n", "------------------------------", "--------------------", "--------------------", "--------------------");
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
            
        int written = snprintf(full_path, MAX_PATH_SIZE, "%s/%s", client_info.sync_dir_path, entry->d_name);
        if (written < 0 || written >= MAX_PATH_SIZE) {
            fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, entry->d_name);
            continue;
        }
        
        if (stat(full_path, &file_stat) == -1) {
            perror("Erro ao obter informações do arquivo");
            continue;
        }
        
        char mtime_str[20], atime_str[20], ctime_str[20];
        strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
        strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_atime));
        strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_ctime));
        
        printf("%-30s %-20s %-20s %-20s\n", entry->d_name, mtime_str, atime_str, ctime_str);
    }
    
    closedir(dir);
}

// Função para listar arquivos no servidor
void list_server_files() {
    printf("\n--- Solicitando lista de arquivos do servidor ---\n");
    
    char **arquivos_servidor = NULL;
    int num_arquivos = receiveFileListFromServer(&arquivos_servidor, client_info.server_port, client_info.server_ip);
    
    if (num_arquivos > 0) {
        printf("Arquivos no servidor (%d):\n", num_arquivos);
        for (int i = 0; i < num_arquivos; i++) {
            printf("  %s\n", arquivos_servidor[i]);
            free(arquivos_servidor[i]);
        }
        free(arquivos_servidor);
    } else if (num_arquivos == 0) {
        printf("Não há arquivos no servidor.\n");
    } else {
        printf("Erro ao listar arquivos do servidor.\n");
    }
}

// Função para fazer upload de um arquivo
void upload_file(const char *path) {
    printf("Iniciando upload do arquivo: %s\n", path);
    
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("Erro ao abrir arquivo para upload");
        return;
    }
    fclose(file);
    
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path; 
    }
    
    char dest_path[MAX_PATH_SIZE];
    int written = snprintf(dest_path, sizeof(dest_path), "%s/%s", client_info.sync_dir_path, filename);
    if (written < 0 || written >= sizeof(dest_path)) {
        fprintf(stderr, "Caminho muito longo: %s/%s\n", client_info.sync_dir_path, filename);
        return;
    }
    
    FILE *src = fopen(path, "rb");
    FILE *dest = fopen(dest_path, "wb");
    
    if (!src || !dest) {
        if (src) fclose(src);
        if (dest) fclose(dest);
        perror("Erro ao copiar arquivo para diretório de sincronização");
        return;
    }
    
    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    
    fclose(src);
    fclose(dest);
    
    printf("Arquivo %s copiado para o diretório de sincronização.\n", filename);
}

// Função para fazer download de um arquivo
void download_file(const char *filename) {
    printf("Iniciando download do arquivo: %s\n", filename);
    
    char current_dir[MAX_PATH_SIZE];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("Erro ao obter diretório atual");
        return;
    }
    
    int result = receiveNewFileFromServer((char*)filename, current_dir, client_info.server_port, client_info.server_ip);
    
    if (result == 0) {
        printf("Arquivo %s baixado com sucesso para o diretório atual: %s\n", 
               filename, current_dir);
    } else {
        printf("Erro ao baixar o arquivo %s.\n", filename);
    }
}

// Função para excluir um arquivo
void delete_file(const char *filename) {
    printf("Solicitando exclusão do arquivo: %s\n", filename);
    
    char sync_path[MAX_PATH_SIZE];
    int written = snprintf(sync_path, MAX_PATH_SIZE, "%s/%s", client_info.sync_dir_path, filename);
    if (written < 0 || written >= MAX_PATH_SIZE) {
        fprintf(stderr, "Caminho muito longo, arquivo não removido localmente: %s/%s\n", client_info.sync_dir_path, filename);
        return;
    }
    
    if (access(sync_path, F_OK) == 0) {
        if (remove(sync_path) == 0) {
            printf("Arquivo %s removido do diretório sync_dir.\n", filename);
        } else {
            perror("Erro ao remover arquivo do diretório sync_dir");
        }
    } else {
        printf("Arquivo %s não encontrado localmente.\n", filename);
    }
}

// Função para iniciar a sincronização
void get_sync_dir() {
    printf("Iniciando sincronização do diretório...\n");       
    
    // Verifica se o diretório existe
    if (check_and_create_directory(client_info.sync_dir_path) != 0) {
        printf("Erro ao preparar diretório de sincronização.\n");
        return;
    }
    
    // Obter os nomes dos arquivos que estão no servidor
    char **arquivosServidor;
    int num_arquivos = receiveFileListFromServer(&arquivosServidor, client_info.server_port, client_info.server_ip);
    
    if (num_arquivos < 0) {
        perror("Erro ao obter lista de arquivos do servidor");
        return;
    }
    
    // Inserir as tarefas no vetor pendingTasks (usando mutex)
    notification_t *notificacoes = malloc(num_arquivos * sizeof(notification_t));
    if (notificacoes == NULL) {
        perror("Erro ao alocar memória para notificações");
        pthread_mutex_unlock(&mutex);
        return;
    }
    
    for (int i = 0; i < num_arquivos; i++) {
        strcpy(notificacoes[i].fileName, arquivosServidor[i]);
        notificacoes[i].type = UPDATED_FILE;
        free(arquivosServidor[i]);
    }
    free(arquivosServidor);
    
    insertNewTasks(notificacoes, num_arquivos, &pendingTasks, &nTasks);
    free(notificacoes);
    
    printf("Diretório de sincronização pronto.\n");
}

// Função que processa os comandos do usuário
void process_command(char *command) {
    char cmd[MAX_COMMAND_SIZE];
    char arg[MAX_COMMAND_SIZE];

    arg[0] = '\0';
    
    if (sscanf(command, "%s %[^\n]", cmd, arg) < 1) {
        printf("Comando inválido.\n");
        return;
    }
    
    if (strcmp(cmd, "upload") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: upload <path/filename.ext>\n");
        } else {
            upload_file(arg);
        }
    } else if (strcmp(cmd, "download") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: download <filename.ext>\n");
        } else {
            download_file(arg);
        }
    } else if (strcmp(cmd, "delete") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: delete <filename.ext>\n");
        } else {
            delete_file(arg);
        }
    } else if (strcmp(cmd, "list_server") == 0) {
        list_server_files();
    } else if (strcmp(cmd, "list_client") == 0) {
        list_client_files();
    } else if (strcmp(cmd, "get_sync_dir") == 0) {
        get_sync_dir();
    } else if (strcmp(cmd, "exit") == 0) {
        printf("Encerrando sessão...\n");
        client_info.running = 0;
    } else {
        printf("Comando desconhecido: %s\n", cmd);
        printf("Comandos disponíveis:\n");
        printf("  upload <path/filename.ext>\n");
        printf("  download <filename.ext>\n");
        printf("  delete <filename.ext>\n");
        printf("  list_server\n");
        printf("  list_client\n");
        printf("  get_sync_dir\n");
        printf("  exit\n");
    }
}

// Thread para receber comandos do usuário
void *command_thread_function(void *arg) {
    char command[MAX_COMMAND_SIZE];
    
    printf("\n=== Cliente Dropbox ===\n");
    printf("Digite 'help' para ver os comandos disponíveis.\n");
    
    while (client_info.running) {
        printf("\n> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = '\0';
        
        if (strlen(command) > 0) {
            if (strcmp(command, "help") == 0) {
                printf("Comandos disponíveis:\n");
                printf("  upload <path/filename.ext> - Envia um arquivo para o servidor\n");
                printf("  download <filename.ext> - Baixa um arquivo do servidor\n");
                printf("  delete <filename.ext> - Exclui um arquivo do servidor\n");
                printf("  list_server - Lista os arquivos no servidor\n");
                printf("  list_client - Lista os arquivos no diretório local\n");
                printf("  get_sync_dir - Inicia a sincronização\n");
                printf("  exit - Encerra a sessão\n");
            } else {
                process_command(command);
            }
        }
    }
    
    printf("Thread de comandos encerrada.\n");
    return NULL;
}   

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <username> <server_ip> <port>\n", argv[0]);
        return 1;
    }
    
    // Inicializa as informações do cliente
    strncpy(client_info.username, argv[1], sizeof(client_info.username));
    strncpy(client_info.server_ip, argv[2], sizeof(client_info.server_ip));
    client_info.server_port = atoi(argv[3]);
    client_info.running = 1;
    
    // Configura o diretório do cliente
    if (setup_client_directory(client_info.username) != 0) {
        fprintf(stderr, "Erro ao configurar diretório do cliente.\n");
        return 1;
    }
    
    // Inicializa os vetores de tarefas
    pendingTasks = malloc(sizeof(struct Task));
    pendingTasksToServer = malloc(sizeof(struct Task));
    currentOrRecentTasksRecv = malloc(sizeof(struct Task));
    currentOrRecentTasksSended = malloc(sizeof(struct Task));
    
    if (!pendingTasks || !pendingTasksToServer || !currentOrRecentTasksRecv || !currentOrRecentTasksSended) {
        perror("Erro ao alocar memória para vetores de tarefas");
        return 1;
    }
    
    // Inicializa o diretório de sincronização
    get_sync_dir();
    
    // Cria as threads
    pthread_t command_thread, sync_receive_thread_id, sync_process_thread_id;
    pthread_t local_monitor_thread_id, local_send_thread_id, cleanup_thread_id;
    
    // Thread para receber comandos do usuário
    if (pthread_create(&command_thread, NULL, command_thread_function, NULL) != 0) {
        perror("Erro ao criar thread de comandos");
        return 1;
    }
    
    // Thread para receber notificações do servidor
    if (pthread_create(&sync_receive_thread_id, NULL, sync_receive_thread, NULL) != 0) {
        perror("Erro ao criar thread de recebimento de notificações");
        return 1;
    }
    
    // Thread para processar tarefas pendentes do servidor
    if (pthread_create(&sync_process_thread_id, NULL, sync_process_thread, NULL) != 0) {
        perror("Erro ao criar thread de processamento de tarefas");
        return 1;
    }
    
    // Thread para monitorar alterações locais
    if (pthread_create(&local_monitor_thread_id, NULL, local_monitor_thread, NULL) != 0) {
        perror("Erro ao criar thread de monitoramento local");
        return 1;
    }
    
    // Thread para enviar alterações locais ao servidor
    if (pthread_create(&local_send_thread_id, NULL, local_send_thread, NULL) != 0) {
        perror("Erro ao criar thread de envio local");
        return 1;
    }
    
    // Thread para limpar tarefas antigas
    if (pthread_create(&cleanup_thread_id, NULL, cleanup_thread, NULL) != 0) {
        perror("Erro ao criar thread de limpeza");
        return 1;
    }
    
    // Aguarda a thread de comandos terminar
    pthread_join(command_thread, NULL);
    
    // Encerra as outras threads
    client_info.running = 0;
    pthread_join(sync_receive_thread_id, NULL);
    pthread_join(sync_process_thread_id, NULL);
    pthread_join(local_monitor_thread_id, NULL);
    pthread_join(local_send_thread_id, NULL);
    pthread_join(cleanup_thread_id, NULL);
    
    // Libera a memória alocada
    free(pendingTasks);
    free(pendingTasksToServer);
    free(currentOrRecentTasksRecv);
    free(currentOrRecentTasksSended);
    
    printf("Cliente encerrado.\n");
    return 0;
}
