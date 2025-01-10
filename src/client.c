#include "server.h"
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>


int main(){
    int sock_fd;
    struct sockaddr_in server_addr;
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    // Create socket
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        return 1;
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT
 );

    if(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0){
        perror("Invalid address / Address not supported");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }


    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock_fd);
        return 1;
    }

    // Construct the HTTP GET request
    snprintf(request, sizeof(request),
        "GET /index.html HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connections: close\r\n"
        "\r\n",
        SERVER_IP, SERVER_PORT
    );

    // Send the request to the server
    if (send(sock_fd, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        close(sock_fd);
        return 1;
    }

    // Receive the response
    printf("Server response:\n");
    size_t bytes_received;
    while((bytes_received = recv(sock_fd, response, sizeof(response) - 1,0)) > 0){
        response[bytes_received] = '\0';
        printf("%s", response);
    }
    if (bytes_received < 0) {
        perror("Failed to receive response");
    }

    // Close the socket
    close(sock_fd);
    return 0;
}
