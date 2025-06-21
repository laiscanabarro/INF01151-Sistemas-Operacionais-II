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

// ---- [ELEIÇÃO DE LÍDER - início da seção] ----

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

// Função para iniciar o algoritmo de Bully
// Um RM chama esta função quando detecta uma falha do líder ou quando não há líder conhecido
void start_election() {
    pthread_mutex_lock(&election_mutex);    // Bloqueia para manipular as flags de eleição
    election_in_progress = 1;
    answer_received = 0;
    pthread_mutex_unlock(&election_mutex);  // Libera o mutex

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
        pthread_mutex_lock(&leader_mutex);      // Bloqueia para atualizar o líder
        current_leader_id = my_rm_id;
        pthread_mutex_unlock(&leader_mutex);    // Libera o mutex

        printf("Eu (RM %d) sou o novo líder!\n", my_rm_id);
        // Anuncia para todos os outros RMs que eu sou o coordenador
        for (int i = 0; i < num_all_rms; i++) {
            if (all_rms[i].id != my_rm_id) { // Não envia para si mesmo
                send_coordinator_message(all_rms[i].ip, all_rms[i].port, my_rm_id);
            }
        }
        // Eleição finalizada
        pthread_mutex_lock(&election_mutex);       // Bloqueia para atualizar a flag de eleição
        election_in_progress = 0; 
        pthread_mutex_unlock(&election_mutex);     // Libera o mutex

    } else {
        // Se encontrou RMs maiores, o processo atual espera por um ANSWER ou COORDINATOR
        // A lógica de timeout/recebimento é tratada em 'handle_client' e 'heartbeat_thread'
    }
}

