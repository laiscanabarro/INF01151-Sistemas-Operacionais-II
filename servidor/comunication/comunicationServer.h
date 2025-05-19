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

typedef struct {
    notification_t notification;
    operation_destiny_t destiny;
    int serverSock;
} operation_t;

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
