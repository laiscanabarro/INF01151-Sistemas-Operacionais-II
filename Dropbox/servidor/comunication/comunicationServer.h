#ifndef COMUNICATION_SERVER_H
#define COMUNICATION_SERVER_H

#include "../../common.h" 

#define MAX_CLIENTS 100
typedef enum {
    SERVER,      
    CLIENT,    
} operation_destiny_t;

typedef struct {
    notification_t notification;
    operation_destiny_t destiny;
    int serverSock;
} operation_t;

// Estrutura para armazenar informações sobre os clientes conectados
typedef struct {
    client clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t mutex;
} client_list;

// Declaração de variáveis globais 
extern int nOperations;
extern operation_t actualOperations[100];

// Declaração das funções
int receiveNewFileFromClient(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int removeFileInServer(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int updateFileName(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int sendNewFileToClient(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int sendLastSecondNotificationToClient(int novo_socket, char *diretorio);
int receiveLastSecondLocalNotification(notification_t *notification, char *diretorio);
int sendFileListToClient(int novo_socket, char *diretorio);
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos);
void filterNotifications(notification_t *notifications, int *num_notifications);
int get_connected_devices(const char* username);

#endif // COMUNICATION_SERVER_H
