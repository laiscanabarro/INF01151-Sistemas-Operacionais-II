#ifndef COMUNICATION_CLIENT_H
#define COMUNICATION_CLIENT_H

#include "../../common.h" 

int sendNewFileToServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP);
int removeFileInServer(char *nomeArquivo,char *diretorio,int PORTA, char * IP);
int updateFileName(char *novoNovo,char *nomeAntigo,char *diretorio,int PORTA, char * IP);
int receiveNewFileFromServer(char *nomeArquivo, char *diretorio, int PORTA, char *IP);
int receiveLastSecondNotificationFromServer(notification_t *notification,char *diretorio, int PORTA, char *IP);
int receiveFileListFromServer(char ***arquivosServidor,char *diretorio,int  PORTA,char * IP);
void filterNotifications(notification_t *notifications, int *num_notifications);
int receiveLastSecondLocalNotification(notification_t *notifications, char *diretorio);

#endif // COMUNICATION_CLIENT_H
