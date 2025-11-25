#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#define MAX_POST_SIZE 8192 // 8KB máximo
#define LOG_SEPARATOR "\n========================================\n"

// Declaração das funções
char* get_config_string(const char *filename, int target_line);
int get_config_int(const char *filename, int target_line);
void handle_get(int client_fd, const char *rootDir, const char *filename);
void handle_post(int client_fd, const char *buffer);
void handle_request(int client_fd, const char *rootDir);
void start_server(const char *rootDir, int port);

typedef struct {
    char *ext;
    char *mediatype;
} extn;

typedef struct {
    int client_fd;
    const char *rootDir;
} ThreadArgs;


// Tipos de media suportados
extn extensions[] = {
    {"gif", "image/gif"},
    {"txt", "text/plain"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"ico", "image/x-icon"},
    {"zip", "application/zip"},
    {"gz", "application/gzip"},
    {"tar", "application/x-tar"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"php", "text/html"},
    {"pdf", "application/pdf"},
    {"rar", "application/vnd.rar"},
    {NULL, NULL}};

// Estrutura para lidar com requisições HTTP
typedef struct {
    void (*handle_get)(int client_fd, const char *rootDir, const char *filename);
    void (*handle_post)(int client_fd, const char *buffer);
} RequestHandler;

// Obtém tamanho do arquivo
unsigned long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Obtém o tipo de mídia pelo nome do arquivo
const char *get_mime_type(const char *filename) {
    char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    ext++;
    for (int i = 0; extensions[i].ext != NULL; i++) {
        if (strcmp(ext, extensions[i].ext) == 0) {
            return extensions[i].mediatype;
        }
    }
    return "application/octet-stream";
}

// Envia um arquivo ao cliente
void send_file(int client_fd, const char *filepath) {
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        printf("\U0001F6AB [ERRO] Arquivo não encontrado: %s\n", filepath);
        send(client_fd, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
                        "<html><body><h1>404 Not Found</h1></body></html>", 75, 0);
        return;
    }
    
    unsigned long filesize = get_file_size(filepath);
    printf("\U0001F4C4 [INFO] Enviando arquivo: %s (Tamanho: %lu bytes)\n", filepath, filesize);

    char header[256];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\n\r\n",
             get_mime_type(filepath), filesize);
    send(client_fd, header, strlen(header), 0);
    sendfile(client_fd, file_fd, NULL, filesize);

    close(file_fd);
}

// Obtém uma string de uma linha específica no arquivo de configuração
char* get_config_string(const char *filename, int target_line) {
    static char value[256];
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo de configuração");
        exit(EXIT_FAILURE);
    }

    int line_count = 0;
    while (fgets(value, sizeof(value), file)) {
        if (value[0] == '#' || strlen(value) < 2) continue;
        value[strcspn(value, "\r\n")] = 0;
        if (line_count == target_line) {
            fclose(file);
            return value;
        }
        line_count++;
    }
    fclose(file);
    return NULL;
}

// Obtém um número inteiro de uma linha específica no arquivo de configuração
int get_config_int(const char *filename, int target_line) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo de configuração");
        exit(EXIT_FAILURE);
    }

    char line[64];
    int line_count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) < 2) continue;
        line[strcspn(line, "\r\n")] = 0;
        if (line_count == target_line) {
            fclose(file);
            return atoi(line);
        }
        line_count++;
    }
    fclose(file);
    return -1;
}

