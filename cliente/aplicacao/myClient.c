#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define portS 1234
int get_sync_dir(char *username, int server_IP, int port) {
    //fazer conexao aqui,se nao conseguir, retornar erro (1)
    if(port!=portS)
    return 1;
    printf("Connection established successfully \n");
    //verificar se diretorio existe no servidor, se nao, criar

    
    //verificar se diretorio existe no cliente, se nao, criar

    
    
        

    
    
    return (0);

}





   
int setLocalDirectory(char *username){
    char diretorio_local[50];
    snprintf(diretorio_local, sizeof(diretorio_local), "SYNC_DIR_%s", username);

    char *full_path_sync_dir = getenv(diretorio_local);
    if (full_path_sync_dir != NULL) {
        printf("Diretório local de sincronização %s encontrado \n", diretorio_local);
        printf("Full PATH: %s \n", full_path_sync_dir);
    } else {
        printf("Directory local de sincronizacao %s not finded \n", diretorio_local);
        char *parentPath= malloc(150 * sizeof(char));
        int existParenthPath=0;

        printf("Insert parent PATH of new local directory %s : \n", diretorio_local);
        fgets(parentPath,150,stdin);
        parentPath[strlen(parentPath)-1]='\0';
        existParenthPath=access(parentPath, F_OK);

        while(existParenthPath==-1){
        printf("Parenth PATH not found \n");
        printf("Insert parent PATH of new local directory %s : \n", diretorio_local);
        fgets(parentPath,150,stdin);
        parentPath[strlen(parentPath)-1]='\0';
        existParenthPath=access(parentPath, F_OK);
        }
        full_path_sync_dir=malloc(300*sizeof(char));
        strcpy(full_path_sync_dir,parentPath);
        strcat(full_path_sync_dir,"/");
        strcat(full_path_sync_dir,diretorio_local);
        if (mkdir(full_path_sync_dir, 0755) == 0) {
        printf("Diretório criado com sucesso: \n");
        printf("Full path: %s \n", full_path_sync_dir);
        } else {
        return(1);
        }
    }

}
int main(int argc, char *argv[]){
    if(argc!=4){
        printf("Invalid Input! \n");
        printf("Input Format: <username> <server_ip_address> <port> \n" );
        return(1);
    }
    int server_IP=atoi(argv[2]);
    int port=atoi(argv[3]);
    char *username=argv[1];
    if(setLocalDirectory(username)==1){
        printf("Not possbile to define local directory \n");
        return(1);
    }
    if(get_sync_dir(argv[1],server_IP,port)==1){
        printf("Not possible to estabilshed connection \n");
        return(1);
    }
    


   return(0);
}