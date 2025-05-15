#include <stdio.h>
#include <time.h>

int main() {
    // Caminho completo para o arquivo de saída
    char caminho_arquivo[] = "/home/vitor/Desktop/tempoUltimaSessao.txt";
    
    while(1){
        sleep(1);
    time_t agora = time(NULL);

    

    // Abrir o arquivo em modo de escrita
    FILE *arquivo = fopen(caminho_arquivo, "w");
    if (arquivo == NULL) {
        perror("Erro ao abrir o arquivo");
        return 1;
    }

    // Salvar apenas o valor bruto de time_t no arquivo
    fprintf(arquivo, "%ld", (long)agora);
    // Fechar o arquivo
    fclose(arquivo);
    
    printf("Tempo salvo em: %s\n", caminho_arquivo);
    }



    

    return 0;
}