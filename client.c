#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"

#define MSG_BUF_SIZE 1000

char msg_buf[MSG_BUF_SIZE];
char stdin_buf[MSG_BUF_SIZE];

char c;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: %s <ip> <port>\n", argv[0]);
        exit(-1);
    }

    long port = strtol(argv[2], NULL, 10);
    if (port == 0) {
        fprintf(stderr, "invalid port\n");
        exit(-1);
    }

    int sock = socket_init();

    // Construct the server address object
    struct sockaddr_in server_addr = sock_addr_init(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr) <= 0) {
        fprintf(stderr, "invalid ip address\n");
        exit(-1);
    }

    printf("connecting to server...\n");
    while (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0);
    printf("connected to server!\n");

    make_non_blocking(sock);
    make_non_blocking(STDIN_FILENO);

    while (1) {
        // Poll the server for new data
        long n = recv(sock, &c, 1, MSG_WAITALL);
        if (n > 0 && update_buf(msg_buf, c, MSG_BUF_SIZE)) {
            printf("%s\n", msg_buf);
            msg_buf[0] = '\0';
        }
        if (n == 0) {
            printf("lost connection with server\n");
            exit(-1);
        }

        // Read stdin and send if we have a message
        if (read(STDIN_FILENO, &c, 1) > 0 && update_buf(stdin_buf, c, MSG_BUF_SIZE)) {
            if (strncmp(stdin_buf, "exit", 4) == 0) {
                printf("closing connection with server...\n");
                close(sock);
                exit(0);
            }

            if (send(sock, stdin_buf, strlen(stdin_buf), 0) == -1 || send(sock, "\n", 1, 0) == -1) {
                printf("Error sending message to server");
                printf("\n");
            }
            stdin_buf[0] = '\0';
        }
    }
}