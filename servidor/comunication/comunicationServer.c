#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#include "comunicationServer.h"
#define MAX_OPERATIONS 300
#define MAX_CLIENTES 200

int nOperations = 0;


operationEntry_t actualOperations[MAX_OPERATIONS];

// Verifica se o arquivo está em operação conflitante para o cliente específico
int isFileInClientServerConflitOperation(char *file_name, notification_type_t type, operation_destiny_t destiny, const char *clientName) {
    for (int i = 0; i < nOperations; i++) {
        if (strcmp(actualOperations[i].operation.notification.fileName, file_name) == 0 &&
            actualOperations[i].operation.notification.type == UPDATED_FILE &&
            (actualOperations[i].operation.destiny == SERVER || destiny != CLIENT) &&
            strcmp(actualOperations[i].clientName, clientName) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int getIndex(char *file_name, notification_type_t type, operation_destiny_t destiny, int sock, const char *clientName) {
    for (int i = 0; i < nOperations; i++) {
        if (strcmp(actualOperations[i].operation.notification.fileName, file_name) == 0 &&
            actualOperations[i].operation.notification.type == type &&
            actualOperations[i].operation.destiny == destiny &&
            actualOperations[i].operation.serverSock == sock &&
            strcmp(actualOperations[i].clientName, clientName) == 0)
        {
            return i;
        }
    }
    return -1;
}

int insertOperation(char *file_name, notification_type_t type, operation_destiny_t destiny, int sock, const char *clientName) {
    if (nOperations < MAX_OPERATIONS) {
        strncpy(actualOperations[nOperations].operation.notification.fileName, file_name, sizeof(actualOperations[nOperations].operation.notification.fileName));
        actualOperations[nOperations].operation.notification.type = type;
        actualOperations[nOperations].operation.destiny = destiny;
        actualOperations[nOperations].operation.serverSock = sock;
        strncpy(actualOperations[nOperations].clientName, clientName, sizeof(actualOperations[nOperations].clientName));
        nOperations++;
        return nOperations - 1;
    }
    return -1;
}

int removeOperation(char *file_name, notification_type_t type, operation_destiny_t destiny, int sock, const char *clientName) {
    int ind = getIndex(file_name, type, destiny, sock, clientName);
    if (ind == -1) {
        return -1;  // Operação não encontrada
    }
    for (int i = ind + 1; i < nOperations; i++) {
        actualOperations[i - 1] = actualOperations[i];
    }
    nOperations--;
    return 0;  // Sucesso
}

int receiveNewFileFromClient(int novo_socket,char *diretorio,pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente){
    
    int tamanho_nome;
    char nome_arquivo[1024];
    char buffer[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';
    // Verifica se a conexão foi encerrada
    if (strcmp(nome_arquivo, "0") == 0) {
        printf("Conexão encerrada pelo cliente.\n");
        return 1;
    }
  
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_arquivo,UPDATED_FILE,SERVER,nome_cliente)){
            wait=0;
           insertOperation(nome_arquivo,UPDATED_FILE,SERVER,novo_socket,nome_cliente);

         }
        pthread_mutex_unlock(conflitOperations);

    }
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    // Recebe o tamanho do arquivo
    long long tamanho_arquivo;
    recv(novo_socket, &tamanho_arquivo, sizeof(long long), 0);
    FILE *arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo");
        return 1;
    }
    // Recebe e grava o conteúdo do arquivo
    long long bytes_recebidos = 0;
    while (bytes_recebidos < tamanho_arquivo) {
        int bytes = recv(novo_socket, buffer, 1024, 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, arquivo);
        bytes_recebidos += bytes;
    }
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_arquivo,UPDATED_FILE,SERVER,novo_socket,nome_cliente);
    pthread_mutex_unlock(conflitOperations);
    fclose(arquivo);

    replicateSendNewFileToBackups(servers,nServers, nome_arquivo,diretorio,8080,nome_cliente,IP_cliente);
    int waitBackup=0;
    send(novo_socket, &waitBackup, sizeof(int), 0);
    close(novo_socket);



}

