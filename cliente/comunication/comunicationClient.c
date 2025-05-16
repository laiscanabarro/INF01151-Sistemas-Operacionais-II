#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "comunicationClient.h"
int sendNewFileToServer(char *nomeArquivo,char *diretorio,int PORTA,char * IP){
    int sock;
    struct sockaddr_in endereco;
    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);
    // Caminho completo
    char caminho_completo[1024*2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nomeArquivo);
    char buffer[1024];
    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo=1;
    send(sock,&codigo,sizeof(int),0);
    printf("Conectado ao servidor!\n");
   
    FILE *arquivo = fopen(caminho_completo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo");
        return 1;
    }
    // Envia o nome do arquivo
    int tamanho_nome = strlen(nomeArquivo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeArquivo, tamanho_nome, 0);
    
    // Tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long long tamanho_arquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    send(sock, &tamanho_arquivo, sizeof(long long), 0);
  
    // Envia o conteúdo
    int bytes_lidos;
    while ((bytes_lidos = fread(buffer, 1, 1024, arquivo)) > 0) {
        send(sock, buffer, bytes_lidos, 0);
    }

    fclose(arquivo);
    close(sock);
    
    return 0;


}

int removeFileInServer(char *nomeArquivo,char *diretorio,int PORTA,char * IP){
    int sock;
    struct sockaddr_in endereco;
    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);
    // Caminho completo
    char caminho_completo[1024*2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nomeArquivo);
    char buffer[1024];
    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo=2;
    send(sock,&codigo,sizeof(int),0);
    printf("Conectado ao servidor!\n");
    // Envia o nome do arquivo
    int tamanho_nome = strlen(nomeArquivo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeArquivo, tamanho_nome, 0);
    close(sock);
    
    return 0;





}
int updateFileName(char *nomeNovo, char *nomeAntigo, char *diretorio, int PORTA, char *IP) {
    int sock;
    struct sockaddr_in endereco;

    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);

    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo = 3;  // Novo código para "renomear arquivo"
    send(sock, &codigo, sizeof(int), 0);
    printf("Conectado ao servidor!\n");

    // Envia o nome antigo do arquivo
    int tamanho_nome = strlen(nomeAntigo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeAntigo, tamanho_nome, 0);

    // Envia o nome novo do arquivo
    tamanho_nome = strlen(nomeNovo);  // Corrigido
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeNovo, tamanho_nome, 0);

    close(sock);
    return 0;
}

int receiveNewFileFromServer(char *nomeArquivo, char *diretorio, int PORTA, char *IP) {
    int sock;
    struct sockaddr_in endereco;

    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);

    connect(sock, (struct sockaddr *)&endereco, sizeof(endereco));
    int codigo = 4;
    send(sock, &codigo, sizeof(int), 0);

    // envia o nome do arquivo solicitado
    int tamanho_nome=strlen(nomeArquivo);
    send(sock, &tamanho_nome, sizeof(int), 0);
    send(sock, nomeArquivo, tamanho_nome, 0);
    
    
    // Recebe o conteúdo
    char caminho_completo[1024 * 2];
    
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nomeArquivo);
   
    FILE *arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo");
        return 1;
    }
    // Recebe o tamanho do arquivo
    long long tamanho_arquivo;
    recv(sock, &tamanho_arquivo, sizeof(long long), 0);
     

    // Recebe e grava o conteúdo do arquivo
    char buffer[1024];
    long long bytes_recebidos = 0;
    while (bytes_recebidos < tamanho_arquivo) {
        int bytes = recv(sock, buffer, 1024, 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, arquivo);
        bytes_recebidos += bytes;
    }
    
    fclose(arquivo);
    close(sock);
    return 0;

    
    
    
}
int receiveLastSecondNotificationFromServer(notification_t *notification, int PORTA, char *IP) {
    int sock;
    struct sockaddr_in endereco;

    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);

    // Conectando ao servidor
    if (connect(sock, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro ao conectar ao servidor");
        close(sock);
        return 0;
    }

    // Código da operação para notificações (por exemplo, 5)
    int codigo = 5;
    if (send(sock, &codigo, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar código para o servidor");
        close(sock);
        return 1;
    }

    // Receber o número de notificações
    int num_notifications;
    int bytes_recebidos = recv(sock, &num_notifications, sizeof(int), 0);
    if (bytes_recebidos <= 0) {
        perror("Erro ao receber número de notificações");
        close(sock);
        return 1;
    }

    // Verificar se não há notificações
    if (num_notifications == 0) {
        printf("Sem notificações no último segundo.\n");
        close(sock);
        return 0;
    }

    // Receber cada notificação
    for (int i = 0; i < num_notifications; i++) {
        bytes_recebidos = recv(sock, &notification[i], sizeof(notification_t), 0);
        if (bytes_recebidos <= 0) {
            perror("Erro ao receber notificação");
            close(sock);
            return 1;
        }
    }

    printf("Recebidas %d notificações do servidor.\n", num_notifications);
    close(sock);
    return num_notifications;
}

/*
int  receiveLastSessionNotificationFromServer(notification_t *notification,int PORTA,char *IP){
    int sock;
    struct sockaddr_in endereco;

    // Conexão ao servidor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORTA);
    inet_pton(AF_INET, IP, &endereco.sin_addr);

        // Conectando ao servidor
    if (connect(sock, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro ao conectar ao servidor");
        close(sock);
        return 1;
    }

    // Código da operação para notificações (por exemplo, 4)
    int codigo = 6;
    send(sock, &codigo, sizeof(int), 0);

    // Recebendo a estrutura de notificação
    int bytes_recebidos = recv(sock, notification, sizeof(notification_t), 0);
    if (bytes_recebidos <= 0) {
        perror("Erro ao receber notificação");
        close(sock);
        return 1;
    }


    close(sock);
    return 0;

}*/
