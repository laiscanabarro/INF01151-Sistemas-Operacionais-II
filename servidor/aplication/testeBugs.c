#include <stdio.h>
char diretorio[] = "sou a global";

void teste() {
    char diretorio[100] = "sou a local";
    printf("diretorio = %s\n", diretorio);
}

int main() {
    teste();
}
