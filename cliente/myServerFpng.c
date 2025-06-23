#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORTA 8080
#define TAMANHO_BUFFER 1024

int main() {
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
    listen(servidor_fd, 3);
    printf("Aguardando conexão...\n");

    novo_socket = accept(servidor_fd, (struct sockaddr *)&endereco, (socklen_t *)&tamanho_endereco);
    printf("Conectado ao cliente!\n");

    while (1) {
        int tamanho_nome;

        // Recebe o nome do arquivo
        if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) break;
        recv(novo_socket, nome_arquivo, tamanho_nome, 0);
        nome_arquivo[tamanho_nome] = '\0';

        // Verifica se a conexão foi encerrada
        if (strcmp(nome_arquivo, "0") == 0) {
            printf("Conexão encerrada pelo cliente.\n");
            break;
        }

        printf("Nome do arquivo recebido: %s\n", nome_arquivo);

        // Caminho completo
        char caminho_completo[TAMANHO_BUFFER * 2];
        snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

        // Recebe o tamanho do arquivo
        long long tamanho_arquivo;
        recv(novo_socket, &tamanho_arquivo, sizeof(long long), 0);

        // Abre o arquivo para escrita binária
        arquivo = fopen(caminho_completo, "wb");
        if (!arquivo) {
            perror("Erro ao criar arquivo");
            continue;
        }

        // Recebe e grava o conteúdo do arquivo
        long long bytes_recebidos = 0;
        while (bytes_recebidos < tamanho_arquivo) {
            int bytes = recv(novo_socket, buffer, TAMANHO_BUFFER, 0);
            if (bytes <= 0) break;
            fwrite(buffer, 1, bytes, arquivo);
            bytes_recebidos += bytes;
        }

        printf("Arquivo '%s' recebido e salvo com sucesso!\n", nome_arquivo);
        fclose(arquivo);
    }

    close(novo_socket);
    close(servidor_fd);
    return 0;
}