// Thread para enviar e receber heartbeats
// Usado para a detecção de falhas do líder e para manter a coesão do cluster
void* heartbeat_thread(void* arg) {
    // Lógica de heartbeat:
    // Se for um líder: envia heartbeats para todos os backups
    // Se for um backup: espera heartbeats do primário. Se não receber, inicia eleição
    // Se for um backup: envia heartbeats para o primário
    while (1) {
        usleep(1000000);
        pthread_mutex_lock(&leader_mutex);      // Bloqueia para ler o ID do líder
        int leader = current_leader_id;
        pthread_mutex_unlock(&leader_mutex);    // Libera o mutex

        if (my_rm_id == leader) { // Sou o líder
            // Envia heartbeats para todos os backups (ou outros RMs)
            for (int i = 0; i < num_all_rms; i++) {
                if (all_rms[i].id != my_rm_id) { // Não sou eu e não é o líder
                    send_heartbeat_to_rm(all_rms[i].ip, all_rms[i].port, my_rm_id);
                }
            }
        } else { // Sou um backup
            // Tentar enviar heartbeat para o líder
            // Se o líder não responder após N tentativas, iniciar eleição
            rm_info leader_info;
            int found_leader = 0;
            for(int i = 0; i < num_all_rms; i++) {
                if(all_rms[i].id == leader) {
                    leader_info = all_rms[i];
                    found_leader = 1;
                    break;
                }
            }

            if (found_leader && leader_info.id != my_rm_id) { // Se o líder é conhecido e não sou eu
                // Tenta enviar um heartbeat para o líder. Se falhar, o líder pode ter caído
                if (send_heartbeat_to_rm(leader_info.ip, leader_info.port, my_rm_id) < 0) {
                    // Falha no heartbeat para o líder, então iniciar eleição
                    pthread_mutex_lock(&election_mutex);        // Bloqueia para verificar/iniciar eleição
                    if (!election_in_progress) {                // Se não houver eleição em andamento, inicie uma
                        printf("Líder %d falhou! Iniciando eleição...\n", leader);
                        election_in_progress = 1;
                        answer_received = 0; 
                        pthread_mutex_unlock(&election_mutex);  // Libera o mutex antes de chamar start_election
                        start_election();                       // Inicia o processo de eleição
                    } else {
                        pthread_mutex_unlock(&election_mutex);  // Libera o mutex se já houver eleição
                    }
                }
            } else if (leader == -1 || leader == my_rm_id) {
                // Se não há líder conhecido ou se eu sou o líder (mas não me enviei heartbeat)
                // Isto é para cenários onde um RM se inicia ou detecta que não há líder
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

// Função para lidar com mensagens de eleição recebidas de outros RMs 
void handle_election_message(int novo_socket, election_message_payload payload) {
    pthread_mutex_lock(&election_mutex);        // Bloqueia para manipular o estado da eleição
    printf("RM %d recebeu mensagem %d do RM %d\n", my_rm_id, payload.election_cmd_type, payload.sender_id);

    switch (payload.election_cmd_type) {
        case CMD_ELECTION:      // Recebeu uma mensagem de ELECTION de um RM com ID menor 
            // Envia uma resposta se meu ID for maior
            if (my_rm_id > payload.sender_id) {
                send_answer_message(all_rms[payload.sender_id -1].ip, all_rms[payload.sender_id -1].port, my_rm_id);
                // Se eu ainda não estivesse em uma eleição, comece uma
                if (!election_in_progress) {
                    election_in_progress = 1;
                    answer_received = 0; 
                    pthread_mutex_unlock(&election_mutex); // Libera o mutex antes de chamar start_election
                    start_election();                      // Inicia a própria eleição
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
            pthread_mutex_lock(&leader_mutex);      // Bloqueia para atualizar o líder
            current_leader_id = payload.leader_id;
            pthread_mutex_unlock(&leader_mutex);    // Libera o mutex
            election_in_progress = 0;               // Eleição terminada
            answer_received = 0;                    // Reseta a flag de resposta
            printf("RM %d: O novo líder é o RM %d.\n", my_rm_id, current_leader_id);
            break;
        case CMD_HEARTBEAT:    // Recebeu um heartbeat do líder atual
            // Atualizar o líder
            pthread_mutex_lock(&leader_mutex);     // Bloqueia para atualizar o líder
            current_leader_id = payload.sender_id; 
            pthread_mutex_unlock(&leader_mutex);   // Libera o mutex
            election_in_progress = 0;              // Se um heartbeat foi recebido, não há necessidade de eleição
            answer_received = 0;
            break;
        default:
            printf("RM %d: Mensagem de eleição desconhecida: %d\n", my_rm_id, payload.election_cmd_type);
            break;
    }
    pthread_mutex_unlock(&election_mutex);
}

// Função que será executada por cada thread conectada ao servidor RM
void *handle_client(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;
    int received_code;

    // Primeiro, recebe o código do comando inicial
    // Este código indicará se é uma mensagem de eleição ou uma operação de arquivo
    if (recv(novo_socket, &received_code, sizeof(int), 0) <= 0) {
        perror("Erro ao receber o primeiro código");
        free(args);
        close(novo_socket);
        pthread_exit(NULL);
    }

    // [ELEIÇÃO DE LÍDER - INÍCIO LÓGICA DE MANUSEIO DE MENSAGENS]

    // Verifica se o código recebido indica uma mensagem relacionada à eleição (com payload)
    // ou se é uma consulta de "quem é o líder"
    if ((received_code >= CMD_ELECTION && received_code <= CMD_HEARTBEAT) || received_code == CMD_WHO_IS_LEADER) {
        if (received_code == CMD_WHO_IS_LEADER) {   // Caso seja uma consulta do cliente sobre o líder 
            // Função que lida com a pergunta do cliente "quem é o líder?"
            handle_who_is_leader_query(novo_socket);
        } else {
            election_message_payload payload;

            // O tipo de comando de eleição já foi recebido em 'received_code'
            // Define o tipo na estrutura de payload local
            payload.election_cmd_type = (election_command_type_t)received_code;

            // Agora, recebe APENAS o restante do payload (sender_id e leader_id). 
            // Isso assume que election_command_type_t tem o mesmo tamanho de 'int'
            // e que ele é o primeiro membro da estrutura election_message_payload
            size_t size_of_cmd_type = sizeof(int); 
            size_t remaining_payload_size = sizeof(election_message_payload) - size_of_cmd_type;

            // Recebe os bytes restantes diretamente na memória logo após o campo 'election_cmd_type'.
            if (recv(novo_socket, ((char*)&payload) + size_of_cmd_type, remaining_payload_size, 0) <= 0) {
                perror("Erro ao receber o restante do payload da mensagem de eleição");
                free(args);
                close(novo_socket);
                pthread_exit(NULL);
            }
            // Delega o manuseio da mensagem de eleição para a função handle_election_message
            handle_election_message(novo_socket, payload);
        }
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    // [ELEIÇÃO DE LÍDER - FIM LÓGICA DE MANUSEIO DE MENSAGENS]
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

    // [ELEIÇÃO DE LÍDER - INICIALIZAÇÃO]
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

    // [ELEIÇÃO DE LÍDER - FIM DA INICIALIZAÇÃO]
  
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
