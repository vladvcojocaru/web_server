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

        if ((*client_fd = accept(server_fd, (struct sockaddr_in *)&client_addr,
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

}
