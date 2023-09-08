#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define MSG_BUF_SIZE 1000

// Buffer that a message from the server is stored in as it is read in
char msg_buf[MSG_BUF_SIZE];

// Buffer that user input is stored in as it is read in
char stdin_buf[MSG_BUF_SIZE];

char c;

int socket_init();
struct sockaddr_in sock_addr_init(long port);
void make_non_blocking(int fd);
bool update_buf(char *buf, char new_char, int max_size);

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

    // Construct the server address object
    struct sockaddr_in server_addr = sock_addr_init(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr) <= 0) {
        fprintf(stderr, "invalid ip address\n");
        exit(-1);
    }

    printf("[log] connecting to server...\n");

    // Try to connect to the server until success
    while (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0);

    printf("[log] connected to server!\n");
    printf("[log] type any message and press enter to send\n");
    printf("[log] special commands are:\n");
    printf("[log]     \"exit\": closes connection with the server\n");

    // Make sock and stdin non-blocking so we can poll both at once
    make_non_blocking(sock);
    make_non_blocking(STDIN_FILENO);

    while (1) {
        // Receive a character from the server if there is one.
        // n=1   - We have a character, process it
        // n=0   - Connection has been terminated, exit
        // n=-1  - Nothing to receive, poll again later
        long n = recv(sock, &c, 1, MSG_WAITALL);

        // We have a character, add it to msg_buf with update_buf and if that
        // returns true we have the whole message
        if (n > 0 && update_buf(msg_buf, c, MSG_BUF_SIZE)) {
            printf("%s\n", msg_buf);
            msg_buf[0] = '\0';
        }

        // Exit if n=0 since that means connection has been terminated
        if (n == 0) {
            printf("lost connection with server\n");
            exit(-1);
        }

        // Read stdin and send if we have a message
        if (read(STDIN_FILENO, &c, 1) > 0 && update_buf(stdin_buf, c, MSG_BUF_SIZE)) {
            // Exit command
            if (strncmp(stdin_buf, "exit", 4) == 0) {
                printf("[log] closing connection with server...\n");
                close(sock);  // Gracefully close the socket before exiting
                exit(0);
            }

            // If we can't send the message or the newline, log it but don't exit
            if (send(sock, stdin_buf, strlen(stdin_buf), 0) == -1 || send(sock, "\n", 1, 0) == -1) {
                printf("[log] error sending message to server");
                printf("\n");
            }

            // Clear the stdin buf to prepare for a new message
            stdin_buf[0] = '\0';
        }
    }
}


// --- COMMON FUNCTIONS (in client and server) ---

int socket_init() {
    /**
     * Initializes a TCP socket
     * @return the socket's fd
     */
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        fprintf(stderr, "error on socket()\n");
        exit(-1);
    }
    return sock;
}

struct sockaddr_in sock_addr_init(long port) {
    /**
     * Initialize a socket address object (stores ip and port) with a given port.
     * @note the IP address is returned null and must be populated by the function caller
     * @return the created socket address
     */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return addr;
}

void make_non_blocking(int fd) {
    /**
     * Makes the given file descriptor non-blocking. Exits program on failure.
     */
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
        fprintf(stderr, "error on fcntl()\n");
        exit(-1);
    }
}

bool update_buf(char *buf, char new_char, int max_size) {
    /**
     * Updates a buffer with a given new character. Returns true if the buffer
     * is done being read, otherwise false. Used for reading characters from stdin or messages from server.
     * Buffer is done reading when a newline is reached or the buffer is full.
     */

    unsigned long c_len = strlen(buf);

    if (new_char != '\n') {
        buf[c_len] = new_char;
        buf[c_len + 1] = '\0';
    }

    if (c_len + 1 == max_size - 1 || new_char == '\n') {
        return true;
    }

    return false;
}