int removeFileInServer(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente) {
    
    int tamanho_nome;
    char nome_arquivo[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';
    
   
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_arquivo,REMOVED_FILE,SERVER,nome_cliente)){
            wait=0;
          insertOperation(nome_arquivo,REMOVED_FILE,SERVER,novo_socket,nome_cliente);

         }
        pthread_mutex_unlock(conflitOperations);

    }
    
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

    // Tenta remover o arquivo
    if (remove(caminho_completo) == 0) {
        printf("Arquivo '%s' removido com sucesso!\n", nome_arquivo);
    } else {
        perror("Erro ao remover o arquivo");
        return 1;
    }
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_arquivo,REMOVED_FILE,SERVER,novo_socket,nome_cliente);
    pthread_mutex_unlock(conflitOperations);
    replicateRemoveFileOnBackups(servers,nServers, nome_arquivo,diretorio,8080,nome_cliente,IP_cliente);
    int waitBackup=0;
    send(novo_socket, &waitBackup, sizeof(int), 0);
    close(novo_socket);
    return 0;
}

int updateFileName(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente) {
    int tamanho_nome;
    char nome_antigo[1024];
    char nome_novo[1024];

    // Recebe o nome antigo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_antigo, tamanho_nome, 0);
    nome_antigo[tamanho_nome] = '\0';
    
   
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_antigo,RENAMED_FILE,SERVER,nome_cliente)){
            wait=0;
           insertOperation(nome_antigo,RENAMED_FILE,SERVER,novo_socket,nome_cliente);

         }
        pthread_mutex_unlock(conflitOperations);

    }
    // Caminho completo antigo
    char caminho_completo_antigo[1024 * 2];
    snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", diretorio, nome_antigo);

    // Recebe o nome novo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_novo, tamanho_nome, 0);
    nome_novo[tamanho_nome] = '\0';

    // Caminho completo novo
    char caminho_completo_novo[1024 * 2];
    snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", diretorio, nome_novo);  // Corrigido

    // Renomeia o arquivo
    if (rename(caminho_completo_antigo, caminho_completo_novo) != 0) {
        perror("Erro ao renomear arquivo");
        return 1;
    } else {
        printf("Arquivo '%s' renomeado para '%s' com sucesso!\n", nome_antigo, nome_novo);
    }
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_antigo,RENAMED_FILE,SERVER,novo_socket,nome_cliente);
    pthread_mutex_unlock(conflitOperations);
    replicateUpdateFileNameOnBackups(servers,nServers, nome_novo,nome_antigo,diretorio,8080,nome_cliente,IP_cliente);
    int waitBackup=0;
    send(novo_socket, &waitBackup, sizeof(int), 0);
    close(novo_socket);
    return 0;
}

int sendNewFileToClient(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations,char *nome_cliente) {
    int tamanho_nome;
    char nome_arquivo[1024];
    char buffer[1024];
    // Recebe o nome do arquivo solicitado
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';


    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_arquivo,UPDATED_FILE,CLIENT,nome_cliente)){
            wait=0;
            insertOperation(nome_arquivo,UPDATED_FILE,CLIENT,novo_socket,nome_cliente);

         }
        pthread_mutex_unlock(conflitOperations);

    }
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

    FILE *arquivo = fopen(caminho_completo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    // Tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long long tamanho_arquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    send(novo_socket, &tamanho_arquivo, sizeof(long long), 0);
  
    // Envia o conteúdo
    int bytes_lidos;
    while ((bytes_lidos = fread(buffer, 1, 1024, arquivo)) > 0) {
        send(novo_socket, buffer, bytes_lidos, 0);
    }
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_arquivo,UPDATED_FILE,CLIENT,novo_socket,nome_cliente);
    pthread_mutex_unlock(conflitOperations);
    fclose(arquivo);
    close(novo_socket);
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


