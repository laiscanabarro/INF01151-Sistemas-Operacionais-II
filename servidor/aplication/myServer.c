#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/time.h>
#include "../comunication/comunicationServer.h"
#include <errno.h>
#define PORTA 8080
#define TAMANHO_BUFFER 1024

time_t last_heartbeat_time;

// Variáveis globais para o cluster
rm_info all_rms[MAX_RMS];           // Lista de todos os RMs (preenchido no main)
int num_all_rms = 0;                // Quantidade de RMs (preenchido no main)
int my_rm_id = -1;                  // ID desta instância do servidor (preenchido no main)
int current_leader_id = -1;         // Líder conhecido (preenchido na eleição)

// Mutexes para proteger o estado da eleição e do líder 
pthread_mutex_t leader_mutex = PTHREAD_MUTEX_INITIALIZER;   // Protege current_leader_id
pthread_mutex_t election_mutex = PTHREAD_MUTEX_INITIALIZER; // Proteger o estado de eleição (flags)

int election_in_progress = 0; // Flag para indicar se uma eleição está em andamento
int answer_received = 0;      // Flag para saber se recebeu resposta de um RM maior durante a eleição

clientInfo_t clientes_info[200];
int num_clientes=0;

pthread_mutex_t conflitOperation = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t insertDevice = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t heartbeat_mutex = PTHREAD_MUTEX_INITIALIZER;

// Estrutura para passar parâmetros para a thread
typedef struct {
    int socket;
} ThreadArgs;

void listar_clientes_conectados() {
    printf("=== Clientes conectados ===\n");
    for (int i = 0; i < num_clientes; i++) {
        printf("Cliente: %s\n", clientes_info[i].nome_cliente);
        printf("  Devices conectados: %d\n", clientes_info[i].num_devices_conected);

        for (int j = 0; j < clientes_info[i].num_devices_conected && j < 2; j++) {
            printf("    - IP do device %d: %s\n", j + 1, clientes_info[i].IP_devices[j]);
        }
    }
    printf("===========================\n");
}

int buscar_indice_cliente(const char *nome_cliente) {
    for (int i = 0; i < num_clientes; i++) {
        if (strcmp(clientes_info[i].nome_cliente, nome_cliente) == 0) {
            return i;  // cliente encontrado
        }
    }
    return -1;  // cliente não encontrado
}

int inserir_cliente(const char *nome_cliente){
    if(num_clientes<200){
        strcpy(clientes_info[num_clientes].nome_cliente,nome_cliente);
        clientes_info[num_clientes].num_devices_conected=0;
        num_clientes++;
        return 0;
    }
    return -1;  // cliente não encontrado
}

int buscar_indice_device(int indCliente,char * IP_device) {
    for (int i = 0; i < clientes_info[indCliente].num_devices_conected; i++) {
        if (strcmp(clientes_info[indCliente].IP_devices[i], IP_device) == 0) {
            return i;  // cliente encontrado
        }
    }
    return -1;  // cliente não encontrado
}

void inserir_device(int indCliente,char * IP_device) {
    strcpy(clientes_info[indCliente].IP_devices[clientes_info[indCliente].num_devices_conected],IP_device);
    clientes_info[indCliente].num_devices_conected++;
}

// Função para iniciar o algoritmo de Bully
// Um RM chama esta função quando detecta uma falha do líder ou quando não há líder conhecido
void start_election() {
    pthread_mutex_lock(&election_mutex);    
    election_in_progress = 1;
    answer_received = 0;
    pthread_mutex_unlock(&election_mutex);  

    int higher_rm_found = 0;
    // Percorre todos os RMs para encontrar aqueles com ID maior
    for (int i = 0; i < num_all_rms; i++) {
        // Envia mensagem de eleição para RMs com ID maior
        if (all_rms[i].id > my_rm_id) { 
            // send_election_message tenta enviar a mensagem. Retorna 0 em sucesso.
            if (send_election_message(all_rms[i].ip, all_rms[i].port, my_rm_id) == 0) {
                higher_rm_found = 1; // Encontrou um RM maior e conseguiu enviar
            }
        }
    }
    // Se não encontrou nenhum RM maior ou todos os maiores falharam
    if (!higher_rm_found) { 
        // Eu sou o maior ID entre os ativos, me declaro coordenador
        pthread_mutex_lock(&leader_mutex);      
        current_leader_id = my_rm_id;
        pthread_mutex_unlock(&leader_mutex);    

        printf("Eu (RM %d) sou o novo líder!\n", my_rm_id);
        // Anuncia para todos os outros RMs que eu sou o coordenador
        for (int i = 0; i < num_all_rms; i++) {
            if (all_rms[i].id != my_rm_id) { // Não envia para si mesmo
                send_coordinator_message(all_rms[i].ip, all_rms[i].port, my_rm_id);
            }
        }
        // Eleição finalizada
        pthread_mutex_lock(&election_mutex);       
        election_in_progress = 0; 
        pthread_mutex_unlock(&election_mutex);     
    } else {
        // Se encontrou RMs maiores, o processo atual espera por um ANSWER ou COORDINATOR
        // A lógica de timeout/recebimento é tratada em 'handle_client' e 'heartbeat_thread'
    }
}

