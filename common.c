#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "common.h"

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
