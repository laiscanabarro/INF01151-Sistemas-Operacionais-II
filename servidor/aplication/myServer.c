//1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include "../comunication/comunicationServer.h"
#define PORTA 8080
#define TAMANHO_BUFFER 1024
#define MAX_SERVERS 10

server_t servers[10];
int nServers=0;
typedef struct {
    char nome_cliente[100];
    int num_devices_conected;
    char IP_devices [2][16];
} clientInfo_t;
int iAmPrimary=0;
clientInfo_t clientes_info[200];
int num_clientes=0;

void listar_clientes_conectados() {
    printf("=== Clientes conectados ===\n");
    for (int i = 0; i < num_clientes; i++) {
        printf("Cliente: %s\n", clientes_info[i].nome_cliente);
        printf("  Devices conectados: %d\n", clientes_info[i].num_devices_conected);

        for (int j = 0; j < clientes_info[i].num_devices_conected && j < 2; j++) {
            printf("    - IP do device %d: %s\n", j + 1, clientes_info[i].IP_devices[j]);
        }
    }
    printf("===========================\n");
}
int buscar_indice_cliente(const char *nome_cliente) {
    for (int i = 0; i < num_clientes; i++) {
        if (strcmp(clientes_info[i].nome_cliente, nome_cliente) == 0) {
            return i;  // cliente encontrado
        }
    }
    return -1;  // cliente não encontrado
}
int inserir_cliente(const char *nome_cliente){
    if(num_clientes<200){
        strcpy(clientes_info[num_clientes].nome_cliente,nome_cliente);
        clientes_info[num_clientes].num_devices_conected=0;
        num_clientes++;
        return 0;
    }
    return -1;  // cliente não encontrado
}
int buscar_indice_device(int indCliente,char * IP_device) {
    for (int i = 0; i < clientes_info[indCliente].num_devices_conected; i++) {
        if (strcmp(clientes_info[indCliente].IP_devices[i], IP_device) == 0) {
            return i;  // cliente encontrado
        }
    }
    return -1;  // cliente não encontrado
}
void inserir_device(int indCliente,char * IP_device) {
    strcpy(clientes_info[indCliente].IP_devices[clientes_info[indCliente].num_devices_conected],IP_device);
    clientes_info[indCliente].num_devices_conected++;
}
pthread_mutex_t conflitOperation = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t insertDevice = PTHREAD_MUTEX_INITIALIZER;
// Estrutura para passar parâmetros para a thread
typedef struct {
    int socket;
} ThreadArgs;
void *handle_primary_server(void *args)
{   
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;
    int codigo;
    recv(novo_socket, &codigo, sizeof(int), 0);
    char nome_cliente[100] = {0};
    int tamanho_nome_cliente = 0;
    recv(novo_socket, &tamanho_nome_cliente, sizeof(int), 0);

    if (tamanho_nome_cliente <= 0 || tamanho_nome_cliente >= sizeof(nome_cliente))
    {
        fprintf(stderr, "Tamanho de nome_cliente inválido: %d\n", tamanho_nome_cliente);
        close(novo_socket);
        free(args);
        pthread_exit(NULL);
    }
    recv(novo_socket, nome_cliente, tamanho_nome_cliente, 0);
    nome_cliente[tamanho_nome_cliente] = '\0';
    
    int tamanho_nome_IP;
    char IP_cliente[16];
    recv(novo_socket, &tamanho_nome_IP, sizeof(int), 0);
    recv(novo_socket, IP_cliente, tamanho_nome_IP, 0);
    IP_cliente[tamanho_nome_IP] = '\0';
    printf("Cliente conectado na thread %ld!\n", pthread_self());

    char diretorio[1000] = {0};
    if (codigo == 0)
    {
        printf("codigo 0");
        fflush(stdout);
        pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);

        const char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "Não foi possível obter o diretório HOME.\n");
            exit(1);
        }
        snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", home, nome_cliente);
        if (ind_cliente == -1)
        {
            inserir_cliente(nome_cliente);
            ind_cliente = num_clientes - 1;
            // Verifica se o diretório existe

            DIR *dir;
            dir = opendir(diretorio);

            if (dir == NULL)
            {
                char mkdirCommand[1500];
                snprintf(mkdirCommand, sizeof(mkdirCommand), "mkdir -p %s", diretorio);
                if (system(mkdirCommand) != 0)
                {
                    perror("Erro ao criar o diretório");
                }
                printf("Diretório criado com sucesso: %s\n", diretorio);

                // Aguarda 3 segundos
                sleep(3);
            }
            closedir(dir);
        }
        int codigo_device = 0;
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        if (ind_device == -1)
        {
            if (clientes_info[ind_cliente].num_devices_conected < 1)
            {
                inserir_device(ind_cliente, IP_cliente);
                ind_device = clientes_info[ind_cliente].num_devices_conected - 1;
            }
            else
            {
                codigo_device = -1;
                send(novo_socket, &codigo_device, sizeof(int), 0);
                pthread_mutex_unlock(&insertDevice);
                close(novo_socket);
                free(args); // Liberar a memória alocada para os argumentos
                pthread_exit(NULL);
            }
        }
        send(novo_socket, &codigo_device, sizeof(int), 0);
        pthread_mutex_unlock(&insertDevice);
    }
     else
    {

        const char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "Não foi possível obter o diretório HOME.\n");
            exit(1);
        }
        snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", home, nome_cliente);
    }
    if (codigo == 1)
    {
        if (receiveNewFileFromPrimary(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Erro ao receber o arquivo\n");
    }
    else if (codigo == 2)
    {
        if (removeFileInServerBackup(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Erro ao remover o arquivo\n");
    }
    else if (codigo == 3)
    {
        if (updateFileNameInBackup(novo_socket, diretorio, &conflitOperation) == 1)
            printf("Erro ao atualizar o nome do arquivo\n");
    }
    else if (codigo == 4)
    {
        if (sendNewFileToClient(novo_socket, diretorio, &conflitOperation,nome_cliente) == 1)
            printf("Erro ao atualizar o arquivo no cliente\n");
    }
    else if (codigo == 5)
    {
        pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        pthread_mutex_unlock(&insertDevice);
        if (sendLastSecondNotificationToClient(novo_socket,ind_cliente, diretorio) == 1)
            printf("Erro ao enviar notificação para o cliente\n");
    }
    else if (codigo == 6)
    {
        if (sendFileListToClient(novo_socket, diretorio) == 1)
            printf("Erro ao enviar lista de arquivos para o cliente\n");
    }
    else if (codigo==7){
        pthread_mutex_lock(&insertDevice);//usando mutex de inserir, mas agora no contexto de remover
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        strcpy(clientes_info[ind_cliente].IP_devices[ind_device],"");
        clientes_info[ind_cliente].num_devices_conected--;
        if(clientes_info[ind_cliente].num_devices_conected==0)
        strcpy(clientes_info[ind_cliente].nome_cliente,"");
        pthread_mutex_unlock(&insertDevice);
        int cod_encerra=0;
        send(novo_socket,&cod_encerra,sizeof(int), 0);

    }
    free(args); // Liberar a memória alocada para os argumentos
    pthread_exit(NULL);
}
// Função que será executada por cada thread
void *handle_client_or_backups(void *args)
{   
    
    
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int novo_socket = threadArgs->socket;
    int codigo;
    recv(novo_socket, &codigo, sizeof(int), 0);
    char nome_cliente[100] = {0};
    int tamanho_nome_cliente=0;
    recv(novo_socket, &tamanho_nome_cliente, sizeof(int), 0);

if (tamanho_nome_cliente <= 0 || tamanho_nome_cliente >= sizeof(nome_cliente)) {
    fprintf(stderr, "Tamanho de nome_cliente inválido: %d\n", tamanho_nome_cliente);
    close(novo_socket);
    free(args);
    pthread_exit(NULL);
}
    recv(novo_socket, nome_cliente, tamanho_nome_cliente, 0);
    nome_cliente[tamanho_nome_cliente] = '\0';
    printf("[DEBUG] Nome cliente recebido: %s\n", nome_cliente);
    int tamanho_nome_IP;
    char IP_cliente[16];
    recv(novo_socket, &tamanho_nome_IP, sizeof(int), 0);
    recv(novo_socket, IP_cliente, tamanho_nome_IP, 0);
    IP_cliente[tamanho_nome_IP] = '\0';
    printf("[DEBUG] IP cliente recebido: %s\n", IP_cliente);
    printf("Cliente conectado na thread %ld!\n", pthread_self());

    char diretorio[1000] = {0};
    if (codigo == 0)
    {       printf("codigo 0");
             fflush(stdout);
           pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        printf("[DEBUG] Índice do cliente encontrado: %d\n", ind_cliente);
        const char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "Não foi possível obter o diretório HOME.\n");
            exit(1);
        }
        snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", home, nome_cliente);
        if (ind_cliente == -1)
        {   
            inserir_cliente(nome_cliente);
            ind_cliente = num_clientes - 1;
            printf("[DEBUG] Novo indice do cliente: %d\n", ind_cliente);

            // Verifica se o diretório existe
         
            DIR *dir;
            dir = opendir(diretorio);
   
            if (dir == NULL)
            {
                char mkdirCommand[1500];
                snprintf(mkdirCommand, sizeof(mkdirCommand), "mkdir -p %s", diretorio);
                if (system(mkdirCommand) != 0)
                {
                    perror("Erro ao criar o diretório");
                }
                printf("Diretório criado com sucesso: %s\n", diretorio);

                // Aguarda 3 segundos
                sleep(3);
            
            }
            closedir(dir);
            
        }
        int codigo_device = 0;
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        printf("[DEBUG] Índice do device: %d\n", ind_device);
        if (ind_device == -1)
        {
            if (clientes_info[ind_cliente].num_devices_conected < 1)
            {
                inserir_device(ind_cliente, IP_cliente);
                ind_device = clientes_info[ind_cliente].num_devices_conected - 1;
                 printf("[DEBUG] Novo device inserido no índice: %d\n", ind_device);
            }
            else
            {
                codigo_device = -1;
                send(novo_socket, &codigo_device, sizeof(int), 0);
                pthread_mutex_unlock(&insertDevice);
                close(novo_socket);
                free(args); // Liberar a memória alocada para os argumentos
                pthread_exit(NULL);
            }
        }
        send(novo_socket, &codigo_device, sizeof(int), 0);
        pthread_mutex_unlock(&insertDevice);
        replicateConnecitonOnBackups(servers,nServers,nome_cliente,IP_cliente);
        int waitBackup=0;
        send(novo_socket, &waitBackup, sizeof(int), 0);
        
    }
    else
    {

        const char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "Não foi possível obter o diretório HOME.\n");
            exit(1);
        }
        snprintf(diretorio, sizeof(diretorio), "%s/sync_dir_%s_server", home, nome_cliente);
    }
    printf("[DEBUG] Diretório atribuído ao cliente %s: %s\n", nome_cliente, diretorio);
    if (codigo == 1)
    {
        if (receiveNewFileFromClient(novo_socket, diretorio, &conflitOperation,servers,nServers,nome_cliente,IP_cliente) == 1)
            printf("Erro ao receber o arquivo\n");
    }
    else if (codigo == 2)
    {
        if (removeFileInServer(novo_socket, diretorio, &conflitOperation,servers,nServers,nome_cliente,IP_cliente) == 1)
            printf("Erro ao remover o arquivo\n");
    }
    else if (codigo == 3)
    {
        if (updateFileName(novo_socket, diretorio, &conflitOperation,servers,nServers,nome_cliente,IP_cliente) == 1)
            printf("Erro ao atualizar o nome do arquivo\n");
    }
    else if (codigo == 4)
    {
        if (sendNewFileToClient(novo_socket, diretorio, &conflitOperation,nome_cliente) == 1)
            printf("Erro ao atualizar o arquivo no cliente\n");
    }
    else if (codigo == 5)
    {   pthread_mutex_lock(&insertDevice);
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        pthread_mutex_unlock(&insertDevice);
        if (sendLastSecondNotificationToClient(novo_socket,ind_cliente, diretorio) == 1)
            printf("Erro ao enviar notificação para o cliente\n");
    }
    else if (codigo == 6)
    {
        if (sendFileListToClient(novo_socket, diretorio) == 1)
            printf("Erro ao enviar lista de arquivos para o cliente\n");
    }
    else if (codigo==7){
        pthread_mutex_lock(&insertDevice);//usando mutex de inserir, mas agora no contexto de remover
        int ind_cliente = buscar_indice_cliente(nome_cliente);
        int ind_device = buscar_indice_device(ind_cliente, IP_cliente);
        strcpy(clientes_info[ind_cliente].IP_devices[ind_device],"");
        clientes_info[ind_cliente].num_devices_conected--;
        if(clientes_info[ind_cliente].num_devices_conected==0)
        strcpy(clientes_info[ind_cliente].nome_cliente,"");
        pthread_mutex_unlock(&insertDevice);
        int cod_encerra=0;
        send(novo_socket,&cod_encerra,sizeof(int), 0);

    }
    free(args); // Liberar a memória alocada para os argumentos
    pthread_exit(NULL);
}

