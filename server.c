#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define MSG_BUF_SIZE 1000

// A client's data
typedef struct client_t {
    int sock;  // Their socket fd
    struct sockaddr_in addr;  // Their ip address
    char buf[MSG_BUF_SIZE];  // Their message buffer
} client;

#define MAX_CLIENTS 50
unsigned int num_clients = 0;
client clients[MAX_CLIENTS];

/**
 * Print the IP and port of the given socket addr
 */
void print_addr(struct sockaddr_in *s);

/**
 * Load the ascii form of the ip address into ip_addr_buf
 */
void get_addr(struct sockaddr_in *s);

/**
 * Send the given message to all clients except the one specified in exclusion.
 * Set exclusion to -1 to send to everyone.
 */
void broadcast(char *msg, int exclusion);

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

/**
 * Makes the given file descriptor non-blocking. Exits program on failure.
 */
void make_non_blocking(int fd);

/**
 * Updates a buffer with a given new character. Returns true if the buffer
 * is done being read, otherwise false. Used for reading characters from stdin or messages from server.
 * Buffer is done reading when a newline is reached or the buffer is full.
 */
bool update_buf(char *buf, char new_char, int max_size);

// Stores a client's ip address as a string (in preparation to print it)
char ip_addr_buf[16];

// Stores a message in this form "[<user>]: <message>"
// Printed to stdout and broadcast to each client
char msg_buf[MSG_BUF_SIZE];

// Buffer that user input is stored in as it is read in
char stdin_buf[MSG_BUF_SIZE];

char c;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        exit(-1);
    }

    // Get port from cmd line args
    long port = strtol(argv[1], NULL, 10);
    if (port == 0) {
        fprintf(stderr, "invalid port\n");
        exit(-1);
    }

    printf("[log] initializing...\n");

    int server_sock = socket_init();

    // Configure the server to accept clients regardless of IP
    struct sockaddr_in server_addr = sock_addr_init(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind to the port
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        fprintf(stderr, "error on bind()\n");
        exit(-1);
    }

    // Listen on the socket
    if (listen(server_sock, 5) != 0) {
        fprintf(stderr, "error on listen()\n");
        exit(-1);
    }

    // Make sock and stdin non-blocking so we can poll both at once
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
                // Store the new socket fd in the client object
                clients[num_clients].sock = client_sock;

                printf("[log] client connected: ");
                print_addr(&clients[num_clients].addr);
                printf("\n");

                // Let everyone know we have a new friend! :)
                num_clients++;
            }
        }

        // Poll every client
        for (int i = 0; i < num_clients; i++) {
            // Receive a character from the client if there is one.
            // n=1   - We have a character, process it
            // n=0   - Connection has been terminated, remove client from list
            // n=-1  - Nothing to receive, poll again later
            long n = recv(clients[i].sock, &c, 1, MSG_WAITALL);

            // We have a character, add it to msg_buf with update_buf and if that
            // returns true we have the whole message
            if (n > 0 && update_buf(clients[i].buf, c, MSG_BUF_SIZE)) {
                // Format the message
                get_addr(&clients[i].addr);
                snprintf(msg_buf, MSG_BUF_SIZE, "[%s:%d]: %s\n", ip_addr_buf, ntohs(clients[i].addr.sin_port), clients[i].buf);

//                broadcast(msg_buf, i);  // Broadcast to everyone except the sender
                broadcast(msg_buf, -1);  // Broadcast to everyone (so sender gets the echo back)

                printf("%s", msg_buf);  // Print to server console
                clients[i].buf[0] = '\0';  // Zero out client's buffer so we can accept more messages from them
            }

            // Client connection has been terminated, remove it from the list
            if (n == 0) {
                printf("[log] client disconnected: ");
                print_addr(&clients[i].addr);
                printf("\n");

                // Shift all clients after it down
                for (int j = i + 1; j < num_clients; j++) {
                    clients[j - 1] = clients[j];
                }

                // Goodbye friend :(
                num_clients--;
            }
        }

        // Read stdin
        if (read(STDIN_FILENO, &c, 1) > 0 && update_buf(stdin_buf, c, MSG_BUF_SIZE)) {
            // Exit command
            if (strncmp(stdin_buf, "exit", 4) == 0) {
                printf("[log] shutting down server...\n");

                // Say goodbye to all our friends :(
                // But gracefully!!
                for (int i = 0; i < num_clients; i++) {
                    close(clients[i].sock);  // Close each client's socket
                }

                close(server_sock);  // Can't forget this one
                exit(0);
            }

            // Format the server's message
            snprintf(msg_buf, MSG_BUF_SIZE, "[server]: %s\n", stdin_buf);

            // Send it to all clients and the console
            broadcast(msg_buf, -1);
            printf("%s", msg_buf);

            // Zero the buffer out so the server can send more stuff
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


// --- COMMON FUNCTIONS (in client and server) ---

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

void make_non_blocking(int fd) {
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
        fprintf(stderr, "error on fcntl()\n");
        exit(-1);
    }
}

bool update_buf(char *buf, char new_char, int max_size) {
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
