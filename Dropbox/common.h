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

// ---- [ELEIÇÃO DE LÍDER - início das definições] ----

// Tipos de comandos para eleição de Líder
typedef enum {
    CMD_ELECTION = 20,    // Mensagem de eleição (inicia a eleição)
    CMD_ANSWER,           // Mensagem de resposta (resposta a ELECTION)
    CMD_COORDINATOR,      // Mensagem de Coordenador (anuncia o novo líder)
    CMD_WHO_IS_LEADER,    // Cliente pergunta quem é o líder
    CMD_LEADER_IS,        // Servidor responde quem é o líder
    CMD_HEARTBEAT         // Mensagem de heartbeat do líder para backups
} election_command_type_t; 

// Estrutura para representar um Replica Manager (RM)
typedef struct rm_info {
    int id;
    char ip[16];
    int port;
    int is_active;      
} rm_info;

// Novo payload para mensagens de eleição/coordenador
typedef struct election_message_payload {
    election_command_type_t election_cmd_type; // Tipo de comando de eleição
    int sender_id;                            // ID do processo que enviou a mensagem
    int leader_id;                            // ID do novo líder (apenas para CMD_COORDINATOR)
} election_message_payload;

#define MAX_RMS 5 // Até 5 servidores no cluster (uso de exemplo no momento)
extern rm_info all_rms[MAX_RMS];
extern int num_all_rms; // Quantidade real de RMs

extern int my_rm_id; // ID desta instância do servidor
extern int current_leader_id; // ID do líder atualmente conhecido
extern pthread_mutex_t leader_mutex; // Mutex para proteger o acesso ao ID do 

// ---- [ELEIÇÃO DE LÍDER - fim das definições] ----

#endif // COMMON_H
