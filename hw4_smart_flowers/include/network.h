#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>

/** @brief Parses a decimal TCP port in the range 1..65535. */
int parse_port(const char *text, int *port);

/** @brief Creates a listening IPv4 TCP socket on the requested address. */
int create_listener(const char *ip, int port);

/** @brief Connects a TCP client to a server. */
int connect_server(const char *ip, int port);

/** @brief Sends a line without blocking indefinitely on a slow peer. */
int send_line(int fd, const char *line);

/** @brief Reads a newline-terminated command; returns 0 on disconnect. */
int read_line(int fd, char *buffer, size_t capacity);

#endif
