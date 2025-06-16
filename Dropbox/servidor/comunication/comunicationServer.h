#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#define MAX_FILEPATH 512
#define MAX_CLIENTS 100

typedef enum {
    SERVER,      
    CLIENT,    
} operation_destiny_t;

// Tipos de notificação para facilitar o uso e a comparação
typedef enum {
    UPDATED_FILE,      // Arquivo atualizado
    RENAMED_FILE,      // Arquivo renomeado
    REMOVED_FILE       // Arquivo removido
} notification_type_t;

// Estrutura para representar uma notificação de alteração de arquivo
typedef struct {
    char fileName[200];            // Nome do arquivo
    char ancientFileName[200];     // Nome anterior, se houve mudança
    notification_type_t type;      // Tipo da notificação
} notification_t;

typedef struct {
    notification_t notification;
    operation_destiny_t destiny;
    int serverSock;
} operation_t;

// Estrutura de cliente
typedef struct client {
    int socket_fd;          // Descritor do socket
    char username[50];      // Nome do usuário
    pthread_t thread_id;    // ID da thread do cliente
    int device_id;          // ID do dispositivo (1 ou 2)
    char sync_dir[MAX_FILEPATH]; // Caminho do diretório de sincronização
    int is_active;          // Flag para indicar se o cliente está ativo
} client;

typedef struct {
    client clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t mutex;
} client_list;

// Declaração de variáveis globais (uso de extern)
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