#define MAX_NOTIFICATIONS 1024
#define MAX_CLIENTS 200  // número máximo de clientes simultâneos

// Agora a função recebe também o índice do cliente
int sendLastSecondNotificationToClient(int novo_socket, int ind_cliente, char *diretorio) {
    notification_t notifications[MAX_NOTIFICATIONS];
    int num_notifications = receiveLastSecondLocalNotification(ind_cliente, notifications, diretorio);

    // Verifica se houve erro ao obter notificações
    if (num_notifications < 0) {
        perror("Erro ao obter notificações do último segundo");
        close(novo_socket);
        return 1;
    }

    // Enviar o número de notificações para o cliente
    if (send(novo_socket, &num_notifications, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar número de notificações");
        close(novo_socket);
        return 1;
    }

    // Enviar cada notificação para o cliente
    for (int i = 0; i < num_notifications; i++) {
        if (send(novo_socket, &notifications[i], sizeof(notification_t), 0) <= 0) {
            perror("Erro ao enviar notificação");
            close(novo_socket);
            return 1;
        }
    }

    close(novo_socket);
    return 0;
}

// Função para receber notificações locais, diferenciadas por cliente via ind_cliente
int receiveLastSecondLocalNotification(int ind_cliente, notification_t *notifications, char *diretorio) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    time_t now = time(NULL);

    // Variáveis estáticas por cliente para armazenar estado anterior
    static struct stat previous_stats[MAX_CLIENTS][1024];
    static char previous_names[MAX_CLIENTS][1024][200];
    static int previous_count[MAX_CLIENTS] = {0};
    static int initialized[MAX_CLIENTS] = {0};

    // Cache das notificações do último segundo por cliente
    static notification_t last_notifications[MAX_CLIENTS][300];
    static int last_notification_count[MAX_CLIENTS] = {0};
    static time_t last_second[MAX_CLIENTS] = {0};

    // Verificar se estamos no mesmo segundo da última chamada
    if (now == last_second[ind_cliente]) {
        memcpy(notifications,
               last_notifications[ind_cliente],
               last_notification_count[ind_cliente] * sizeof(notification_t));
        return last_notification_count[ind_cliente];
    }

    // Atualizar o segundo atual e resetar contagem
    last_second[ind_cliente] = now;
    last_notification_count[ind_cliente] = 0;

    // Tabelas para marcar arquivos já encontrados na iteração atual
    int found_in_iteration[1024] = {0};

    // Abrir o diretório para leitura
    dir = opendir(diretorio);
    if (!dir) {
        perror("Erro ao abrir o diretório");
        return -1;
    }

    // Iterar sobre os arquivos do diretório
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);
        if (stat(full_path, &file_stat) == -1) continue;

        int is_new = 1;
        for (int i = 0; i < previous_count[ind_cliente]; i++) {
            if (file_stat.st_ino == previous_stats[ind_cliente][i].st_ino) {
                is_new = 0;
                found_in_iteration[i] = 1;

                // renomeação
                if (strcmp(previous_names[ind_cliente][i], entry->d_name) != 0) {
                    strncpy(last_notifications[ind_cliente][last_notification_count[ind_cliente]].fileName,
                            entry->d_name,
                            sizeof(last_notifications[ind_cliente][0].fileName));
                    strncpy(last_notifications[ind_cliente][last_notification_count[ind_cliente]].ancientFileName,
                            previous_names[ind_cliente][i],
                            sizeof(last_notifications[ind_cliente][0].ancientFileName));
                    last_notifications[ind_cliente][last_notification_count[ind_cliente]].type = RENAMED_FILE;
                    last_notification_count[ind_cliente]++;
                    strncpy(previous_names[ind_cliente][i], entry->d_name, sizeof(previous_names[ind_cliente][i]));
                }

                // atualização
                if (difftime(now, file_stat.st_mtime) <= 1) {
                    strncpy(last_notifications[ind_cliente][last_notification_count[ind_cliente]].fileName,
                            entry->d_name,
                            sizeof(last_notifications[ind_cliente][0].fileName));
                    last_notifications[ind_cliente][last_notification_count[ind_cliente]].type = UPDATED_FILE;
                    last_notification_count[ind_cliente]++;
                }
                break;
            }
        }

        // novo arquivo
        if (is_new) {
            strncpy(last_notifications[ind_cliente][last_notification_count[ind_cliente]].fileName,
                    entry->d_name,
                    sizeof(last_notifications[ind_cliente][0].fileName));
            last_notifications[ind_cliente][last_notification_count[ind_cliente]].type = UPDATED_FILE;
            last_notification_count[ind_cliente]++;
        }
    }
    closedir(dir);

    // remoções
    for (int i = 0; i < previous_count[ind_cliente]; i++) {
        if (!found_in_iteration[i]) {
            strncpy(last_notifications[ind_cliente][last_notification_count[ind_cliente]].fileName,
                    previous_names[ind_cliente][i],
                    sizeof(last_notifications[ind_cliente][0].fileName));
            last_notifications[ind_cliente][last_notification_count[ind_cliente]].type = REMOVED_FILE;
            last_notification_count[ind_cliente]++;
        }
    }

    // atualizar histórico
    dir = opendir(diretorio);
    previous_count[ind_cliente] = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);
        if (stat(full_path, &file_stat) == -1) continue;
        previous_stats[ind_cliente][previous_count[ind_cliente]] = file_stat;
        strncpy(previous_names[ind_cliente][previous_count[ind_cliente]],
                entry->d_name,
                sizeof(previous_names[ind_cliente][0]));
        previous_count[ind_cliente]++;
    }
    closedir(dir);

    // primeira chamada retorna 0
    if (!initialized[ind_cliente]) {
        initialized[ind_cliente] = 1;
        return 0;
    }

    filterNotifications(last_notifications[ind_cliente], &last_notification_count[ind_cliente]);
    memcpy(notifications,
           last_notifications[ind_cliente],
           last_notification_count[ind_cliente] * sizeof(notification_t));
    return last_notification_count[ind_cliente];
}

