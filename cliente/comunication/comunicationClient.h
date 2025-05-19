#include <sys/stat.h>
#include <time.h>



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


int sendNewFileToServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP);
int removeFileInServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP);
int updateFileName(char *novoNovo,char *nomeAntigo,char *diretorio,int PORTA, char * IP);
int receiveNewFileFromServer(char *nomeArquivo, char *diretorio, int PORTA, char *IP);
int receiveLastSecondNotificationFromServer(notification_t *notification, int PORTA, char *IP);
int receiveFileListFromServer(char ***arquivosServidor,int  PORTA,char * IP);
void filterNotifications(notification_t *notifications, int *num_notifications);
int receiveLastSecondLocalNotification(notification_t *notifications, char *diretorio);
//int receiveLastSessionNotificationFromServer(notification_t *notification, int PORTA, char *IP);


