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





// Estrutura para representar um Replica Manager (RM)
typedef struct rm_info {
    int id;
    char ip[16];
    int port;
    int is_active;      
} rm_info;
typedef struct {
    notification_t notification;
    operation_destiny_t destiny;
    int serverSock;
} operation_t;

typedef struct {
    char clientName[100];
    operation_t operation;
} operationEntry_t;
// Tipos de comandos para eleição de Líder
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
    election_command_type_t election_cmd_type; // Tipo de comando de eleição
    int sender_id;                            // ID do processo que enviou a mensagem
    int leader_id;                            // ID do novo líder (apenas para CMD_COORDINATOR)
} election_message_payload;
typedef struct {
    char nome_cliente[100];
    int num_devices_conected;
    char IP_devices [2][16];
} clientInfo_t;
#define MAX_RMS 5 // Até 5 servidores no cluster (uso de exemplo no momento)
extern rm_info all_rms[MAX_RMS];
extern int num_all_rms; // Quantidade real de RMs

extern int my_rm_id; // ID desta instância do servidor
extern int current_leader_id; // ID do líder atualmente conhecido
extern pthread_mutex_t leader_mutex; // Mutex para proteger o acesso ao ID do 

// Declaração de variáveis globais (uso de extern)
#define MAX_OPERATIONS 300
#define MAX_CLIENTES 200

extern int nOperations;
extern operationEntry_t actualOperations[MAX_OPERATIONS];


// Declaração das funções
int receiveNewFileFromClient(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,rm_info all_rms [],int num_all_rms,int leader_id,char *nome_cliente,char *IP_cliente);
int removeFileInServer(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,rm_info all_rms [],int num_all_rms,int leader_id,char *nome_cliente,char *IP_cliente);
int updateFileName(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations,rm_info all_rms [],int num_all_rms,int leader_id,char *nome_cliente,char *IP_cliente);

int sendNewFileToClient(int novo_socket, char *diretorio,pthread_mutex_t *conflitOperations,char *nome_cliente);
int sendLastSecondNotificationToClient(int novo_socket, int ind_cliente, char *diretorio);
int receiveLastSecondLocalNotification(int ind_cliente, notification_t *notifications, char *diretorio);
int sendFileListToClient(int novo_socket, char *diretorio);
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos);
void filterNotifications(notification_t *notifications, int *num_notifications);

void replicateConnecitonOnBackups(rm_info all_rms [],int num_all_rms,int leader_id,char *nome_cliente,char *IP_cliente);

void replicateSendNewFileToBackups(rm_info all_rms [],int num_all_rms,int leader_id, char *fileName, char *directory, int port, char *nome_cliente, char *IP_cliente);
void replicateRemoveFileOnBackups(rm_info all_rms [], int num_all_rms,int leader_id,char *fileName, char *directory, int port, char *nome_cliente, char *IP_cliente);
void replicateUpdateFileNameOnBackups(rm_info all_rms [], int num_all_rms,int leader_id,char *newName, char *oldName, char *directory,int port, char *nome_cliente, char *IP_cliente);


int sendNewFileToServerBackups(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int removeFileInServerBackups(char *nomeArquivo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);
int updateFileNameInServerBakcups(char *novoNovo,char *nomeAntigo,char *diretorio,int PORTA, char * IP,char *nome_cliente,char *IP_cliente);

int receiveNewFileFromPrimary(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int removeFileInServerBackup(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);
int updateFileNameInBackup(int novo_socket, char *diretorio, pthread_mutex_t *conflitOperations);

// Funções para o algoritmo de Bully
int send_election_message(const char* ip, int port, int sender_id);
int send_answer_message(const char* ip, int port, int sender_id);
int send_coordinator_message(const char* ip, int port, int leader_id);
int send_heartbeat_to_rm(const char* ip, int port, int sender_id);
int handle_who_is_leader_query(int client_socket_fd); 
int send_heartbeat_to_clients(clientInfo_t clientesInfo[],int num_clientes,int port,char *IP_primario);
int send_heartbeat_to_client(const char* ip_destino, int port, const char* ip_primario);