void filterNotifications(notification_t *notifications, int *num_notifications) {
    for (int i = 0; i < *num_notifications; i++) {
        if (notifications[i].type == UPDATED_FILE) {
            for (int j = i + 1; j < *num_notifications; j++) {
                if (strcmp(notifications[i].fileName, notifications[j].fileName) == 0) {
                    for (int k = j; k < *num_notifications - 1; k++) {
                        notifications[k] = notifications[k + 1];
                    }
                    (*num_notifications)--;
                    j--; 
                }
            }
        }
    }
}




int sendFileListToClient(int novo_socket, char *diretorio){
    int nArquivos=0;
    char **arquivos;
    // Chamada da função para obter a lista de arquivos
    obterListaArquivos(diretorio, &arquivos, &nArquivos);

    // Verificar se a lista foi preenchida corretamente
    if (arquivos == NULL) {
        printf("Erro ao obter a lista de arquivos\n");
        return -1;
    }
    if (send(novo_socket, &nArquivos, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar numero de arquivos");
        close(novo_socket);
        return 1;
    }
    for (int i = 0; i < nArquivos; i++)
    {
        int len = strlen(arquivos[i]) + 1; // +1 para incluir o '\0'

        // Envia o tamanho do nome do arquivo
        if (send(novo_socket, &len, sizeof(int), 0) <= 0)
        {
            perror("Erro ao enviar tamanho do arquivo");
            close(novo_socket);
            return 1;
        }

        // Envia o nome do arquivo
        if (send(novo_socket, arquivos[i], len, 0) <= 0)
        {
            perror("Erro ao enviar nome do arquivo");
            close(novo_socket);
            return 1;
        }
    }
    for (int i = 0; i < nArquivos; i++)
    {
        free(arquivos[i]);
    }
    free(arquivos);
}
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos) {
    DIR *dir;
    struct dirent *entry;
    int capacidade = 100;  // Capacidade inicial

    // Alocar memória para a lista de arquivos
    *arquivos = malloc(capacidade * sizeof(char *));
    if (*arquivos == NULL) {
        perror("Erro ao alocar memória para a lista de arquivos");
        return;
    }

    *nArquivos = 0;

    // Abrir o diretório especificado
    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretório");
        free(*arquivos);
        *arquivos = NULL;
        return;
    }

    // Percorrer os arquivos no diretório
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Realocar memória se necessário
        if (*nArquivos >= capacidade) {
            capacidade *= 2;
            char **temp = realloc(*arquivos, capacidade * sizeof(char *));
            if (temp == NULL) {
                perror("Erro ao realocar memória para a lista de arquivos");
                closedir(dir);
                free(*arquivos);
                *arquivos = NULL;
                return;
            }
            *arquivos = temp;
        }

        // Alocar memória para o nome do arquivo e armazenar
        (*arquivos)[*nArquivos] = strdup(entry->d_name);
        if ((*arquivos)[*nArquivos] == NULL) {
            perror("Erro ao alocar memória para o nome do arquivo");
            closedir(dir);
            free(*arquivos);
            *arquivos = NULL;
            return;
        }

        (*nArquivos)++;
    }
    closedir(dir);
}
void replicateConnecitonOnBackups(server_t servers[], int nServers, char *nome_cliente, char *IP_cliente) {
   

    for (int i = 0; i < nServers; i++) {
        if (servers[i].isPrimary) continue; // pula o próprio primário

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Erro ao criar socket para backup");
            continue;
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);

        if (inet_pton(AF_INET, servers[i].IP, &addr.sin_addr) <= 0) {
            fprintf(stderr, "IP de backup inválido: %s\n", servers[i].IP);
            close(sockfd);
            continue;
        }

        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "Erro ao conectar ao backup %s\n", servers[i].IP);
            close(sockfd);
            continue;
        }

        // Envia dados no mesmo formato que o cliente envia ao primário
        int codigo = 0;
        int tamanho_nome_cliente = strlen(nome_cliente);
        int tamanho_nome_IP = strlen(IP_cliente);

        send(sockfd, &codigo, sizeof(int), 0);
        send(sockfd, &tamanho_nome_cliente, sizeof(int), 0);
        send(sockfd, nome_cliente, tamanho_nome_cliente, 0);
        send(sockfd, &tamanho_nome_IP, sizeof(int), 0);
        send(sockfd, IP_cliente, tamanho_nome_IP, 0);

        close(sockfd);
    }
}

