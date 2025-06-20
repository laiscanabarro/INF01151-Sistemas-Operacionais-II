#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>

#define MAX_PATH_SIZE 500

// Tipos de notificação para facilitar o uso e a comparação 
typedef enum {
    UPDATED_FILE,      // Arquivo atualizado ou novo arquivo
    RENAMED_FILE,      // Arquivo renomeado
    REMOVED_FILE       // Arquivo removido
} notification_type_t;

// Estrutura para representar uma notificação de alteração de arquivo 
typedef struct {
    char fileName[200];            // Nome do arquivo
    char ancientFileName[200];     // Nome anterior, se houve mudança 
    notification_type_t type;      // Tipo da notificação
} notification_t;

// Estrutura de cliente (usada no servidor, antes estava em server.h)
typedef struct client {
    int socket_fd;          // Descritor do socket
    char username[50];      // Nome do usuário
    pthread_t thread_id;    // ID da thread do cliente
    int device_id;          // ID do dispositivo (1 ou 2)
    char sync_dir_path[MAX_PATH_SIZE]; // Caminho base do diretório de sincronização do usuário no servidor
    int running;          // Flag para indicar se o cliente está ativo
    char server_ip[16];
    int server_port;
} client;

#endif // COMMON_H
