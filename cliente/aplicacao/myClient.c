#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <arpa/inet.h>
#include "../comunication/comunicationClient.h"

#define MAX_COMMAND_SIZE 1024
#define MAX_PATH_SIZE 1024

typedef struct {
    char username[50];
    char sync_dir_path[MAX_PATH_SIZE];
    char server_ip[16];
    int server_port;
    int running;
} client_info_t;

client_info_t client_info;

// Função para verificar se o diretório existe, se não, criar
int check_and_create_directory(const char *path) {
    DIR *dir = opendir(path);
    if (dir) {
        closedir(dir);
        printf("Diretório %s já existe.\n", path);
        return 0;
    } else {
        if (mkdir(path, 0755) == 0) {
            printf("Diretório %s criado com sucesso.\n", path);
            return 0;
        } else {
            perror("Erro ao criar diretório");
            return 1;
        }
    }
}

// Função para verificar/criar o diretório do cliente
int setup_client_directory(char *username) {
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        fprintf(stderr, "Não foi possível obter o diretório home do usuário.\n");
        return 1;
    }
    
    char env_var_name[100];
    snprintf(env_var_name, sizeof(env_var_name), "SYNC_DIR_%s", username);
    
    char *env_dir = getenv(env_var_name);
    if (env_dir) {
        strncpy(client_info.sync_dir_path, env_dir, MAX_PATH_SIZE);
        printf("Usando diretório de sincronização definido na variável de ambiente: %s\n", client_info.sync_dir_path);
    } else {
        snprintf(client_info.sync_dir_path, MAX_PATH_SIZE, "%s/sync_dir_%s", home_dir, username);
        printf("Diretório de sincronização não definido em variável de ambiente.\n");
        printf("Usando diretório padrão: %s\n", client_info.sync_dir_path);
    }
    
    return check_and_create_directory(client_info.sync_dir_path);
}

// Função para listar arquivos no diretório do cliente
void list_client_files() {
    DIR *dir;
    struct dirent *entry;
    
    printf("\n--- Arquivos no diretório do cliente ---\n");
    
    dir = opendir(client_info.sync_dir_path);
    if (!dir) {
        perror("Erro ao abrir diretório");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Ignora . e ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
            
        printf("%s\n", entry->d_name);
    }
    
    closedir(dir);
}

// Função para listar arquivos no servidor
void list_server_files() {
    printf("\n--- Solicitando lista de arquivos do servidor ---\n");
    
    char **arquivos_servidor = NULL;
    int num_arquivos = receiveFileListFromServer(&arquivos_servidor, client_info.server_port, client_info.server_ip);
    
    if (num_arquivos > 0) {
        printf("\n--- Arquivos no servidor ---\n");
        printf("%-30s\n", "Nome");
        printf("%-30s\n", "------------------------------");
        
        for (int i = 0; i < num_arquivos; i++) {
            printf("%-30s\n", arquivos_servidor[i]);
            free(arquivos_servidor[i]);
        }
        free(arquivos_servidor);
    } else {
        printf("Nenhum arquivo encontrado no servidor ou erro na comunicação.\n");
    }
}

// Função para fazer upload de um arquivo
void upload_file(const char *path) {
    printf("Iniciando upload do arquivo: %s\n", path);
    
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("Erro ao abrir arquivo para upload");
        return;
    }
    fclose(file);
    
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path; 
    }
    
    if (sendNewFileToServer((char *)filename, (char *)path, client_info.server_port, client_info.server_ip) == 0) {
        printf("Upload do arquivo %s concluído com sucesso.\n", filename);
    } else {
        printf("Erro ao fazer upload do arquivo %s.\n", filename);
    }
}

// Função para fazer download de um arquivo
void download_file(const char *filename) {
    printf("Iniciando download do arquivo: %s\n", filename);
    
    char current_dir[MAX_PATH_SIZE];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("Erro ao obter diretório atual");
        return;
    }
    
    if (receiveNewFileFromServer((char *)filename, current_dir, client_info.server_port, client_info.server_ip) == 0) {
        printf("Download do arquivo %s concluído com sucesso para o diretório atual.\n", filename);
    } else {
        printf("Erro ao fazer download do arquivo %s.\n", filename);
    }
}

