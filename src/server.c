#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <regex.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *handle_client(void *arg);
void send_error_message(int client_fd);
void send_content(int client_fd, char *file_path);

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failes");
        exit(1);
    }

    // Configure socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        perror("bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(1);
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if (!client_fd){
            perror("Memory allocation failed");
            continue;
        }

        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                                 &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    // Create thread to handle client request

    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE);

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if(bytes_received == 0){
        printf("client disconnected...\n");
        close(client_fd);
        free(arg);
        free(buffer);
        return NULL;
    } else if (bytes_received < 0){
        perror("recv failed");
        close(client_fd);
        free(arg);
        free(buffer);
        return NULL;
    }

    printf("request:\n%s", buffer);

    char *request = strtok(buffer, " ");
    if(strcmp(request,"GET") != 0){
        perror("Must have a GET request");
        close(client_fd);
        free(arg);
        free(buffer);
        return NULL;
    }

    char *file_name = strtok(NULL, " ");
    char file_path[BUFFER_SIZE];
    if (strcmp(file_name, "/") == 0){
        snprintf(file_path, sizeof(file_path), "./www/index.html");
    } else {
        snprintf(file_path, sizeof(file_path), "./www%s", file_name);
    }

    if (access(file_path, F_OK) == 0){
        send_content(client_fd, file_path);
    } else{
        send_error_message(client_fd);
    }

    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

void send_content(int client_fd, char *file_path){
    FILE *file = fopen(file_path, "r");
    if (file == NULL){
        perror("Failed to open file");
        send_error_message2(client_fd);
        return;
    }

    char *response_header =  "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n";

    send(client_fd, response_header, strlen(response_header), 0);

    char file_buffer[BUFFER_SIZE];
    while (fgets(file_buffer, sizeof(file_buffer), file)){
        send(client_fd, file_buffer, strlen(file_buffer), 0);
    }

    fclose(file);
}


void send_error_message(int client_fd){
    char *response_header =  "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/html\r\n"
                "\r\n";
    char *error_body = "<html><body><h1>404 Not Found ai</h1></body></html>";

    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, error_body, strlen(error_body), 0);
}
