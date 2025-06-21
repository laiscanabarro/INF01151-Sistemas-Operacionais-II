#include "../comunication/comunicationServer.h"
#include <asm-generic/socket.h>

#define TAMANHO_BUFFER 1024

pthread_mutex_t conflitOperation = PTHREAD_MUTEX_INITIALIZER;
// Estrutura para passar parâmetros para a thread
typedef struct {
    int socket;
    char diretorio[TAMANHO_BUFFER];
} ThreadArgs;

client_list server_clients;

// Variáveis globais para o cluster
rm_info all_rms[MAX_RMS]; 
int num_all_rms = 0; // Quantidade de RMs
int my_rm_id = -1; // ID desta instância do servidor (preenchido no main)
int current_leader_id = -1; // Líder conhecido (preenchido na eleição)

pthread_mutex_t leader_mutex = PTHREAD_MUTEX_INITIALIZER;   // Protege current_leader_id
pthread_mutex_t election_mutex = PTHREAD_MUTEX_INITIALIZER; // Proteger o estado de eleição

int election_in_progress = 0; // Flag para indicar se uma eleição está em andamento
int answer_received = 0;      // Flag para saber se recebeu OK de um RM maior

// Função para iniciar o algoritmo de Bully
void start_election() {
    pthread_mutex_lock(&election_mutex);
    election_in_progress = 1;
    answer_received = 0;
    pthread_mutex_unlock(&election_mutex);

    int higher_rm_found = 0;
    for (int i = 0; i < num_all_rms; i++) {
        if (all_rms[i].id > my_rm_id) { // Envia mensagem de eleição para RMs com ID maior
            if (send_election_message(all_rms[i].ip, all_rms[i].port, my_rm_id) == 0) {
                higher_rm_found = 1; // Encontrou um RM maior e conseguiu enviar
            }
        }
    }

    if (!higher_rm_found) { // Se não encontrou nenhum RM maior ou todos falharam
        // Eu sou o maior ID entre os ativos, me declaro coordenador
        pthread_mutex_lock(&leader_mutex);
        current_leader_id = my_rm_id;
        pthread_mutex_unlock(&leader_mutex);

        printf("Eu (RM %d) sou o novo líder!\n", my_rm_id);
        // Anuncia para todos os outros RMs que eu sou o coordenador
        for (int i = 0; i < num_all_rms; i++) {
            if (all_rms[i].id != my_rm_id) {
                send_coordinator_message(all_rms[i].ip, all_rms[i].port, my_rm_id);
            }
        }
        // Eleição finalizada
        pthread_mutex_lock(&election_mutex);
        election_in_progress = 0; 
        pthread_mutex_unlock(&election_mutex);

    } else {
        // Espere por OK ou Coordinator (implementado na função handle_client_thread para mensagens recebidas)
        // Se receber OK, minha eleição para. Se não receber nada após timeout, assumo que os maiores falharam e inicio novamente.
        // Implementação de timeout pode ser mais complexa (thread separada ou semaforos/condvars)
    }
}

