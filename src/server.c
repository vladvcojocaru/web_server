#include "headers.h"
#include <stdio.h>
#include <zconf.h>
#include <zlib.h>

int server_fd;

// TODO: add caching
// TODO: support partial content delevery (206 parial content)
// TODO: use HTTPS

int main() {
    struct sockaddr_in server_addr;

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failes");
        exit(1);
    }

    // For faster testing
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_signal);

    // Configure socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) <
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

        if (!client_fd) {
            perror("Memory allocation failed");
            continue;
        }

        if ((*client_fd = accept(server_fd, (struct sockaddr *) &client_addr,
                                 &client_addr_len)) < 0) {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client,
                           (void *) client_fd) != 0) {
            perror("pthread_create failed");
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(thread_id);
    }

    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *((int *) arg);
    char *request = (char *) malloc(BUFFER_SIZE);

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, request, BUFFER_SIZE, 0);

    if (bytes_received == 0) {
        printf("client disconnected...\n");
        close(client_fd);
        free(arg);
        free(request);
        return NULL;
    } else if (bytes_received < 0) {
        perror("Failed to receive client request");
        close(client_fd);
        free(arg);
        free(request);
        return NULL;
    }

    char **headers = parse_headers(request);
    free(request);

    if (headers) {
        for (int i = 0; headers[i]; i++) {
            printf("Header[%d]: %s\n", i, headers[i]);
        }
    }

    char *method = strtok(headers[0], " ");
    if (strcmp(method, "GET") != 0) {
        fprintf(stderr, "Unsuported HTTP method: %s\n", method);
        close(client_fd);
        free(arg);
        free(headers);
        return NULL;
    }

    char *file_name = strtok(NULL, " ");
    if (!file_name) {
        perror("Malformed HTTP request: missing file name");
        file_not_found_error(client_fd);
        close(client_fd);
        free(arg);
        free(headers);
        return NULL;
    }

    char file_path[BUFFER_SIZE];
    if (strcmp(file_name, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "./www/index.html");
    } else {
        if (snprintf(file_path, sizeof(file_path), "./www%s", file_name) >=
            sizeof(file_path)) {
            fprintf(stderr, "File path too long\n");
            file_not_found_error(client_fd);
            close(client_fd);
            free(arg);
            free(headers);
            return NULL;
        }
    }

    if (access(file_path, F_OK) == 0) {
        send_file(client_fd, file_path, headers);
    } else {
        file_not_found_error(client_fd);
    }

    close(client_fd);
    free(arg);
    free(headers);
    return NULL;
}

void handle_signal(int signal) {
    printf("Shutting down server...\n");
    close(server_fd);
    exit(0);
}

void send_file(int client_fd, char *file_path, char **headers) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Failed to open file");
        file_not_found_error(client_fd);
        return;
    }

    char response_header[BUFFER_SIZE];

    int index;
    char *algorithms;

    // Include Accept-Encoding
    if ((index = search_header(headers, "Accept-Encoding")) != -1) {
        printf("original header: %s\n", headers[index]);

        // I could have used char: *header_copy = strdup(headers[index]);
        char *header_copy = (char *) malloc(strlen(headers[index]) + 1);
        if (!header_copy) {
            perror("Failed to allocate memory");
            exit(1);
        }
        strncpy(header_copy, headers[index], strlen(headers[index]) + 1);
        printf("header copy: %s\n", header_copy);

        // Tokenize the copied header
        char *key = strtok(header_copy, ": ");
        algorithms = strtok(NULL, "");
        printf("key: %s\nalgorithms: %s\n", key, algorithms);
        printf("tokenize finished\n");
    }

    // Format response header
    snprintf(response_header, sizeof(response_header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "%s\r\n"
             "\r\n",
             get_mime_type(file_path),
             (algorithms && strstr(algorithms, "gzip"))
                 ? "Content-Encoding: gzip"
                 : "");

    printf("created response header:\n%s\n", response_header);
    send(client_fd, response_header, strlen(response_header), 0);
    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Send file contents
    if (!strstr(algorithms, "gzip")) {
        while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) >
               0) {
            send(client_fd, file_buffer, bytes_read, 0);
        }
        // Compress the data and send it
    } else {
        char compressed_buffer[BUFFER_SIZE * 2];
        z_stream stream = {
            0}; // z_stream holds the state of the compression process

        // Initialize zlib compression
        // zalloc, zfree, opaque are used for memory allocation
        // Setting these on Z_NULL uses the default allocator
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        // Initialize z_stream to gzip compression
        if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED,
                         16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            perror("Failed to initialize gzip compression");
            fclose(file);
            return;
        }

        while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) >
               0) {
            stream.next_in = (Bytef *) file_buffer;
            stream.avail_in = bytes_read;

            do {
                stream.next_out = (Bytef *) compressed_buffer;
                stream.avail_out = sizeof(compressed_buffer);

                deflate(&stream, Z_SYNC_FLUSH);

                size_t compressed_size =
                    sizeof(compressed_buffer) - stream.avail_out;
                send(client_fd, compressed_buffer, compressed_size, 0);
            } while (stream.avail_out == 0);
        }

        do {
            stream.next_out = (Bytef *) compressed_buffer;
            stream.avail_out = sizeof(compressed_buffer);

            deflate(&stream, Z_SYNC_FLUSH);

            size_t compressed_size =
                sizeof(compressed_buffer) - stream.avail_out;
            send(client_fd, compressed_buffer, compressed_size, 0);
        } while (stream.avail_out == 0);

        deflateEnd(&stream);
    }

    fclose(file);
}

