#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"

#define MSG_BUF_SIZE 1000

typedef struct client_t {
    int sock;
    struct sockaddr_in addr;
    char buf[MSG_BUF_SIZE];
} client;

#define MAX_CLIENTS 5
unsigned int num_clients = 0;
client clients[MAX_CLIENTS];

void print_addr(struct sockaddr_in *s);
void get_addr(struct sockaddr_in *s);
void broadcast(char *msg, int exclusion);

char ip_addr_buf[16];
char msg_buf[MSG_BUF_SIZE];

char stdin_buf[MSG_BUF_SIZE];

char c;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        exit(-1);
    }

    long port = strtol(argv[1], NULL, 10);
    if (port == 0) {
        fprintf(stderr, "invalid port\n");
        exit(-1);
    }

    printf("[log] initializing...\n");

    int server_sock = socket_init();

    struct sockaddr_in server_addr = sock_addr_init(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind to the port
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        fprintf(stderr, "error on bind()\n");
        exit(-1);
    }

    // Listen on that port
    if (listen(server_sock, 5) != 0) {
        fprintf(stderr, "error on listen()\n");
        exit(-1);
    }

    make_non_blocking(server_sock);
    make_non_blocking(STDIN_FILENO);

    printf("[log] server is listening at ");
    print_addr(&server_addr);
    printf("\n");

    printf("[log] type any message and press enter to send to all clients\n");
    printf("[log] special commands are:\n");
    printf("[log]     \"exit\": shuts down the server\n");

    while (1) {
        // Accept a connection if we can
        if (num_clients < MAX_CLIENTS) {
            socklen_t sock_length = sizeof(clients[num_clients]);
            int client_sock;
            if ((client_sock = accept(server_sock, (struct sockaddr *) &clients[num_clients].addr, &sock_length)) != -1) {
                clients[num_clients].sock = client_sock;

                printf("[log] client connected: ");
                print_addr(&clients[num_clients].addr);
                printf("\n");

                num_clients++;
            }
        }

        // Poll every client
        for (int i = 0; i < num_clients; i++) {
            long n = recv(clients[i].sock, &c, 1, MSG_WAITALL);
            if (n > 0 && update_buf(clients[i].buf, c, MSG_BUF_SIZE)) {
                get_addr(&clients[i].addr);
                snprintf(msg_buf, MSG_BUF_SIZE, "[%s:%d]: %s\n", ip_addr_buf, ntohs(clients[i].addr.sin_port), clients[i].buf);
//                broadcast(msg_buf, i);
                broadcast(msg_buf, -1);
                printf("%s", msg_buf);
                clients[i].buf[0] = '\0';
            }
            if (n == 0) {
                printf("[log] client disconnected: ");
                print_addr(&clients[i].addr);
                printf("\n");

                for (int j = i + 1; j < num_clients; j++) {
                    clients[j - 1] = clients[j];
                }

                num_clients--;
            }
        }

        // Read stdin
        if (read(STDIN_FILENO, &c, 1) > 0 && update_buf(stdin_buf, c, MSG_BUF_SIZE)) {
            if (strncmp(stdin_buf, "exit", 4) == 0) {
                printf("[log] shutting down server...\n");
                for (int i = 0; i < num_clients; i++) {
                    close(clients[i].sock);
                }
                close(server_sock);
                exit(0);
            }

            snprintf(msg_buf, MSG_BUF_SIZE, "[server]: %s\n", stdin_buf);
            broadcast(msg_buf, -1);
            printf("%s", msg_buf);
            stdin_buf[0] = '\0';
        }
    }
}

void get_addr(struct sockaddr_in *s) {
    inet_ntop(AF_INET, &s->sin_addr.s_addr, ip_addr_buf, 17);
}
void print_addr(struct sockaddr_in *s) {
    get_addr(s);
    printf("%s:%d", ip_addr_buf, ntohs(s->sin_port));
}

void broadcast(char *msg, int exclusion) {
    for (int i = 0; i < num_clients; i++) {
        if (i == exclusion) continue;
        if (send(clients[i].sock, msg, strlen(msg), 0) == -1) {
            printf("Error sending to ");
            print_addr(&clients[i].addr);
            printf("\n");
        }
    }
}