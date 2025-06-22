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

typedef enum {
    CMD_ELECTION = 20,    // Mensagem de eleição (inicia a eleição)
    CMD_ANSWER,           // Mensagem de resposta (resposta a ELECTION)
    CMD_COORDINATOR,      // Mensagem de Coordenador (anuncia o novo líder)
    CMD_WHO_IS_LEADER,    // Cliente pergunta quem é o líder
    CMD_LEADER_IS,        // Servidor responde quem é o líder
    CMD_HEARTBEAT         // Mensagem de heartbeat do líder para backups
} election_command_type_t; 

// Novo payload para mensagens de eleição/coordenador
typedef struct election_message_payload {
    election_command_type_t election_cmd_type;  // Tipo de comando de eleição
    int sender_id;                              // ID do processo que enviou a mensagem
    int leader_id;                              // ID do novo líder (apenas para CMD_COORDINATOR)
    char leader_ip[16];                         // IP do líder (para CMD_COORDINATOR e CMD_LEADER_IS)
} election_message_payload;

int sendNewFileToServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int removeFileInServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int updateFileName(char *novoNovo,char *nomeAntigo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int receiveNewFileFromServer(char *nomeArquivo, char *diretorio, int PORTA, char *IP,char *nome_cliente,char *IP_cliente);
int receiveLastSecondNotificationFromServer(notification_t *notification,char *diretorio, int PORTA, char *IP,char *nome_cliente,char *IP_cliente);
int receiveFileListFromServer(char ***arquivosServidor,char *diretorio,int  PORTA,char * IP,char *nome_cliente,char *IP_cliente);
void filterNotifications(notification_t *notifications, int *num_notifications);
int receiveLastSecondLocalNotification(notification_t *notifications, char *diretorio);
int exitConnectionOnServer(int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