void replicateSendNewFileToBackups(server_t servers[], int nServers, char *fileName, char *directory, int port, char *nome_cliente, char *IP_cliente) {
   

    for (int i = 0; i < nServers; i++) {
        if (servers[i].isPrimary) continue; // pula o próprio primário
        sendNewFileToServerBackups(fileName,directory,port, servers[i].IP,nome_cliente,IP_cliente);
    }
}

// -----------------------------------------------------------------------------
// 2) Replica “Remove File”
// -----------------------------------------------------------------------------
void replicateRemoveFileOnBackups(server_t servers[], int nServers,
    char *fileName, char *directory,
    int port, char *clientName, char *clientIP)
{
    for (int i = 0; i < nServers; i++) {
        if (servers[i].isPrimary) continue;
        removeFileInServerBackups((char*)fileName,
                                  (char*)directory,
                                  port,
                                  servers[i].IP,
                                  (char*)clientName,
                                  (char*)clientIP);
    }
}

// -----------------------------------------------------------------------------
// 3) Replica “Update File Name”
// -----------------------------------------------------------------------------
void replicateUpdateFileNameOnBackups(server_t servers[], int nServers,
   char *newName, char *oldName, char *directory,
    int port, char *clientName, char *clientIP)
{
    for (int i = 0; i < nServers; i++) {
        if (servers[i].isPrimary) continue;
        updateFileNameInServerBakcups((char*)newName,
                                      (char*)oldName,
                                      (char*)directory,
                                      port,
                                      servers[i].IP,
                                      (char*)clientName,
                                      (char*)clientIP);
    }
}
int sendNewFileToServerBackups(char *nomeArquivo,char *diretorio,int PORTA,char * IP,char *nome_cliente,char *IP_cliente){
    int sock;
    struct sockaddr_in endereco;
    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);
    // Caminho completo
    char caminho_completo[1024*2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nomeArquivo);
    char buffer[1024];
    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo=1;
    send(sock,&codigo,sizeof(int),0);
    // Envia o nome do cliente
    int tamanho_nome_cliente= strlen(nome_cliente);
   
    send(sock, &tamanho_nome_cliente, sizeof(int), 0);
    send(sock, nome_cliente, tamanho_nome_cliente, 0);
    // Envia o ip do cliente
    int tamanho_nome_IP= strlen(IP_cliente);
    send(sock, &tamanho_nome_IP, sizeof(int), 0);
    send(sock, IP_cliente, tamanho_nome_IP, 0);
    
    
    // printf("Conectado ao servidor!\n");
    
    FILE *arquivo = fopen(caminho_completo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo");
        return 1;
    }
     
    // Envia o nome do arquivo
    int tamanho_nome = strlen(nomeArquivo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeArquivo, tamanho_nome, 0);
    
    // Tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long long tamanho_arquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    send(sock, &tamanho_arquivo, sizeof(long long), 0);
  
    // Envia o conteúdo
    int bytes_lidos;
    while ((bytes_lidos = fread(buffer, 1, 1024, arquivo)) > 0) {
        send(sock, buffer, bytes_lidos, 0);
    }

    fclose(arquivo);
    close(sock);
    
    return 0;
}


