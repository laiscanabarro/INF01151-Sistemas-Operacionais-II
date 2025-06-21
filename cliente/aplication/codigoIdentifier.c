#include <stdio.h>
#include <stdlib.h>
#include <string.h>




int main() {
    char ip[100] = {0};
    obter_ip_local(ip, sizeof(ip));
    int ID=getpid();
    printf("IP local (semi-privado): %s\n", ip);
    printf("ID do processo: %d\n",ID);
    return 0;
}
