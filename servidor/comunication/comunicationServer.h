#include <sys/stat.h>
#include <time.h>
#include <pthread.h>

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



typedef struct{
    char IP [16];
    int isPrimary; //1 indica que é primario
    
}server_t;

typedef struct {
    notification_t notification;
    operation_destiny_t destiny;
    int serverSock;
} operation_t;

typedef struct {
    char clientName[100];
    operation_t operation;
} operationEntry_t;

// Declaração de variáveis globais (uso de extern)
#define MAX_OPERATIONS 300
#define MAX_CLIENTES 200

extern int nOperations;
extern operationEntry_t actualOperations[MAX_OPERATIONS];


// Declaração das funções
int receiveNewFileFromClient(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente);
int removeFileInServer(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente);
int updateFileName(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,server_t servers [],int nServers,char *nome_cliente,char *IP_cliente);

int sendNewFileToClient(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations,char *nome_cliente);
int sendLastSecondNotificationToClient(int novo_socket, int ind_cliente, char *diretorio);
int receiveLastSecondLocalNotification(int ind_cliente, notification_t *notifications, char *diretorio);
int sendFileListToClient(int novo_socket, char *diretorio);
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos);
void filterNotifications(notification_t *notifications, int *num_notifications);

void replicateConnecitonOnBackups(server_t servers [],int nServers,char *nome_cliente,char *IP_cliente);

void replicateSendNewFileToBackups(server_t servers[], int nServers, char *fileName, char *directory, int port, char *nome_cliente, char *IP_cliente);
void replicateRemoveFileOnBackups(server_t servers[], int nServers,char *fileName, char *directory, int port, char *nome_cliente, char *IP_cliente);
void replicateUpdateFileNameOnBackups(server_t servers[], int nServers,char *newName, char *oldName, char *directory,int port, char *nome_cliente, char *IP_cliente);


int sendNewFileToServerBackups(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int removeFileInServerBackups(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int updateFileNameInServerBakcups(char *novoNovo,char *nomeAntigo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);

int receiveNewFileFromPrimary(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int removeFileInServerBackup(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int updateFileNameInBackup(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);