// Manipula requisições GET
void handle_get(int client_fd, const char *rootDir, const char *filename) {
    if (!filename || strcmp(filename, "/") == 0) {
        filename = "/index.html";  // Página padrão
    }

    if (strstr(filename, "..")) { // Prevenir ataques de Directory Traversal
        send(client_fd, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
        printf("\U0001F6AB [ERRO] Tentativa de Directory Traversal: %s\n", filename);
        return;
    }

    char filepath[512], real_filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", rootDir, filename);
    
    printf("\U0001F50D [DEBUG] filepath antes do realpath: %s\n", filepath);
    
    if (realpath(filepath, real_filepath) == NULL) {
        perror("\U0001F6AB [ERRO] Falha ao resolver realpath");
        printf("\U0001F50D [DEBUG] Caminho fornecido para realpath: %s\n", filepath);
        send(client_fd, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
        return;
    }
    

    printf("\U0001F50D [DEBUG] real_filepath: %s\n", real_filepath);

    send_file(client_fd, filepath);
}

// Manipula requisições POST
void handle_post(int client_fd, const char *buffer) {
    
    printf("\U0001F4E2 [INFO] Método POST recebido!\n");

    // Encontrar o Content-Length para saber o tamanho do corpo da requisição
    char *content_length_str = strstr(buffer, "Content-Length: ");
    int content_length = 0;
    if (content_length_str) {
        content_length = atoi(content_length_str + 16);
    }

    if (content_length > MAX_POST_SIZE) {
        send(client_fd, "HTTP/1.1 413 Payload Too Large\r\n\r\n", 33, 0);
        return;
    }

    // Se houver um corpo na requisição
    char *body = strstr(buffer, "\r\n\r\n");
    if (body) {
        body += 4;  // Avançar os 4 caracteres do \r\n\r\n para chegar no corpo
    }

    if (content_length > 0 && body) {
        char post_data[1024] = {0};
        strncpy(post_data, body, content_length);
        post_data[content_length] = '\0'; // Certificar que a string termina corretamente

        printf("\U0001F4DD [POST DATA]: %s\n", post_data);

        // Responder ao cliente com os dados recebidos
        char response[1024];
        snprintf(response, sizeof(response) - 1, 
         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"
         "Dados recebidos: %.900s", post_data);
        response[sizeof(response) - 1] = '\0';  // Garantir terminação

        send(client_fd, response, strlen(response), 0);

    } else {
        send(client_fd, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n"
                        "Nenhum dado POST recebido.", 51, 0);
    }
}

// Processa a requisição HTTP
void handle_request(int client_fd, const char *rootDir) {
    char buffer[2048] = {0};
    read(client_fd, buffer, sizeof(buffer) - 1);
    
    printf(LOG_SEPARATOR);
    printf("\U0001F680 [REQUISIÇÃO] Recebida:\n%s\n", buffer);

    RequestHandler handler = {handle_get, handle_post};

    if (strncmp(buffer, "GET ", 4) == 0) {
        char *filename = strtok(buffer + 4, " ");
        handler.handle_get(client_fd, rootDir, filename);
    } else if (strncmp(buffer, "POST ", 5) == 0) {
        handler.handle_post(client_fd, buffer);
    } else {
        send(client_fd, "HTTP/1.1 405 Method Not Allowed\r\n\r\n", 35, 0);
    }
}

// Thread para processar cada requisição
void* handle_request_thread(void* arg) {
    ThreadArgs *args = (ThreadArgs*) arg;
    handle_request(args->client_fd, args->rootDir);
    close(args->client_fd);
    free(args);
    return NULL;
}

void start_server(const char *rootDir, int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao fazer bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Erro ao escutar");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("\U0001F680 [SERVIDOR] Rodando em: http://127.0.0.1:%d/\n", port);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        if (client_fd == NULL) {
            perror("Erro ao alocar memória para client_fd");
            continue;
        }
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        

        if (*client_fd < 0) {
            free(client_fd);
            perror("Erro ao aceitar conexão");
            continue;
        }

        pthread_t thread;
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->client_fd = *client_fd;
        args->rootDir = rootDir;

        pthread_create(&thread, NULL, handle_request_thread, args);
        pthread_detach(thread);
    }
}

int main() {
    const char *config_file = ".serverConfig";
    
    char *rootDir = get_config_string(config_file, 0);
    int port = get_config_int(config_file, 2);
    
    printf(LOG_SEPARATOR);
    printf("\U0001F50E [CONFIG] RootDir: %s\n", rootDir);
    printf("\U0001F50E [CONFIG] Port: %d\n", port);

    if (!rootDir || port < 1024 || port > 65535) {
        printf("\U0001F6AB [ERRO] Configuração inválida!\n");
        exit(EXIT_FAILURE);
    }
    
    printf("\U0001F4A1 [INFO] Iniciando servidor...\n");
    start_server(rootDir, port);

    return 0;
}
