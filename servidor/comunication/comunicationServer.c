#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "comunicationServer.h"
int receiveNewFileFromClient(int novo_socket,char *diretorio){
    
    int tamanho_nome;
    char nome_arquivo[1024];
    char buffer[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';
    // Verifica se a conexão foi encerrada
    if (strcmp(nome_arquivo, "0") == 0) {
        printf("Conexão encerrada pelo cliente.\n");
        return 1;
    }
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);
    // Recebe o tamanho do arquivo
    long long tamanho_arquivo;
    recv(novo_socket, &tamanho_arquivo, sizeof(long long), 0);
    FILE *arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo");
        return 1;
    }
    // Recebe e grava o conteúdo do arquivo
    long long bytes_recebidos = 0;
    while (bytes_recebidos < tamanho_arquivo) {
        int bytes = recv(novo_socket, buffer, 1024, 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, arquivo);
        bytes_recebidos += bytes;
    }
    
    fclose(arquivo);
    close(novo_socket);



}

int removeFileInServer(int novo_socket, char *diretorio) {
    
    int tamanho_nome;
    char nome_arquivo[1024];
    
    // Recebe o nome do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';

    
    
    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

    // Tenta remover o arquivo
    if (remove(caminho_completo) == 0) {
        printf("Arquivo '%s' removido com sucesso!\n", nome_arquivo);
    } else {
        perror("Erro ao remover o arquivo");
        return 1;
    }

    close(novo_socket);
    return 0;
}

int updateFileName(int novo_socket, char *diretorio) {
    int tamanho_nome;
    char nome_antigo[1024];
    char nome_novo[1024];

    // Recebe o nome antigo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_antigo, tamanho_nome, 0);
    nome_antigo[tamanho_nome] = '\0';

    // Caminho completo antigo
    char caminho_completo_antigo[1024 * 2];
    snprintf(caminho_completo_antigo, sizeof(caminho_completo_antigo), "%s/%s", diretorio, nome_antigo);

    // Recebe o nome novo do arquivo
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_novo, tamanho_nome, 0);
    nome_novo[tamanho_nome] = '\0';

    // Caminho completo novo
    char caminho_completo_novo[1024 * 2];
    snprintf(caminho_completo_novo, sizeof(caminho_completo_novo), "%s/%s", diretorio, nome_novo);  // Corrigido

    // Renomeia o arquivo
    if (rename(caminho_completo_antigo, caminho_completo_novo) != 0) {
        perror("Erro ao renomear arquivo");
        return 1;
    } else {
        printf("Arquivo '%s' renomeado para '%s' com sucesso!\n", nome_antigo, nome_novo);
    }

    close(novo_socket);
    return 0;
}