int removeFileInServerBackups(char *nomeArquivo,char *diretorio,int PORTA,char * IP,char *nome_cliente,char *IP_cliente){
    int sock;
    struct sockaddr_in endereco;
    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);
    // Caminho completo
    char caminho_completo[1024*2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nomeArquivo);
    char buffer[1024];
    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo=2;
    send(sock,&codigo,sizeof(int),0);
     // Envia o nome do cliente
    int tamanho_nome_cliente= strlen(nome_cliente);
    
    send(sock, &tamanho_nome_cliente, sizeof(int), 0);
    send(sock, nome_cliente, tamanho_nome_cliente, 0);
    // Envia o ip do cliente
    int tamanho_nome_IP= strlen(IP_cliente);
    send(sock, &tamanho_nome_IP, sizeof(int), 0);
    send(sock, IP_cliente, tamanho_nome_IP, 0);
    
   // printf("Conectado ao servidor!\n");

    // Envia o nome do arquivo
    int tamanho_nome = strlen(nomeArquivo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeArquivo, tamanho_nome, 0);
    close(sock);
    
    return 0;
}

int updateFileNameInServerBakcups(char *nomeNovo, char *nomeAntigo, char *diretorio, int PORTA, char *IP,char *nome_cliente,char *IP_cliente) {
    int sock;
    struct sockaddr_in endereco;

    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);

    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo = 3;  // Novo código para "renomear arquivo"
    send(sock, &codigo, sizeof(int), 0);
     // Envia o nome do cliente
    int tamanho_nome_cliente= strlen(nome_cliente);
    
    send(sock, &tamanho_nome_cliente, sizeof(int), 0);
    send(sock, nome_cliente, tamanho_nome_cliente, 0);
    // Envia o ip do cliente
    int tamanho_nome_IP= strlen(IP_cliente);
    send(sock, &tamanho_nome_IP, sizeof(int), 0);
    send(sock, IP_cliente, tamanho_nome_IP, 0);
    
   // printf("Conectado ao servidor!\n");

    // Envia o nome antigo do arquivo
    int tamanho_nome = strlen(nomeAntigo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeAntigo, tamanho_nome, 0);

    // Envia o nome novo do arquivo
    tamanho_nome = strlen(nomeNovo);  // Corrigido
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeNovo, tamanho_nome, 0);

    close(sock);
    return 0;
}




