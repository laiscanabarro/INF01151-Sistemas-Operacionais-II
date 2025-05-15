#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "comunicationServer.h"
#define PORTA 8080
#define TAMANHO_BUFFER 1024


int main() {
    long long timeInicio=1747263784;
    int servidor_fd, novo_socket;
    struct sockaddr_in endereco;
    int opt = 1;
    int tamanho_endereco = sizeof(endereco);
    char buffer[TAMANHO_BUFFER];
    char diretorio[TAMANHO_BUFFER];
    char nome_arquivo[TAMANHO_BUFFER];
    FILE *arquivo;

    // Pergunta o diretório de trabalho do servidor
    printf("Digite o diretório para salvar os arquivos recebidos:\n");
    fgets(diretorio, TAMANHO_BUFFER, stdin);
    diretorio[strcspn(diretorio, "\n")] = '\0';

    // Criação do socket
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    bind(servidor_fd, (struct sockaddr *)&endereco, sizeof(endereco));
    while(1){
    listen(servidor_fd, 3);
    printf("Aguardando conexão...\n");

    novo_socket = accept(servidor_fd, (struct sockaddr *)&endereco, (socklen_t *)&tamanho_endereco);
    printf("Conectado ao cliente!\n");

    int codigo;
    recv(novo_socket, &codigo, sizeof(int), 0);
    
    if(codigo==1)
    {if(receiveNewFileFromClient(novo_socket,diretorio)==1)
    printf("erro ao receber o arquivo");}
    if(codigo==2)
    {if(removeFileInServer(novo_socket,diretorio)==1)
    printf("erro ao remover o arquivo");}
    if(codigo==3)
    {if(updateFileName(novo_socket,diretorio)==1)
    printf("erro ao atualizar o nome do arquivo");}
    if(codigo==4)
    {if(sendNewFileToClient(novo_socket,diretorio)==1)
    printf("erro ao atualizar o  arquivo no cliente");}
    if(codigo==5)
    {if(sendLastSecondNotificationToClient(novo_socket,diretorio)==1)
    printf("erro ao enviar notificacao para o cliente");}
    
    }

    close(servidor_fd);
    return 0;
}

