#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h> 
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
   // printf("Conectado ao servidor!\n");
   
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
   // printf("Conectado ao servidor!\n");
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
   // printf("Conectado ao servidor!\n");

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
    //sleep(15);
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
      //  printf("Sem notificações no último segundo.\n");
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

  //  printf("Recebidas %d notificações do servidor.\n", num_notifications);
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

int receiveFileListFromServer(char ***arquivosServidor,int  PORTA,char * IP){
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
    int codigo = 6;
    if (send(sock, &codigo, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar código para o servidor");
        close(sock);
        return 1;
    }
    int nArquivos=0;
    if (recv(sock, &nArquivos, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar código para o servidor");
        close(sock);
        return 1;
    }
    (*arquivosServidor)=malloc(nArquivos*sizeof(char *));
    int len;
    for(int i=0;i<nArquivos;i++){
        recv(sock, &len, sizeof(int), 0);
        (*arquivosServidor)[i]=malloc(len*sizeof(char));
        recv(sock,(*arquivosServidor)[i],len*sizeof(char),0);

    }
   
    
    return nArquivos;
    



}

int receiveLastSecondLocalNotification(notification_t *notifications, char *diretorio) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    time_t now = time(NULL);
    int notification_count = 0;

    // Variáveis estáticas para armazenar o estado anterior e as notificações do último segundo
    static struct stat previous_stats[1024];
    static char previous_names[1024][200];
    static int previous_count = 0;
    static int initialized = 0;

    // Cache das notificações do último segundo
    static notification_t last_notifications[300];
    static int last_notification_count = 0;
    static time_t last_second = 0;

    // Verificar se estamos no mesmo segundo da última chamada
    if (now == last_second) {
        // Retornar as notificações previamente calculadas
        memcpy(notifications, last_notifications, last_notification_count * sizeof(notification_t));
        return last_notification_count;
    }

    // Atualizar o segundo atual
    last_second = now;
    last_notification_count = 0;

    // Tabelas para marcar arquivos já encontrados na iteração atual
    int found_in_iteration[1024] = {0};

    // Abrir o diretório para leitura
    dir = opendir(diretorio);
    if (!dir) {
        perror("Erro ao abrir o diretório");
        return 1;
    }

    // Iterar sobre os arquivos do diretório
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Montar o caminho completo do arquivo
        snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);

        // Obter informações do arquivo
        if (stat(full_path, &file_stat) == -1) {
            perror("Erro ao obter informações do arquivo");
            continue;
        }

        int is_new = 1;
        for (int i = 0; i < previous_count; i++) {
            // Comparação pelo inode para garantir que o arquivo é o mesmo
            if (file_stat.st_ino == previous_stats[i].st_ino) {
                is_new = 0;
                found_in_iteration[i] = 1;

                // Verifica se o nome mudou (renomeação)
                if (strcmp(previous_names[i], entry->d_name) != 0) {
                    strncpy(last_notifications[last_notification_count].fileName, entry->d_name, sizeof(last_notifications[last_notification_count].fileName));
                    strncpy(last_notifications[last_notification_count].ancientFileName, previous_names[i], sizeof(last_notifications[last_notification_count].ancientFileName));
                    last_notifications[last_notification_count].type = RENAMED_FILE;
                    last_notification_count++;

                    // Atualiza o nome armazenado para refletir a mudança
                    strncpy(previous_names[i], entry->d_name, sizeof(previous_names[i]));
                }

                // Verifica se foi atualizado recentemente
                if (difftime(now, file_stat.st_mtime) <= 1) {
                    strncpy(last_notifications[last_notification_count].fileName, entry->d_name, sizeof(last_notifications[last_notification_count].fileName));
                    last_notifications[last_notification_count].type = UPDATED_FILE;
                    last_notification_count++;
                }
                break;
            }
        }

        // Arquivo novo (não encontrado na lista anterior)
        if (is_new) {
            strncpy(last_notifications[last_notification_count].fileName, entry->d_name, sizeof(last_notifications[last_notification_count].fileName));
            last_notifications[last_notification_count].type = UPDATED_FILE;
            last_notification_count++;
        }
    }
    closedir(dir);

    // Verificar arquivos que foram removidos desde a última chamada
    for (int i = 0; i < previous_count; i++) {
        if (!found_in_iteration[i]) {
            strncpy(last_notifications[last_notification_count].fileName, previous_names[i], sizeof(last_notifications[last_notification_count].fileName));
            last_notifications[last_notification_count].type = REMOVED_FILE;
            last_notification_count++;
        }
    }

    // Atualizar a lista de arquivos e inodes para futuras comparações
    dir = opendir(diretorio);
    previous_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." e ".." novamente ao atualizar a lista de arquivos anteriores
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);
        if (stat(full_path, &file_stat) == -1) continue;

        previous_stats[previous_count] = file_stat;
        strncpy(previous_names[previous_count], entry->d_name, sizeof(previous_names[previous_count]));
        previous_count++;
    }
    closedir(dir);

    // Marcar como inicializado após a primeira leitura completa
    if (!initialized) {
        initialized = 1;
        return 0;
    }

    // Filtrar notificações duplicadas
    filterNotifications(last_notifications, &last_notification_count);

    // Copiar as notificações filtradas para a saída
    memcpy(notifications, last_notifications, last_notification_count * sizeof(notification_t));
    return last_notification_count;
}

void filterNotifications(notification_t *notifications, int *num_notifications) {
    for (int i = 0; i < *num_notifications; i++) {
        if (notifications[i].type == UPDATED_FILE) {
            for (int j = i + 1; j < *num_notifications; j++) {
                if (strcmp(notifications[i].fileName, notifications[j].fileName) == 0) {
                    for (int k = j; k < *num_notifications - 1; k++) {
                        notifications[k] = notifications[k + 1];
                    }
                    (*num_notifications)--;
                    j--; 
                }
            }
        }
    }
}
