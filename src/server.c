#include "server.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>

int server_fd;

// TODO: add compression (gzip/brotli)
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
	signal(SIGINT, handle_signal);

	// Configure socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(SERVER_PORT);

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

		if (!client_fd) {
			perror("Memory allocation failed");
			continue;
		}

		if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
		                         &client_addr_len)) < 0) {
			perror("accept failed");
			free(client_fd);
			continue;
		}

		pthread_t thread_id;
		if (pthread_create(&thread_id, NULL, handle_client,
		                   (void *)client_fd) != 0) {
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
	int client_fd = *((int *)arg);
	char *request = (char *)malloc(BUFFER_SIZE);

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

	printf("request:\n%s", request);

	char **headers = parse_headers(request);

	char *method = strtok(request, " ");
	if (strcmp(method, "GET") != 0) {
		fprintf(stderr, "Unsuported HTTP method: %s\n", method);
		close(client_fd);
		free(arg);
		free(request);
		return NULL;
	}

	char *file_name = strtok(NULL, " ");
	if (!file_name) {
		perror("Malformed HTTP request: missing file name");
		file_not_found_error(client_fd);
		close(client_fd);
		free(arg);
		free(request);
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
			free(request);
			return NULL;
		}
	}

	if (access(file_path, F_OK) == 0) {
		send_file(client_fd, file_path);
	} else {
		file_not_found_error(client_fd);
	}

	close(client_fd);
	free(arg);
	free(request);
	return NULL;
}

void handle_signal(int signal) {
	printf("Shutting down server...\n");
	close(server_fd);
	exit(0);
}

void send_file(int client_fd, char *file_path) {
	FILE *file = fopen(file_path, "r");
	if (file == NULL) {
		perror("Failed to open file");
		file_not_found_error(client_fd);
		return;
	}

	char response_header[BUFFER_SIZE];
	snprintf(response_header, sizeof(response_header),
	         "HTTP/1.1 200 OK\r\n"
	         "Content-Type: %s\r\n"
	         "\r\n",
	         get_mime_type(file_path));

	// Send file contents
	send(client_fd, response_header, strlen(response_header), 0);
	char file_buffer[BUFFER_SIZE];
	size_t bytes_read;
	while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) >
	       0) {
		send(client_fd, file_buffer, bytes_read, 0);
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