int sendNewFileToClient(int novo_socket, char *diretorio) {
    int tamanho_nome;
    char nome_arquivo[1024];
    char buffer[1024];
    // Recebe o nome do arquivo solicitado
    if (recv(novo_socket, &tamanho_nome, sizeof(int), 0) <= 0) 
        return 1;
    recv(novo_socket, nome_arquivo, tamanho_nome, 0);
    nome_arquivo[tamanho_nome] = '\0';

    // Caminho completo
    char caminho_completo[1024 * 2];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s/%s", diretorio, nome_arquivo);

    FILE *arquivo = fopen(caminho_completo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    // Tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long long tamanho_arquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    send(novo_socket, &tamanho_arquivo, sizeof(long long), 0);
  
    // Envia o conteúdo
    int bytes_lidos;
    while ((bytes_lidos = fread(buffer, 1, 1024, arquivo)) > 0) {
        send(novo_socket, buffer, bytes_lidos, 0);
    }

    fclose(arquivo);
    close(novo_socket);
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>



#define MAX_NOTIFICATIONS 1024
int sendLastSecondNotificationToClient(int novo_socket, char *diretorio) {
    notification_t notifications[MAX_NOTIFICATIONS];
    int num_notifications = receiveLastSecondLocalNotification(notifications, diretorio);

    // Verifica se houve erro ao obter notificações
    if (num_notifications < 0) {
        perror("Erro ao obter notificações do último segundo");
        close(novo_socket);
        return 1;
    }

    // Enviar o número de notificações para o cliente
    if (send(novo_socket, &num_notifications, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar número de notificações");
        close(novo_socket);
        return 1;
    }

    // Enviar cada notificação para o cliente
    for (int i = 0; i < num_notifications; i++) {
        if (send(novo_socket, &notifications[i], sizeof(notification_t), 0) <= 0) {
            perror("Erro ao enviar notificação");
            close(novo_socket);
            return 1;
        }
    }

    printf("Notificações do último segundo enviadas com sucesso!\n");
    close(novo_socket);
    return 0;
}

/*
int sendLastSessionNotificationToClient(int novo_socket, char *diretorio, time_t timeLastSession) {
    notification_t notifications[MAX_NOTIFICATIONS];
    int num_notifications = receiveLastSessionLocalNotification(notifications, diretorio, timeLastSession);

    // Verifica se houve erro ao obter notificações
    if (num_notifications < 0) {
        perror("Erro ao obter notificações da última sessão");
        close(novo_socket);
        return 1;
    }

    // Enviar o número de notificações para o cliente
    if (send(novo_socket, &num_notifications, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar número de notificações");
        close(novo_socket);
        return 1;
    }

    // Enviar cada notificação para o cliente
    for (int i = 0; i < num_notifications; i++) {
        if (send(novo_socket, &notifications[i], sizeof(notification_t), 0) <= 0) {
            perror("Erro ao enviar notificação");
            close(novo_socket);
            return 1;
        }
    }

    printf("Notificações desde a última sessão enviadas com sucesso!\n");
    close(novo_socket);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
*/
#define MAX_NOTIFICATIONS 1024
/*
// Função para receber notificações locais da última sessão
int receiveLastSessionLocalNotification(notification_t *notifications, char *diretorio, time_t timeLastSession) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    int notification_count = 0;

    // Variáveis estáticas para armazenar o estado anterior
    static struct stat previous_stats[1024];
    static char previous_names[1024][200];
    static int previous_count = 0;

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

        // Verificar se o arquivo é novo ou atualizado desde a última sessão
        int found = 0;
        for (int i = 0; i < previous_count; i++) {
            if (file_stat.st_ino == previous_stats[i].st_ino) {
                found = 1;

                // Verificar se o nome foi alterado
                if (strcmp(entry->d_name, previous_names[i]) != 0) {
                    strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
                    strncpy(notifications[notification_count].ancientFileName, previous_names[i], sizeof(notifications[notification_count].ancientFileName));
                    notifications[notification_count].type = RENAMED_FILE;
                    notification_count++;
                }
                
                // Verificar se o arquivo foi atualizado
                if (file_stat.st_mtime > timeLastSession) {
                    strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
                    notifications[notification_count].type = UPDATED_FILE;
                    notification_count++;
                }
                break;
            }
        }

        // Arquivo novo
        if (!found) {
            strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
            notifications[notification_count].type = UPDATED_FILE;
            notification_count++;
        }
    }
    closedir(dir);

    // Verificar arquivos removidos desde a última sessão
    for (int i = 0; i < previous_count; i++) {
        int found = 0;
        dir = opendir(diretorio);

        while ((entry = readdir(dir)) != NULL) {
            snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);
            if (stat(full_path, &file_stat) == -1) continue;

            // Verifica se o arquivo ainda existe e se o inode é o mesmo
            if (file_stat.st_ino == previous_stats[i].st_ino) {
                found = 1;
                break;
            }
        }
        closedir(dir);

        // Arquivo não encontrado: foi removido
        if (!found) {
            strncpy(notifications[notification_count].fileName, previous_names[i], sizeof(notifications[notification_count].fileName));
            notifications[notification_count].type = REMOVED_FILE;
            notification_count++;
        }
    }

    // Atualizar o estado anterior com os dados atuais
    dir = opendir(diretorio);
    previous_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", diretorio, entry->d_name);
        if (stat(full_path, &file_stat) == -1) continue;

        previous_stats[previous_count] = file_stat;
        strncpy(previous_names[previous_count], entry->d_name, sizeof(previous_names[previous_count]));
        previous_count++;
    }
    closedir(dir);

    return notification_count;
}*/

// Função para receber notificações locais
int receiveLastSecondLocalNotification(notification_t *notifications, char *diretorio) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    time_t now = time(NULL);
    int notification_count = 0;

    // Variáveis estáticas para armazenar o estado anterior
    static struct stat previous_stats[1024];
    static char previous_names[1024][200];
    static int previous_count = 0;
    static int initialized = 0;

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
                if (strcmp(previous_names[i], entry->d_name) != 0 && initialized) {
                    strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
                    strncpy(notifications[notification_count].ancientFileName, previous_names[i], sizeof(notifications[notification_count].ancientFileName));
                    notifications[notification_count].type = RENAMED_FILE;
                    notification_count++;

                    // Atualiza o nome armazenado para evitar duplicidade
                    strncpy(previous_names[i], entry->d_name, sizeof(previous_names[i]));
                }

                // Verifica se foi atualizado recentemente e evita duplicidade
                if (difftime(now, file_stat.st_mtime) <= 1 && initialized) {
                    // Evita enviar a notificação de atualização duas vezes
                    int already_notified = 0;
                    for (int j = 0; j < notification_count; j++) {
                        if (strcmp(notifications[j].fileName, entry->d_name) == 0 && notifications[j].type == UPDATED_FILE) {
                            already_notified = 1;
                            break;
                        }
                    }
                    if (!already_notified) {
                        strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
                        notifications[notification_count].type = UPDATED_FILE;
                        notification_count++;
                    }
                }
                break;
            }
        }

        // Arquivo novo (não encontrado na lista anterior)
        if (is_new && initialized) {
            strncpy(notifications[notification_count].fileName, entry->d_name, sizeof(notifications[notification_count].fileName));
            notifications[notification_count].type = UPDATED_FILE;
            notification_count++;
        }
    }
    closedir(dir);

    // Verificar arquivos que foram removidos desde a última chamada
    for (int i = 0; i < previous_count; i++) {
        if (!found_in_iteration[i] && initialized) {
            strncpy(notifications[notification_count].fileName, previous_names[i], sizeof(notifications[notification_count].fileName));
            notifications[notification_count].type = REMOVED_FILE;
            notification_count++;
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

    filterNotifications(notifications,&notification_count);
    return notification_count;
}
void filterNotifications(notification_t *notifications, int *num_notifications) {
    for (int i = 0; i < *num_notifications; i++) {
        // Se a notificação atual é de atualização (enum 0)
        if (notifications[i].type == UPDATED_FILE) {
            for (int j = i + 1; j < *num_notifications; j++) {
                // Verifica se o nome do arquivo é o mesmo
                if (strcmp(notifications[i].fileName, notifications[j].fileName) == 0) {
                    // Remove a notificação duplicada (movendo os elementos restantes para trás)
                    for (int k = j; k < *num_notifications - 1; k++) {
                        notifications[k] = notifications[k + 1];
                    }
                    (*num_notifications)--;
                    j--; // Ajusta o índice para continuar verificando
                }
            }
        }
    }
}


int sendFileListToClient(int novo_socket, char *diretorio){
    int nArquivos=0;
    char **arquivos;
    // Chamada da função para obter a lista de arquivos
    obterListaArquivos(diretorio, &arquivos, &nArquivos);

    // Verificar se a lista foi preenchida corretamente
    if (arquivos == NULL) {
        printf("Erro ao obter a lista de arquivos\n");
        return -1;
    }
    if (send(novo_socket, &nArquivos, sizeof(int), 0) <= 0) {
        perror("Erro ao enviar numero de arquivos");
        close(novo_socket);
        return 1;
    }
    for (int i = 0; i < nArquivos; i++)
    {
        int len = strlen(arquivos[i]) + 1; // +1 para incluir o '\0'

        // Envia o tamanho do nome do arquivo
        if (send(novo_socket, &len, sizeof(int), 0) <= 0)
        {
            perror("Erro ao enviar tamanho do arquivo");
            close(novo_socket);
            return 1;
        }

        // Envia o nome do arquivo
        if (send(novo_socket, arquivos[i], len, 0) <= 0)
        {
            perror("Erro ao enviar nome do arquivo");
            close(novo_socket);
            return 1;
        }
    }
    for (int i = 0; i < nArquivos; i++)
    {
        free(arquivos[i]);
    }
    free(arquivos);
}
void obterListaArquivos(char *diretorio, char ***arquivos, int *nArquivos) {
    DIR *dir;
    struct dirent *entry;
    int capacidade = 100;  // Capacidade inicial

    // Alocar memória para a lista de arquivos
    *arquivos = malloc(capacidade * sizeof(char *));
    if (*arquivos == NULL) {
        perror("Erro ao alocar memória para a lista de arquivos");
        return;
    }

    *nArquivos = 0;

    // Abrir o diretório especificado
    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretório");
        free(*arquivos);
        *arquivos = NULL;
        return;
    }

    // Percorrer os arquivos no diretório
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Realocar memória se necessário
        if (*nArquivos >= capacidade) {
            capacidade *= 2;
            char **temp = realloc(*arquivos, capacidade * sizeof(char *));
            if (temp == NULL) {
                perror("Erro ao realocar memória para a lista de arquivos");
                closedir(dir);
                free(*arquivos);
                *arquivos = NULL;
                return;
            }
            *arquivos = temp;
        }

        // Alocar memória para o nome do arquivo e armazenar
        (*arquivos)[*nArquivos] = strdup(entry->d_name);
        if ((*arquivos)[*nArquivos] == NULL) {
            perror("Erro ao alocar memória para o nome do arquivo");
            closedir(dir);
            free(*arquivos);
            *arquivos = NULL;
            return;
        }

        (*nArquivos)++;
    }
    closedir(dir);
}