// Thread para enviar e receber heartbeats
void* heartbeat_thread(void* arg) {
    // Lógica de heartbeat:
    // Se for um líder: envia heartbeats para todos os backups
    // Se for um backup: espera heartbeats do primário. Se não receber, inicia eleição
    // Se for um backup: envia heartbeats para o primário
    while (1) {
        usleep(1000000);
        pthread_mutex_lock(&leader_mutex);
        int leader = current_leader_id;
        pthread_mutex_unlock(&leader_mutex);

        if (my_rm_id == leader) { // Sou o líder
            // Envia heartbeats para todos os backups
            for (int i = 0; i < num_all_rms; i++) {
                if (all_rms[i].id != my_rm_id && all_rms[i].id != leader) { // Não sou eu e não é o líder
                    // Tentar enviar um heartbeat para all_rms[i]
                    // Se falhar para algum backup, não é crítico.
                    // Para replicação passiva, o líder sempre tenta se comunicar com backups.
                }
            }
        } else { // Sou um backup
            // Tentar enviar heartbeat para o líder.
            // Se o líder não responder após N tentativas, iniciar eleição.
            rm_info leader_info;
            int found_leader = 0;
            for(int i = 0; i < num_all_rms; i++) {
                if(all_rms[i].id == leader) {
                    leader_info = all_rms[i];
                    found_leader = 1;
                    break;
                }
            }

            if (found_leader && leader_info.id != my_rm_id) { // Se o líder não sou eu
                // Tenta enviar um heartbeat para o líder
                if (send_heartbeat_to_rm(leader_info.ip, leader_info.port, my_rm_id) < 0) {
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
            } else if (leader == -1 || leader == my_rm_id) {
                // Se não há líder conhecido ou se eu sou o líder (mas não me enviei heartbeat)
                // Isto é para cenários onde um RM se inicia ou detecta que não há líder.
                 pthread_mutex_lock(&election_mutex);
                 if (!election_in_progress) {
                    printf("Nenhum líder conhecido ou líder falhou, iniciando eleição...\n");
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
    return NULL;
}

void handle_election_message(int novo_socket, election_message_payload payload) {
    pthread_mutex_lock(&election_mutex);
    printf("RM %d recebeu mensagem %d do RM %d\n", my_rm_id, payload.election_cmd_type, payload.sender_id);

    switch (payload.election_cmd_type) {
        case CMD_ELECTION:
            // Envia uma resposta se meu ID for maior
            if (my_rm_id > payload.sender_id) {
                send_ok_message(all_rms[payload.sender_id -1].ip, all_rms[payload.sender_id -1].port, my_rm_id);
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
        case CMD_ANSWER:
            // Defina a flag como verdadeira, indicando que um RM mais alto está ativo
            answer_received = 1;
            printf("RM %d recebeu uma ANSWER de RM %d. Abortando eleição.\n", my_rm_id, payload.sender_id);
            break;
        case CMD_COORDINATOR:
            // Reconheça o novo líder
            pthread_mutex_lock(&leader_mutex);
            current_leader_id = payload.leader_id;
            pthread_mutex_unlock(&leader_mutex);
            election_in_progress = 0; 
            answer_received = 
            printf("RM %d: O novo líder é o RM %d.\n", my_rm_id, current_leader_id);
            break;
        case CMD_HEARTBEAT:
            // Atualizar o líder
            pthread_mutex_lock(&leader_mutex);
            current_leader_id = payload.sender_id; 
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

// Função que será executada por cada thread
void *handle_client(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;
    int received_code;

    if (recv(novo_socket, &received_code, sizeof(int), 0) <= 0) {
        perror("Erro ao receber o primeiro código");
        free(args);
        close(novo_socket);
        pthread_exit(NULL);
    }

    if (received_code >= CMD_ELECTION || received_code == CMD_WHO_IS_LEADER) {
        if (received_code == CMD_WHO_IS_LEADER) {
            handle_who_is_leader_query(novo_socket);
        } else {
            election_message_payload payload;
            payload.election_cmd_type = (election_command_type_t)received_code;

            if (recv(novo_socket, &payload, sizeof(election_message_payload), 0) <= 0) {
                perror("Erro ao receber payload da mensagem de eleição");
                free(args);
                close(novo_socket);
                pthread_exit(NULL);
            }
            handle_election_message(novo_socket, payload);
        }
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    } else {
        // Recebe o diretorio
        // Envia o nome do diretorio
        char diretorio[1000];
        int tamanho_dir = received_code;

        if (recv(novo_socket, diretorio, tamanho_dir, 0) <= 0) {
            perror("Erro ao receber o diretorio");
            free(args);
            close(novo_socket);
            pthread_exit(NULL);
        }
        diretorio[tamanho_dir] = '\0';

        int nDir = strlen(diretorio);
        if (!(diretorio[nDir - 1] == 'r' && diretorio[nDir - 2] == 'e' && diretorio[nDir - 3] == 'v'))
            strcat(diretorio, "_server");

        // Verifica se o diretório existe
        DIR *dir;
        dir = opendir(diretorio);
        if (dir == NULL) {
            char mkdirCommand[1500];
            snprintf(mkdirCommand, sizeof(mkdirCommand), "mkdir -p %s", diretorio);
            if (system(mkdirCommand) != 0){
                perror("Erro ao criar o diretório");
            }
            printf("Diretório criado com sucesso: %s\n", diretorio);

            // Aguarda 3 segundos
            sleep(3);
        } 
        closedir(dir);
        
        int codigo;
        if (recv(novo_socket, &codigo, sizeof(int), 0) <= 0) {
            perror("Erro ao receber o código da operação de arquivo");
            free(args);
            close(novo_socket);
            pthread_exit(NULL);
        }

        printf("Cliente conectado na thread %ld!\n", pthread_self());

        if (codigo == 1) {
            if (receiveNewFileFromClient(novo_socket, diretorio, &conflitOperation) == 1)
                printf("Erro ao receber o arquivo\n");
        } else if (codigo == 2) {
            if (removeFileInServer(novo_socket, diretorio, &conflitOperation) == 1)
                printf("Erro ao remover o arquivo\n");
        } else if (codigo == 3) {
            if (updateFileName(novo_socket, diretorio, &conflitOperation) == 1)
                printf("Erro ao atualizar o nome do arquivo\n");
        } else if (codigo == 4) {
            if (sendNewFileToClient(novo_socket, diretorio, &conflitOperation) == 1)
                printf("Erro ao atualizar o arquivo no cliente\n");
        } else if (codigo == 5) {
            if (sendLastSecondNotificationToClient(novo_socket, diretorio) == 1)
                printf("Erro ao enviar notificação para o cliente\n");
        }else if (codigo == 6) {
            if (sendFileListToClient(novo_socket, diretorio) == 1)
                printf("Erro ao enviar lista de arquivos para o cliente\n");
        }
        free(args);  // Liberar a memória alocada para os argumentos
        close(novo_socket);
        pthread_exit(NULL);
    }
}

int server_init(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    memset(&server_clients, 0, sizeof(client_list));
    pthread_mutex_init(&server_clients.mutex, NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Falha ao criar socket");
        return -1;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Falha ao configurar socket");
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Falha ao associar socket");
        return -1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("Falha ao escutar");
        return -1;
    }

    printf("Servidor iniciado na porta %d\n", port);
    return server_fd;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in client_address;  
    socklen_t client_addr_len = sizeof(client_address);

    if (argc != 3) {
        printf("Uso: %s <port> <my_rm_id>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    my_rm_id = atoi(argv[2]); 

    // Exemplo de inicialização do RM:
    // Isso precisa ser consistente em todas as instâncias do RM
    all_rms[0] = (rm_info){1, "127.0.0.1", 8080, 1};
    all_rms[1] = (rm_info){2, "127.0.0.1", 8081, 1};
    all_rms[2] = (rm_info){3, "127.0.0.1", 8082, 1};
    num_all_rms = 3;

    // Inicia heartbeat thread
    pthread_t hb_thread_id;
    if (pthread_create(&hb_thread_id, NULL, heartbeat_thread, NULL) != 0) {
        perror("Erro ao criar a thread de heartbeat");
        return 1;
    }
    pthread_detach(hb_thread_id);
  
    // Criação do socket
    int servidor_fd = server_init(port);
    if (servidor_fd < 0) {
        printf("Erro ao inicializar o servidor.\n");
        return 1;
    }

    while (1) {
        printf("Aguardando conexão...\n");
        int novo_socket = accept(servidor_fd, (struct sockaddr *)&client_address, (socklen_t *)&client_addr_len);
        if (novo_socket < 0) {
            perror("Erro no accept");
            continue;
        }

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->socket = novo_socket;
        strcpy(args->diretorio, "");

        // Criação da thread para lidar com o cliente
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)args) != 0) {
            perror("Erro ao criar a thread");
            free(args);
            close(novo_socket);
        }
        pthread_detach(thread_id);  // Libera recursos automaticamente ao término
    }

    close(servidor_fd);
    pthread_mutex_destroy(&server_clients.mutex);
    return 0;
}