int receiveNewFileFromPrimary(int novo_socket,char *diretorio,pthread_mutex_t *conflitOperations){
    
    int tamanho_nome;
    char nome_arquivo[1024];
    char buffer[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';
    // Verifica se a conexão foi encerrada
    if (strcmp(nome_arquivo, "0") == 0) {
        printf("Conexão encerrada pelo cliente.\n");
        return 1;
    }
    /*
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_arquivo,UPDATED_FILE,SERVER)){
            wait=0;
           insertOperation(nome_arquivo,UPDATED_FILE,SERVER,novo_socket);

         }
        pthread_mutex_unlock(conflitOperations);

    }*/
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    // Recebe o tamanho do arquivo
    long long tamanho_arquivo;
    recv(novo_socket, &tamanho_arquivo, sizeof(long long), 0);
    FILE *arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo");
        return 1;
    }
    // Recebe e grava o conteúdo do arquivo
    long long bytes_recebidos = 0;
    while (bytes_recebidos < tamanho_arquivo) {
        int bytes = recv(novo_socket, buffer, 1024, 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, arquivo);
        bytes_recebidos += bytes;
    }
    /*
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_arquivo,UPDATED_FILE,SERVER,novo_socket);
    pthread_mutex_unlock(conflitOperations);
    */
    fclose(arquivo);
    close(novo_socket);



}

int removeFileInServerBackup(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations) {
    
    int tamanho_nome;
    char nome_arquivo[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';
    
    /*
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_arquivo,REMOVED_FILE,SERVER)){
            wait=0;
          insertOperation(nome_arquivo,REMOVED_FILE,SERVER,novo_socket);

         }
        pthread_mutex_unlock(conflitOperations);

    }*/
    
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

    // Tenta remover o arquivo
    if (remove(caminho_completo) == 0) {
        printf("Arquivo '%s' removido com sucesso!\n", nome_arquivo);
    } else {
        perror("Erro ao remover o arquivo");
        return 1;
    }
    /*
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_arquivo,REMOVED_FILE,SERVER,novo_socket);
    pthread_mutex_unlock(conflitOperations);
    */
    close(novo_socket);
    return 0;
}

int updateFileNameInBackup(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations) {
    int tamanho_nome;
    char nome_antigo[1024];
    char nome_novo[1024];

    // Recebe o nome antigo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_antigo, tamanho_nome, 0);
    nome_antigo[tamanho_nome] = '\0';
    
    /*
    int wait=1;
    while(wait){
        usleep(100000);
        pthread_mutex_lock(conflitOperations);
        if(!isFileInClientServerConflitOperation(nome_antigo,RENAMED_FILE,SERVER)){
            wait=0;
           insertOperation(nome_antigo,RENAMED_FILE,SERVER,novo_socket);

         }
        pthread_mutex_unlock(conflitOperations);

    }*/
    // Caminho completo antigo
    char caminho_completo_antigo[1024 * 2];
    snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", diretorio, nome_antigo);

    // Recebe o nome novo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_novo, tamanho_nome, 0);
    nome_novo[tamanho_nome] = '\0';

    // Caminho completo novo
    char caminho_completo_novo[1024 * 2];
    snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", diretorio, nome_novo);  // Corrigido

    // Renomeia o arquivo
    if (rename(caminho_completo_antigo, caminho_completo_novo) != 0) {
        perror("Erro ao renomear arquivo");
        return 1;
    } else {
        printf("Arquivo '%s' renomeado para '%s' com sucesso!\n", nome_antigo, nome_novo);
    }
    /*
    pthread_mutex_lock(conflitOperations);
    removeOperation(nome_antigo,RENAMED_FILE,SERVER,novo_socket);
    pthread_mutex_unlock(conflitOperations);
    */
    close(novo_socket);
    return 0;
}











