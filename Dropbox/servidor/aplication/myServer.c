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

// Função que será executada por cada thread
void *handle_client(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;

    // Recebe o diretorio
    // Envia o nome do diretorio
    char diretorio[1000];
    int tamanho_dir;
    recv(novo_socket, &tamanho_dir, sizeof(int), 0);
    recv(novo_socket, diretorio, tamanho_dir, 0);

    int nDir=strlen(diretorio);
    if(!(diretorio[nDir-1]=='r'&&diretorio[nDir-2]=='e'&&diretorio[nDir-3]=='v'))
        strcat(diretorio,"_server");

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

    printf("Cliente conectado na thread %ld!\n", pthread_self());
    recv(novo_socket, &codigo, sizeof(int), 0);

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
    pthread_exit(NULL);
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

    if (argc != 2) {
        printf("Uso: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
  
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