// Valida se uma string é um IP IPv4 válido
int validar_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1;
}
// Analisa os argumentos e preenche o vetor servers[]
// Retorna 0 em caso de sucesso, ou -1 em caso de erro
int parse_server_args(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <IP_PRIMARIO> [<IP_BACKUP1> <IP_BACKUP2> ...]\n", argv[0]);
        return -1;
    }

    if (argc - 1 > MAX_SERVERS) {
        fprintf(stderr, "Erro: número máximo de servidores é %d.\n",MAX_SERVERS);
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        if (!validar_ip(argv[i])) {
            fprintf(stderr, "Erro: IP inválido -> %s\n", argv[i]);
            return -1;
        }

        strncpy(servers[nServers].IP, argv[i], 15);
        servers[nServers].IP[15] = '\0'; // garantir null-termination
        servers[nServers].isPrimary = (i == 1) ? 1 : 0;
        nServers++;
    }

    return 0;
}
void obter_ip_local(char *ip_buffer, size_t buffer_size) {
    FILE *fp = popen("hostname -I", "r");
    if (fp == NULL) {
        perror("Erro ao executar comando");
        snprintf(ip_buffer, buffer_size, "Erro");
        return;
    }

    if (fgets(ip_buffer, buffer_size, fp) != NULL) {
        // Remove o '\n' se houver
        char *newline = strchr(ip_buffer, '\n');
        if (newline) *newline = '\0';

        // Se houver vários IPs separados por espaço, pega só o primeiro
        char *espaco = strchr(ip_buffer, ' ');
        if (espaco) *espaco = '\0';
    } else {
        snprintf(ip_buffer, buffer_size, "Desconhecido");
    }

    pclose(fp);
}
int main(int argc, char *argv[])
{

    if (parse_server_args(argc, argv) != 0)
    {
        return 1;
    }

    char ip_local[16];
    obter_ip_local(ip_local, sizeof(ip_local));

    int encontrado = 0;
    for (int i = 0; i < nServers; i++)
    {
        if (strcmp(ip_local, servers[i].IP) == 0)
        {
            encontrado = 1;
            iAmPrimary = servers[i].isPrimary;
            break;
        }
    }

    if (!encontrado)
    {
        fprintf(stderr, "Erro: IP local (%s) não encontrado na lista de servidores fornecida.\n", ip_local);
        return 1;
    }

    // Exibe o resultado
    printf("IP local detectado: %s\n", ip_local);
    printf("%s\n", iAmPrimary ? "Eu sou o primário!" : "Eu sou backup.");

    int i = 0;
    int servidor_fd, novo_socket;
    struct sockaddr_in endereco;
    int opt = 1;

    int tamanho_endereco = sizeof(endereco);

    // Criação do socket
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    bind(servidor_fd, (struct sockaddr *)&endereco, sizeof(endereco));
    listen(servidor_fd, 3);

    while (1)
    {
        printf("Aguardando conexão...\n");
        novo_socket = accept(servidor_fd, (struct sockaddr *)&endereco, (socklen_t *)&tamanho_endereco);
        if (novo_socket < 0)
        {
            perror("Erro no accept");
            continue;
        }

        // Aloca argumentos para a thread
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->socket = novo_socket;
        if (iAmPrimary)
        {
            // Criação da thread para lidar com o cliente e backups
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client_or_backups, (void *)args) != 0)
            {
                printf("aqui?");
                perror("Erro ao criar a thread");
                free(args);
                close(novo_socket);
            }
            pthread_detach(thread_id); // Libera recursos automaticamente ao término
        }
        else
        {
            // Criação da thread para lidar com o servidor primario
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_primary_server, (void *)args) != 0)
            {
                printf("aqui?");
                perror("Erro ao criar a thread");
                free(args);
                close(novo_socket);
            }
            pthread_detach(thread_id); // Libera recursos automaticamente ao término
        }
    }

    close(servidor_fd);
    return 0;
}
