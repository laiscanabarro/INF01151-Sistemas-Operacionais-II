#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "comunicationClient.h"
#define PORTA 8080
#define TAMANHO_BUFFER 1024

int main() {
    
    char buffer[TAMANHO_BUFFER];
    char diretorio[TAMANHO_BUFFER];
    char nome_arquivo[TAMANHO_BUFFER];
    char novo_nome[TAMANHO_BUFFER];
    FILE *arquivo;

    // Pergunta o diretório de trabalho do cliente
    printf("Digite o diretório para buscar os arquivos:\n");
    fgets(diretorio, TAMANHO_BUFFER, stdin);
    diretorio[strcspn(diretorio, "\n")] = '\0';

    

    while(1){
   
        // Lista os arquivos no diretório
        DIR *dir = opendir(diretorio);
        if (!dir) {
            perror("Erro ao abrir diretório");
            exit(EXIT_FAILURE);
        }

        printf("\nArquivos disponíveis:\n");
        struct dirent *entrada;
        while ((entrada = readdir(dir)) != NULL) {
            if (entrada->d_type == DT_REG) {
                printf("- %s\n", entrada->d_name);
            }
        }
        closedir(dir);
        //menu de opcoes
        printf("Menu: \n");
        printf("0- encerra o programa \n");
        printf("1- envia arquivo novo ao servidor \n");
        printf("2- remove arquivo no servidor \n");
        printf("3- atualiza nome do arquivo no servidor \n" );
        printf("4- obter arquivo do servidor \n" );
        printf("5- receber notificacao de mudanca no servidor \n" );

        printf("Digite o opcao desejada:\n");
        int opcao;
        scanf("%d",&opcao);
        getchar();
        if(opcao==0){
            printf("Encerrar ...\n");
            break;
        }
        
        switch (opcao)
        {
        case 1:
            // Pede para selecionar um arquivo
        
            printf("Digite o nome do arquivo a ser enviado:\n");
            fgets(nome_arquivo, TAMANHO_BUFFER, stdin);
            nome_arquivo[strcspn(nome_arquivo, "\n")] = '\0';
            if(sendNewFileToServer(nome_arquivo,diretorio,PORTA,"127.0.0.1")==1)
            printf("erro ao enviar o arquivo");          
            break;
        
        case 2:
            printf("Digite o nome do arquivo a ser removido no servidor :\n");
            fgets(nome_arquivo, TAMANHO_BUFFER, stdin);
            nome_arquivo[strcspn(nome_arquivo, "\n")] = '\0';
            if(removeFileInServer(nome_arquivo,diretorio,PORTA,"127.0.0.1")==1)
            printf("erro ao remover o arquivo");   
            break;
        case 3:
            printf("Digite o nome antigo do arquivo:\n");
            fgets(nome_arquivo, TAMANHO_BUFFER, stdin);
            nome_arquivo[strcspn(nome_arquivo, "\n")] = '\0';
            printf("Digite o novo nome do arquivo:\n");
            fgets(novo_nome, TAMANHO_BUFFER, stdin);
            novo_nome[strcspn(novo_nome, "\n")] = '\0';
            printf("%s  aqui" , novo_nome);
            if(updateFileName(novo_nome,nome_arquivo,diretorio,PORTA,"127.0.0.1")==1)
            printf("erro ao atualizar nome do arquivo");   
            break;
        case 4:
            
            // Pede para selecionar um arquivo
        
            printf("Digite o nome do arquivo a ser recebido do servidor:\n");
            fgets(nome_arquivo, TAMANHO_BUFFER, stdin);
            nome_arquivo[strcspn(nome_arquivo, "\n")] = '\0';
            if(receiveNewFileFromServer(nome_arquivo,diretorio,PORTA,"127.0.0.1")==1)
            printf("erro ao enviar o arquivo");          
            break;
        case 5:
            
           
            
            notification_t notifications[300];
            int num_notifications=receiveLastSecondNotificationFromServer(notifications, PORTA,"127.0.0.1");
            if(num_notifications==0)
            printf("sem mudancas recentes \n");
            else
            {
                for (int i = 0; i < num_notifications; i++)
                {
                    printf("Arquivo: %s | Tipo: %d", notifications[i].fileName, notifications[i].type);
                    if (notifications[i].type == RENAMED_FILE)
                    {
                        printf(" | Nome Anterior: %s", notifications[i].ancientFileName);
                    }
                    printf("\n");
                }
            }
            break;
        }
        

        

        
       
    
    }
   
    return 0;
}
