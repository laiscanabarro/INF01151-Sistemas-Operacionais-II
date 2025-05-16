#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "comunicationServer.h"
#define PORTA 8080
#define TAMANHO_BUFFER 1024

// Estrutura para passar parâmetros para a thread
typedef struct {
    int socket;
    char diretorio[TAMANHO_BUFFER];
} ThreadArgs;

// Função que será executada por cada thread
void *handle_client(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;
    char *diretorio = threadArgs->diretorio;
    int codigo;

    printf("Cliente conectado na thread %ld!\n", pthread_self());
    recv(novo_socket, &codigo, sizeof(int), 0);

    if (codigo == 1) {
        if (receiveNewFileFromClient(novo_socket, diretorio) == 1)
            printf("Erro ao receber o arquivo\n");
    } else if (codigo == 2) {
        if (removeFileInServer(novo_socket, diretorio) == 1)
            printf("Erro ao remover o arquivo\n");
    } else if (codigo == 3) {
        if (updateFileName(novo_socket, diretorio) == 1)
            printf("Erro ao atualizar o nome do arquivo\n");
    } else if (codigo == 4) {
        if (sendNewFileToClient(novo_socket, diretorio) == 1)
            printf("Erro ao atualizar o arquivo no cliente\n");
    } else if (codigo == 5) {
        if (sendLastSecondNotificationToClient(novo_socket, diretorio) == 1)
            printf("Erro ao enviar notificação para o cliente\n");
    }else if (codigo == 6) {
        if (sendFileListToClient(novo_socket,diretorio) == 1)
            printf("Erro ao enviar lista de arquivos para o cliente\n");
    }

    free(args);  // Liberar a memória alocada para os argumentos
    pthread_exit(NULL);
}

int main() {
    int servidor_fd, novo_socket;
    struct sockaddr_in endereco;
    int opt = 1;
    int tamanho_endereco = sizeof(endereco);
    char diretorio[TAMANHO_BUFFER];

    // Pergunta o diretório de trabalho do servidor
    printf("Digite o diretório para salvar os arquivos recebidos:\n");
    fgets(diretorio, TAMANHO_BUFFER, stdin);
    diretorio[strcspn(diretorio, "\n")] = '\0';
     //faca a logica aqui
    // Criação do socket
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    bind(servidor_fd, (struct sockaddr *)&endereco, sizeof(endereco));
    listen(servidor_fd, 3);

    while (1) {
        printf("Aguardando conexão...\n");
        novo_socket = accept(servidor_fd, (struct sockaddr *)&endereco, (socklen_t *)&tamanho_endereco);
        if (novo_socket < 0) {
            perror("Erro no accept");
            continue;
        }

        // Aloca argumentos para a thread
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->socket = novo_socket;
        strcpy(args->diretorio, diretorio);

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
    return 0;
}
