# INF01151-Sistemas-Operacionais-II - Implementação Dropbox

## Como compilar o projeto

Navegue até o diretório raiz do projeto (onde `common.h` está localizado).
Execute os seguintes comandos para compilar o servidor e o cliente:

### Compilação do servidor

```bash
gcc -o server servidor/aplication/myServer.c \
                            servidor/comunication/comunicationServer.c 
```

### Compilação do cliente

```bash
gcc -o client cliente/aplication/myClient.c \
                            cliente/comunication/comunicationClient.c 
```

## Como executar o projeto

### 1. Iniciar o servidor

```bash
./server
```

### 2. Iniciar o(s) cliente(s)

```bash
./client <username> <server_ip_address> <port>
```

### Exemplo:
```bash
./client user1 127.0.0.1 8080
```
