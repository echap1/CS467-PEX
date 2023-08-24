#ifndef PEX_COMMON_H
#define PEX_COMMON_H

int socket_init();
struct sockaddr_in sock_addr_init(long port);
void make_non_blocking(int fd);
bool update_buf(char *buf, char new_char, int max_size);

#endif //PEX_COMMON_H