const char *get_mime_type(const char *file_path) {
    if (ends_with(file_path, ".html"))
        return "text/html";
    if (ends_with(file_path, ".css"))
        return "text/css";
    if (ends_with(file_path, ".js"))
        return "application/javascript";
    if (ends_with(file_path, ".jpg") || ends_with(file_path, ".jpeg"))
        return "image/jpeg";
    if (ends_with(file_path, ".png"))
        return "image/png";
    if (ends_with(file_path, ".json"))
        return "application/json";
    if (ends_with(file_path, ".txt"))
        return "text/plain";
    // Add more cases as needed
    return "application/octet-stream"; // Default for unknown types
}

void unsupported_media_error(int client_fd) {
    char *response_header = "HTTP/1.1 415 Unsupported Media Type\r\n"
                            "Content-Type: text/html\r\n"
                            "\r\n";
    char *error_body =
        "<html><body><h1>415 Unsupported Media Type</h1></body></html>";

    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, error_body, strlen(error_body), 0);
}

void file_not_found_error(int client_fd) {
    char *response_header = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: text/html\r\n"
                            "\r\n";
    char *error_body = "<html><body><h1>404 Not Found</h1></body></html>";

    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, error_body, strlen(error_body), 0);
}

bool ends_with(const char *main_str, char *ending) {
    size_t main_len = strlen(main_str);
    size_t ending_len = strlen(ending);

    if (main_len < ending_len) {
        return false;
    }

    return strcmp(main_str + main_len - ending_len, ending) == 0;
}

char **parse_headers(const char *buffer) {
    // Dynamic memory allocation
    char **tokens = malloc(MAX_TOKENS * sizeof(char));
    if (!tokens) {
        perror("Failed to allocate memory");
        return NULL;
    }

    // Make copy of the buffer to safely tokenize
    char *buffer_copy = strdup(buffer);
    if (!buffer_copy) {
        perror("Failed to duplicate buffer");
        free(tokens);
        return NULL;
    }

    // TODO: Trim white spaces from each header
    int token_count = 0;
    char *token = strtok(buffer_copy, "\n");

    while (token != NULL && token_count != MAX_TOKENS) {
        // Allocate memory for each token
        tokens[token_count] = strdup(token);
        if (!tokens[token_count]) {
            perror("Failed to allocate memory for token");

            // Clean up allocated memory before exiting
            for (int i = 0; i < token_count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            free(buffer_copy);
            return NULL;
        }
        token_count++;
        token = strtok(NULL, "\n");
    }

    tokens[token_count] = NULL; // Null-terminate the array
    free(buffer_copy);

    return tokens;
}

// Search for heading <header>, if it is found the function returns
// the <header> index if it is not it returns -1
int search_header(char **headers, const char *searched_header) {

    for (int i = 0; headers[i]; i++) {
        if (strncmp(headers[i], searched_header, strlen(searched_header)) ==
            0) {
            return i;
        }
    }
    return -1;
}

char *trim_whitespace(char *str) {
    char *end;

    // Trim leading spaces
    while (isspace((unsigned char) *str))
        str++;

    // If is all spaces return an empty string
    if (*str == 0)
        return str;

    // Trim trailing spaces""
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end))
        end--;

    // Null-terminate the string
    *(end + 1) = '\0';

    return str;
}