// Função para lidar com mensagens de eleição recebidas de outros RMs 
void handle_election_message(int novo_socket, election_message_payload payload) {
    pthread_mutex_lock(&election_mutex);        // Bloqueia para manipular o estado da eleição
    printf("RM %d recebeu mensagem %d do RM %d\n", my_rm_id, payload.election_cmd_type, payload.sender_id);

    switch (payload.election_cmd_type) {
        case CMD_ELECTION:      // Recebeu uma mensagem de ELECTION de um RM com ID menor 
            // Envia uma resposta se meu ID for maior
            if (my_rm_id > payload.sender_id) {
                rm_info sender_rm_info;
                int found = 0;
                for(int i = 0; i < num_all_rms; i++) {
                    if (all_rms[i].id == payload.sender_id) {
                        sender_rm_info = all_rms[i];
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    send_answer_message(sender_rm_info.ip, sender_rm_info.port, my_rm_id);
                } else {
                    fprintf(stderr, "Erro: RM remetente com ID %d não encontrado em all_rms.\n", payload.sender_id);
                }

                // Se eu ainda não estivesse em uma eleição, comece uma
                if (!election_in_progress) {
                    election_in_progress = 1;
                    answer_received = 0;
                    pthread_mutex_unlock(&election_mutex); 
                    start_election();                     
                    return;
                }
            }
            break;
        case CMD_ANSWER:        // Recebeu uma mensagem de ANSWER (OK) de um RM com ID maior 
            // Defina a flag answer_received como verdadeira, indicando que um RM mais alto está ativo
            // Isso deve fazer com que a eleição atual deste RM seja abortada
            answer_received = 1;
            printf("RM %d recebeu uma ANSWER de RM %d. Abortando eleição.\n", my_rm_id, payload.sender_id);
            break;
        case CMD_COORDINATOR:   // Recebeu uma mensagem de COORDINATOR, indicando um novo líder 
            // Reconheça o novo líder
            pthread_mutex_lock(&leader_mutex);     
            current_leader_id = payload.leader_id;
            pthread_mutex_unlock(&leader_mutex);   
            election_in_progress = 0;               
            answer_received = 0;                 
            printf("RM %d: O novo líder é o RM %d.\n", my_rm_id, current_leader_id);
            break;
        case CMD_HEARTBEAT:    // Recebeu um heartbeat do líder atual
            // Atualizar o líder
            pthread_mutex_lock(&leader_mutex);     
            current_leader_id = payload.sender_id; 
            pthread_mutex_lock(&heartbeat_mutex);
            last_heartbeat_time = time(NULL);
            pthread_mutex_unlock(&heartbeat_mutex);
            pthread_mutex_unlock(&leader_mutex);   
            election_in_progress = 0;             
            answer_received = 0;
            break;
        default:
            printf("RM %d: Mensagem de eleição desconhecida: %d\n", my_rm_id, payload.election_cmd_type);
            break;
    }
    pthread_mutex_unlock(&election_mutex);
}

// ALTERAÇÕES: Renomeado e modificado o handle_client_or_backups existente para ser um manipulador de solicitação de cliente dedicado
void *handle_client_request(void *args) {
    typedef struct { int socket; int code; } ClientRequestArgs;
    ClientRequestArgs *client_req_args = (ClientRequestArgs *)args;
    int novo_socket = client_req_args->socket;
    int codigo = client_req_args->code;

    char nome_cliente[100] = {0};
    int tamanho_nome_cliente = 0;
    ssize_t bytes_received_name_size = recv(novo_socket, &tamanho_nome_cliente, sizeof(int), 0);
    if (bytes_received_name_size <= 0) {
        if (bytes_received_name_size == 0) {
            printf("[SERVER RM %d] handle_client_request - Cliente no socket %d fechou a conexão ao esperar tamanho_nome_cliente.\n", my_rm_id, novo_socket);
        } else {
            perror("[SERVER RM %d] handle_client_request - Erro ao receber tamanho_nome_cliente para socket %d\n");
        }
        free(args);
        close(novo_socket);
        pthread_exit(NULL);
    }

    if (tamanho_nome_cliente <= 0 || tamanho_nome_cliente >= sizeof(nome_cliente)) {
        fprintf(stderr, "[SERVER RM %d] handle_client_request - Tamanho de nome_cliente inválido ou erro ao receber: %d para socket %d\n", my_rm_id, tamanho_nome_cliente, novo_socket);
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }

    if (recv(novo_socket, nome_cliente, tamanho_nome_cliente, 0) <= 0) {
        perror("Erro ao receber nome do cliente");
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    nome_cliente[tamanho_nome_cliente] = '\0';

    int tamanho_nome_IP;
    char IP_cliente[16];
    if (recv(novo_socket, &tamanho_nome_IP, sizeof(int), 0) <= 0 ||
        recv(novo_socket, IP_cliente, tamanho_nome_IP, 0) <= 0) {
        perror("Erro ao receber IP do cliente");
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    IP_cliente[tamanho_nome_IP] = '\0';

    char diretorio[1000] = {0};
    snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", getenv("HOME"), nome_cliente);

    if (codigo == 0) { 
        printf("Recebido código 0 (conexão inicial) do cliente %s\n", nome_cliente);
        pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);

        if (ind_cliente == -1) {
            inserir_cliente(nome_cliente);
            ind_cliente = num_clientes - 1;

            DIR *dir = opendir(diretorio);
            if (dir == NULL) {
                char mkdirCommand[1500];
                snprintf(mkdirCommand, sizeof(mkdirCommand), "mkdir -p %s", diretorio);
                if (system(mkdirCommand) != 0) {
                    perror("Erro ao criar o diretório do cliente no servidor");
                    pthread_mutex_unlock(&insertDevice);
                    close(novo_socket);
                    free(args);
                    pthread_exit(NULL);
                }
                printf("Diretório criado com sucesso para %s: %s\n", nome_cliente, diretorio);
                sleep(3); 
            }
            if (dir) closedir(dir);
        }

        int codigo_device = 0;
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        if (ind_device == -1) {
            if (clientes_info[ind_cliente].num_devices_conected < 2) {
                inserir_device(ind_cliente, IP_cliente);
                printf("[SERVER RM %d] Novo device '%s' adicionado para cliente '%s' com IP: %s\n", my_rm_id, IP_cliente, nome_cliente, IP_cliente); 
            } else {
                codigo_device = -1;
                fprintf(stderr, "[SERVER RM %d] Cliente '%s' já atingiu o limite de devices.\n", my_rm_id, nome_cliente); 
            }
        } else {
            printf("[SERVER RM %d] Device com IP: %s para cliente '%s' já existe.\n", my_rm_id, IP_cliente, nome_cliente); 
        }
        send(novo_socket, &codigo_device, sizeof(int), 0); 

        if (codigo_device == 0) { 
            replicateConnecitonOnBackups(all_rms, num_all_rms, current_leader_id, nome_cliente, IP_cliente);
            int waitBackup = 0; 
            send(novo_socket, &waitBackup, sizeof(int), 0);
        }
        pthread_mutex_unlock(&insertDevice);
        usleep(500000); 
    } else if (codigo == 1) { 
        if (receiveNewFileFromClient(novo_socket, diretorio, &conflitOperation, all_rms, num_all_rms, current_leader_id, nome_cliente, IP_cliente) == 1)
            printf("Erro ao receber o arquivo\n");
    } else if (codigo == 2) { 
        if (removeFileInServer(novo_socket, diretorio, &conflitOperation, all_rms, num_all_rms, current_leader_id, nome_cliente, IP_cliente) == 1)
            printf("Erro ao remover o arquivo\n");
    } else if (codigo == 3) {
        if (updateFileName(novo_socket, diretorio, &conflitOperation, all_rms, num_all_rms, current_leader_id, nome_cliente, IP_cliente) == 1)
            printf("Erro ao atualizar o nome do arquivo\n");
    } else if (codigo == 4) { 
        if (sendNewFileToClient(novo_socket, diretorio, &conflitOperation, nome_cliente) == 1)
            printf("Erro ao atualizar o arquivo no cliente\n");
    } else if (codigo == 5) { 
        pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        pthread_mutex_unlock(&insertDevice);
        if (sendLastSecondNotificationToClient(novo_socket, ind_cliente, diretorio) == 1)
            printf("Erro ao enviar notificação para o cliente\n");
    } else if (codigo == 6) {
        if (sendFileListToClient(novo_socket, diretorio) == 1)
            printf("Erro ao enviar lista de arquivos para o cliente\n");
    } else if (codigo == 7) { 
        pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        if (ind_cliente != -1 && ind_device != -1) {
            strcpy(clientes_info[ind_cliente].IP_devices[ind_device], "");
            clientes_info[ind_cliente].num_devices_conected--;
            if (clientes_info[ind_cliente].num_devices_conected == 0) {
                strcpy(clientes_info[ind_cliente].nome_cliente, "");
            }
        }
        pthread_mutex_unlock(&insertDevice);
        int cod_encerra = 0; 
        send(novo_socket, &cod_encerra, sizeof(int), 0);
    } else if (codigo == 8) { 
        int ok = 0;
        send(novo_socket, &ok, sizeof(int), 0);
    } else {
        fprintf(stderr, "Código de operação do cliente desconhecido: %d\n", codigo);
    }
    free(args); // Liberar a memória alocada para os argumentos
    close(novo_socket);
    pthread_exit(NULL);
}

// ALTERAÇÃO: Nova função para lidar com mensagens de replicação para backups
void *handle_replication_message(void *args) {
    typedef struct { int socket; int code; } ReplicationMessageArgs; 
    ReplicationMessageArgs *repl_msg_args = (ReplicationMessageArgs *)args;
    int novo_socket = repl_msg_args->socket;
    int codigo = repl_msg_args->code;

    char nome_cliente[100] = {0};
    int tamanho_nome_cliente = 0;
    if (recv(novo_socket, &tamanho_nome_cliente, sizeof(int), 0) <= 0 ||
        tamanho_nome_cliente <= 0 || tamanho_nome_cliente >= sizeof(nome_cliente)) {
        fprintf(stderr, "Tamanho de nome_cliente inválido ou erro ao receber (replicação): %d\n", tamanho_nome_cliente);
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    if (recv(novo_socket, nome_cliente, tamanho_nome_cliente, 0) <= 0) {
        perror("Erro ao receber nome do cliente (replicação)");
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    nome_cliente[tamanho_nome_cliente] = '\0';

    int tamanho_nome_IP;
    char IP_cliente[16];
    if (recv(novo_socket, &tamanho_nome_IP, sizeof(int), 0) <= 0 ||
        recv(novo_socket, IP_cliente, tamanho_nome_IP, 0) <= 0) {
        perror("Erro ao receber IP do cliente (replicação)");
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    IP_cliente[tamanho_nome_IP] = '\0';

    char diretorio[1000] = {0};
    snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", getenv("HOME"), nome_cliente);

    pthread_mutex_lock(&insertDevice); 
    int ind_cliente = buscar_indice_cliente(nome_cliente);
    if (ind_cliente == -1) {
        inserir_cliente(nome_cliente);
        ind_cliente = num_clientes - 1;
        DIR *dir = opendir(diretorio);
        if (dir == NULL) {
            char mkdirCommand[1500];
            snprintf(mkdirCommand, sizeof(mkdirCommand), "mkdir -p %s", diretorio);
            if (system(mkdirCommand) != 0) {
                perror("Erro ao criar o diretório do cliente no servidor backup");
                pthread_mutex_unlock(&insertDevice);
                close(novo_socket);
                free(args);
                pthread_exit(NULL);
            }
            printf("Diretório criado com sucesso para %s no backup: %s\n", nome_cliente, diretorio);
            sleep(1); 
        }
        if (dir) closedir(dir);
    }

    int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
    if (ind_device == -1) {
        if (clientes_info[ind_cliente].num_devices_conected < 2) {
            inserir_device(ind_cliente, IP_cliente);
        }
    }
    pthread_mutex_unlock(&insertDevice);

    if (codigo == 0) { 
        printf("Backup RM %d: Conexão inicial replicada para cliente %s\n", my_rm_id, nome_cliente);
        int ok = 0;
        send(novo_socket, &ok, sizeof(int), 0); 
    } else if (codigo == 1) { 
        if (receiveNewFileFromPrimary(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Backup RM %d: Erro ao receber arquivo replicado\n", my_rm_id);
        else
            printf("Backup RM %d: Arquivo replicado com sucesso.\n", my_rm_id);
    } else if (codigo == 2) { 
        if (removeFileInServerBackup(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Backup RM %d: Erro ao remover arquivo replicado\n", my_rm_id);
        else
            printf("Backup RM %d: Arquivo removido replicado com sucesso.\n", my_rm_id);
    } else if (codigo == 3) { 
        if (updateFileNameInBackup(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Backup RM %d: Erro ao renomear arquivo replicado\n", my_rm_id);
        else
            printf("Backup RM %d: Arquivo renomeado replicado com sucesso.\n", my_rm_id);
    } else {
        fprintf(stderr, "Backup RM %d: Código de operação de replicação desconhecido: %d\n", my_rm_id, codigo);
    }

    free(args);
    close(novo_socket);
    pthread_exit(NULL);
}

// ALTERAÇÃO: Este é o novo manipulador unificado para TODAS as conexões de entrada
void *handle_incoming_connection(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket; 
    int initial_code;

    ssize_t bytes_read_initial_code = recv(novo_socket, &initial_code, sizeof(int), 0);
    if (bytes_read_initial_code <= 0) {
        if (bytes_read_initial_code == 0) {
            printf("[SERVER RM %d] Cliente no socket %d fechou a conexão antes de enviar o código inicial (recv retornou 0).\n", my_rm_id, novo_socket);
        } else { // bytes_read_initial_code < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[SERVER RM %d] Timeout no recv do código inicial para socket %d. Cliente não enviou dados a tempo.\n", my_rm_id, novo_socket);
            } else {
                perror("[SERVER RM %d] Erro inesperado no recv do código inicial para socket %d\n");
            }
        }
        free(args);
        close(novo_socket);
        pthread_exit(NULL);
    }

    if (initial_code == 9) {
        int received_election_cmd_type;
        if (recv(novo_socket, &received_election_cmd_type, sizeof(int), 0) <= 0) {
            perror("Erro ao receber o tipo de comando de eleição");
            free(args);
            close(novo_socket);
            pthread_exit(NULL);
        }

        election_message_payload payload;
        payload.election_cmd_type = (election_command_type_t)received_election_cmd_type;

        size_t size_of_cmd_type = sizeof(int);
        size_t remaining_payload_size = sizeof(election_message_payload) - size_of_cmd_type;

        if (recv(novo_socket, ((char*)&payload) + size_of_cmd_type, remaining_payload_size, 0) <= 0) {
            perror("Erro ao receber o restante do payload da mensagem de eleição");
            free(args);
            close(novo_socket);
            pthread_exit(NULL);
        }

        if (payload.election_cmd_type == CMD_WHO_IS_LEADER) {
            handle_who_is_leader_query(novo_socket);
        } else {
            handle_election_message(novo_socket, payload);
        }
    } else { 
        pthread_mutex_lock(&leader_mutex);
        int leader = current_leader_id;
        pthread_mutex_unlock(&leader_mutex);

        if (my_rm_id == leader) {
            typedef struct { int socket; int code; } ClientRequestArgs;
            ClientRequestArgs *client_req_args = malloc(sizeof(ClientRequestArgs));
            if (client_req_args == NULL) { // Verificação de alocação
                printf("[SERVER RM %d] Erro ao alocar ClientRequestArgs para socket %d\n", my_rm_id, novo_socket);
                close(novo_socket);
                free(args); 
                pthread_exit(NULL);
            }
            client_req_args->socket = novo_socket;
            client_req_args->code = initial_code;

            pthread_t client_thread_id;
            if (pthread_create(&client_thread_id, NULL, handle_client_request, (void *)client_req_args) != 0) {
                printf("[SERVER RM %d] ERRO FATAL: Falha ao criar a thread para requisição do cliente para socket %d\n", my_rm_id, novo_socket);
                free(client_req_args); 
                close(novo_socket);
            } 
            pthread_detach(client_thread_id);
        } else { 
            printf("[SERVER RM %d] Sou BACKUP. Despachando para handle_replication_message (código %d) para socket %d.\n", my_rm_id, initial_code, novo_socket);
            if (initial_code >= 0 && initial_code <= 8) { 
                typedef struct { int socket; int code; } ReplicationMessageArgs;
                ReplicationMessageArgs *repl_msg_args = malloc(sizeof(ReplicationMessageArgs));
                repl_msg_args->socket = novo_socket;
                repl_msg_args->code = initial_code;

                pthread_t replication_thread_id;
                if (pthread_create(&replication_thread_id, NULL, handle_replication_message, (void *)repl_msg_args) != 0) {
                    perror("Erro ao criar a thread para mensagem de replicação");
                    free(repl_msg_args);
                    close(novo_socket);
                }
                pthread_detach(replication_thread_id);

            } else {
                fprintf(stderr, "RM %d (backup): Recebido código desconhecido %d (não é eleição/heartbeat nem operação de replicação).\n", my_rm_id, initial_code);
                close(novo_socket);
                free(args); 
            }
        }
    }
    free(args); // Libera o args que foi mallocado no main
    pthread_exit(NULL); // A thread handle_incoming_connection finaliza após despachar
}

void obter_ip_local(char *ip_buffer, size_t buffer_size) {
    FILE *fp = popen("hostname -I", "r");
    if (fp == NULL) {
        perror("Erro ao executar comando");
        snprintf(ip_buffer, buffer_size, "Erro");
        return;
    }

    if (fgets(ip_buffer, buffer_size, fp) != NULL) {
        // Remove o '\n' se houver
        char *newline = strchr(ip_buffer, '\n');
        if (newline) *newline = '\0';

        // Se houver vários IPs separados por espaço, pega só o primeiro
        char *espaco = strchr(ip_buffer, ' ');
        if (espaco) *espaco = '\0';
    } else {
        snprintf(ip_buffer, buffer_size, "Desconhecido");
    }

    pclose(fp);
}

// Valida se uma string é um IP IPv4 válido
int validar_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1;
}

// Analisa os argumentos e preenche o vetor servers[]
// Retorna 0 em caso de sucesso, ou -1 em caso de erro
int parse_server_args(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <IP_PRIMARIO> [<IP_BACKUP1> <IP_BACKUP2> ...]\n", argv[0]);
        return -1;
    }

    if (argc - 1 > MAX_RMS) {
        fprintf(stderr, "Erro: número máximo de servidores é %d.\n",MAX_RMS);
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        if (!validar_ip(argv[i])) {
            fprintf(stderr, "Erro: IP inválido -> %s\n", argv[i]);
            return -1;
        }
        strncpy(all_rms[num_all_rms].ip, argv[i], 15);
        all_rms[num_all_rms].ip[15] = '\0'; // garantir null-termination
        all_rms[num_all_rms].id=i;
        all_rms[num_all_rms].port=PORTA;
        all_rms[num_all_rms].is_active=1;
        num_all_rms++;
    }

    current_leader_id=num_all_rms;
    char ip_local[16];
    obter_ip_local(ip_local, sizeof(ip_local));
    for(int i=0;i<num_all_rms;i++){
       if(strcmp(all_rms[i].ip,ip_local)==0){
        my_rm_id=all_rms[i].id;
       }

    }

    if(current_leader_id==my_rm_id)
        printf("Sou o primario\n");
    else
        printf("Sou o backup");

    return 0;
}

int server_init(int *servidor_fd, struct sockaddr_in *endereco, int porta) {
    int opt = 1;

    // 1. Criação do socket
    *servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*servidor_fd < 0) {
        perror("[server_init] Erro ao criar socket");
        return -1;
    }

    // 2. Permite reuso do endereço local
    if (setsockopt(*servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[server_init] Erro no setsockopt");
        close(*servidor_fd);
        return -1;
    }

    // 3. Preenche a struct do endereço
    memset(endereco, 0, sizeof(*endereco));
    endereco->sin_family = AF_INET;
    endereco->sin_addr.s_addr = INADDR_ANY;
    endereco->sin_port = htons(porta);

    // 4. Faz bind do socket com o endereço
    if (bind(*servidor_fd, (struct sockaddr *)endereco, sizeof(*endereco)) < 0) {
        perror("[server_init] Erro no bind");
        close(*servidor_fd);
        return -1;
    }

    // 5. Coloca o socket para escutar
    if (listen(*servidor_fd, 3) < 0) {
        perror("[server_init] Erro no listen");
        close(*servidor_fd);
        return -1;
    }

    return 0; // Sucesso
}

// Thread para enviar e receber heartbeats
// Usado para a detecção de falhas do líder e para manter a coesão do cluster
void* heartbeat_thread(void* arg) {
    while (1) {
        usleep(100000);
        pthread_mutex_lock(&leader_mutex);      
        int leader = current_leader_id;
        pthread_mutex_unlock(&leader_mutex);   

        if (my_rm_id == leader) { // Sou o líder
            // Envia heartbeats para todos os backups (ou outros RMs)
            for (int i = 0; i < num_all_rms; i++) {
                if (all_rms[i].id != my_rm_id) { // Não sou eu e não é o líder
                    send_heartbeat_to_rm(all_rms[i].ip, all_rms[i].port, my_rm_id);
                }
            }
            pthread_mutex_lock(&insertDevice); 
            send_heartbeat_to_clients(clientes_info, num_clientes, 9000, all_rms[my_rm_id-1].ip);
            pthread_mutex_unlock(&insertDevice); 
        } else {    
            time_t agora = time(NULL);
            pthread_mutex_lock(&heartbeat_mutex);
            time_t ultimo = last_heartbeat_time;
            pthread_mutex_unlock(&heartbeat_mutex);

            if (difftime(agora, ultimo) > 3.0) {
                // Falha no heartbeat para o líder, então iniciar eleição
                pthread_mutex_lock(&election_mutex);        
                if (!election_in_progress) {                
                    printf("Líder %d falhou! Iniciando eleição...\n", leader);
                    election_in_progress = 1;
                    answer_received = 0; 
                    pthread_mutex_unlock(&election_mutex); 
                    start_election();                       
                } else {
                    pthread_mutex_unlock(&election_mutex);  
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);

    if (parse_server_args(argc, argv) != 0) {
        return 1;
    }

    last_heartbeat_time = time(NULL); 

    // Criação do socket
    int servidor_fd, novo_socket;
    struct sockaddr_in endereco;
    int tamanho_endereco = sizeof(endereco);
    if (server_init(&servidor_fd, &endereco, PORTA) != 0) {
        fprintf(stderr, "Erro ao iniciar servidor\n");
        exit(1);
    }

    // Inicia heartbeat thread
    pthread_t hb_thread_id;
    if (pthread_create(&hb_thread_id, NULL, heartbeat_thread, NULL) != 0) {
        perror("Erro ao criar a thread de heartbeat");
        return 1;
    }
    pthread_detach(hb_thread_id);

    while (1) {
        novo_socket = accept(servidor_fd, (struct sockaddr *)&endereco, (socklen_t *)&tamanho_endereco);
        if (novo_socket < 0) {
            perror("[SERVER RM %d] Erro no accept\n"); 
            continue;
        }

        // Aloca argumentos para a thread
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (args == NULL) {
            perror("[SERVER RM %d] Erro ao alocar ThreadArgs\n"); // Mensagem melhorada
            close(novo_socket);
            continue;
        }
        args->socket = novo_socket;

        pthread_t thread_id;
        // All incoming connections go to the unified handler
        if (pthread_create(&thread_id, NULL, handle_incoming_connection, (void *)args) != 0) {
            perror("[SERVER RM %d] Erro ao criar a thread de conexão\n"); // Mensagem melhorada
            free(args); // Free args if thread creation fails
            close(novo_socket);
        }
        pthread_detach(thread_id); // Libera recursos automaticamente ao término
    }

    close(servidor_fd);
    return 0;
}
