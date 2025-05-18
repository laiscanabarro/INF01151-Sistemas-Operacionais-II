#include <sys/stat.h>
#include <time.h>
int enviando=0;
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

int receiveNewFileFromClient(int novo_socket,char *diretorio);
int removeFileInServer(int novo_socket,char *diretorio);
int updateFileName(int novo_socket,char *diretorio);
int sendNewFileToClient(int novo_socket, char *diretorio);
int sendLastSecondNotificationToClient(int novo_socket,char *diretorio);
//int sendLastSessionNotificationTocLient(int novo_socket,char *diretorio,time_t timeLastSession);
int receiveLastSecondLocalNotification(notification_t *notification, char *diretorio);
int sendFileListToClient(int novo_socket, char *diretorio);
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos);
//int receiveLastSessionLocalNotification(notification_t *notification, char *diretorio, time_t timeLastSession);
void filterNotifications(notification_t *notifications, int *num_notifications);