// Função para excluir um arquivo
void delete_file(const char *filename) {
    printf("Solicitando exclusão do arquivo: %s\n", filename);
    
    if (removeFileInServer((char *)filename, client_info.sync_dir_path, client_info.server_port, client_info.server_ip) == 0) {
        printf("Arquivo %s excluído com sucesso no servidor.\n", filename);
        
        char sync_path[MAX_PATH_SIZE];
        int written = snprintf(sync_path, MAX_PATH_SIZE, "%s/%s", client_info.sync_dir_path, filename);
        if (written < 0 || written >= MAX_PATH_SIZE) {
            fprintf(stderr, "Caminho muito longo, arquivo não removido localmente: %s/%s\n", client_info.sync_dir_path, filename);
            return;
        }
        
        if (access(sync_path, F_OK) == 0) {
            if (remove(sync_path) == 0) {
                printf("Arquivo %s removido do diretório sync_dir.\n", filename);
            } else {
                perror("Erro ao remover arquivo do diretório sync_dir");
            }
        }
    } else {
        printf("Erro ao excluir o arquivo %s no servidor.\n", filename);
    }
}

// Função para iniciar a sincronização
void get_sync_dir() {
    printf("Iniciando sincronização do diretório...\n");
    
    // Falta aqui colocar a lógica de sincronização
    // Por enquanto, apenas verifica se o diretório existe
    if (check_and_create_directory(client_info.sync_dir_path) == 0) {
        printf("Diretório de sincronização pronto.\n");
    } else {
        printf("Erro ao preparar diretório de sincronização.\n");
    }
}

// Função que processa os comandos do usuário
void process_command(char *command) {
    char cmd[MAX_COMMAND_SIZE];
    char arg[MAX_COMMAND_SIZE];

    arg[0] = '\0';
    
    if (sscanf(command, "%s %[^\n]", cmd, arg) < 1) {
        printf("Comando inválido.\n");
        return;
    }
    
    if (strcmp(cmd, "upload") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: upload <path/filename.ext>\n");
        } else {
            upload_file(arg);
        }
    } else if (strcmp(cmd, "download") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: download <filename.ext>\n");
        } else {
            download_file(arg);
        }
    } else if (strcmp(cmd, "delete") == 0) {
        if (strlen(arg) == 0) {
            printf("Uso: delete <filename.ext>\n");
        } else {
            delete_file(arg);
        }
    } else if (strcmp(cmd, "list_server") == 0) {
        list_server_files();
    } else if (strcmp(cmd, "list_client") == 0) {
        list_client_files();
    } else if (strcmp(cmd, "get_sync_dir") == 0) {
        get_sync_dir();
    } else if (strcmp(cmd, "exit") == 0) {
        printf("Encerrando sessão...\n");
        client_info.running = 0;
    } else {
        printf("Comando desconhecido: %s\n", cmd);
        printf("Comandos disponíveis:\n");
        printf("  upload <path/filename.ext>\n");
        printf("  download <filename.ext>\n");
        printf("  delete <filename.ext>\n");
        printf("  list_server\n");
        printf("  list_client\n");
        printf("  get_sync_dir\n");
        printf("  exit\n");
    }
}

// Thread para receber comandos do usuário
void *command_thread_function(void *arg) {
    char command[MAX_COMMAND_SIZE];
    
    printf("\n=== Cliente Dropbox ===\n");
    printf("Digite 'help' para ver os comandos disponíveis.\n");
    
    while (client_info.running) {
        printf("\n> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = '\0';
        
        if (strlen(command) > 0) {
            if (strcmp(command, "help") == 0) {
                printf("Comandos disponíveis:\n");
                printf("  upload <path/filename.ext> - Envia um arquivo para o servidor\n");
                printf("  download <filename.ext> - Baixa um arquivo do servidor\n");
                printf("  delete <filename.ext> - Exclui um arquivo do servidor\n");
                printf("  list_server - Lista os arquivos no servidor\n");
                printf("  list_client - Lista os arquivos no diretório local\n");
                printf("  get_sync_dir - Inicia a sincronização\n");
                printf("  exit - Encerra a sessão\n");
            } else {
                process_command(command);
            }
        }
    }
    
    printf("Thread de comandos encerrada.\n");
    return NULL;
}   

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <username> <server_ip> <port>\n", argv[0]);
        return 1;
    }
    
    // Inicializa as informações do cliente
    strncpy(client_info.username, argv[1], sizeof(client_info.username));
    strncpy(client_info.server_ip, argv[2], sizeof(client_info.server_ip));
    client_info.server_port = atoi(argv[3]);
    client_info.running = 1;
    
    // Configura o diretório do cliente
    if (setup_client_directory(client_info.username) != 0) {
        fprintf(stderr, "Erro ao configurar diretório do cliente.\n");
        return 1;
    }
    
    get_sync_dir();
    
    // Cria a thread para receber comandos
    pthread_t command_thread;
    if (pthread_create(&command_thread, NULL, command_thread_function, NULL) != 0) {
        perror("Erro ao criar thread de comandos");
        return 1;
    }
    
    // Aguarda a thread de comandos terminar
    pthread_join(command_thread, NULL);
    
    printf("Cliente encerrado.\n");
    return 0;
}