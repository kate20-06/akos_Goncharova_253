#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/** @brief Converts a port string to a valid TCP port number. */
int parse_port(const char *text, int *port) {
    char *end;
    long value;
    if (!text || !*text) return 0;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || *end || value < 1 || value > 65535) return 0;
    *port = (int)value;
    return 1;
}

/** @brief Sets time limits for socket reads and writes. */
static void set_timeout(int fd) {
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

/** @brief Binds and starts an IPv4 TCP listener. */
int create_listener(const char *ip, int port) {
    struct sockaddr_in address = {0};
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1 ||
        bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(fd, 128) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/** @brief Connects to an IPv4 address and configures socket timeouts. */
int connect_server(const char *ip, int port) {
    struct sockaddr_in address = {0};
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    set_timeout(fd);
    return fd;
}

/** @brief Sends a complete command without raising SIGPIPE. */
int send_line(int fd, const char *line) {
    size_t remaining = strlen(line);
    while (remaining > 0) {
        ssize_t n = send(fd, line, remaining, MSG_NOSIGNAL);
        if (n > 0) {
            remaining -= (size_t)n;
            line += n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}

/** @brief Reads one bounded text line, rejecting oversized commands. */
int read_line(int fd, char *buffer, size_t capacity) {
    size_t length = 0;
    int partial_timeouts = 0;
    while (length + 1 < capacity) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n == 1) {
            partial_timeouts = 0;
            if (ch == '\n') {
                buffer[length] = '\0';
                return 1;
            }
            if (ch != '\r') buffer[length++] = ch;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (!length) return -1;
            if (++partial_timeouts < 5) continue;
        }
        return 0;
    }
    return 0;
}
