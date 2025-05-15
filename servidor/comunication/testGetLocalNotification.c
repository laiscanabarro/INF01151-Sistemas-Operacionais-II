#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "comunicationServer.h"

int main() {
    notification_t notifications[300];
     printf("ver novas modificiacoes \n");
        fflush(stdout);
    
    while(1){
        
        usleep(1000000);
        
        int num_notifications = receiveLastSecondLocalNotification(notifications, "/home/vitor/Desktop/servidor");
        if(num_notifications==0){
            printf("sem mofificacoes no ultimo segundo");
            printf("\n");
            fflush(stdout);
        }
        
        else{
            for (int i = 0; i < num_notifications; i++) {
        printf("Arquivo: %s | Tipo: %d", notifications[i].fileName, notifications[i].type);
        if (notifications[i].type == RENAMED_FILE) {
            printf(" | Nome Anterior: %s", notifications[i].ancientFileName);
        }
        printf("\n");


        }
        printf("modificacoes nesse segundo \n");
        fflush(stdout);
        }
        
    }



    
    
    return 0;
}