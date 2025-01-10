#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

bool ends_with(const char *main_str, char *ending);
const char *get_mime_type(const char* file_path);
void *handle_client(void *arg);
void file_not_found_error(int client_fd);
void send_file(int client_fd, char *file_path);
void send_html(int client_fd, char *file_path, FILE *file);
void send_css(int client_fd, char *file_path);
void send_js(int client_fd, char *file_path);
void unsupported_media_error(int client_fd);
void handle_signal(int signal);
