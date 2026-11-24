#include "network.h"
#include "worker_protocol.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(CLIENT_FLOWER)
#define ROLE "FLOWER"
#elif defined(CLIENT_GARDENER)
#define ROLE "GARDENER"
#else
#define ROLE "OBSERVER"
#endif

/** @brief Runs one network worker or whole-garden observer. */
int main(int argc, char **argv) {
    int port;
#ifdef CLIENT_OBSERVER
    if (argc != 3 || !parse_port(argv[2], &port)) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }
#else
    char *end;
    long id;
    if (argc != 4 || !parse_port(argv[2], &port)) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <id>\n", argv[0]);
        return 1;
    }
    id = strtol(argv[3], &end, 10);
    if (*end || id < 1 || id > 100) {
        fprintf(stderr, "Invalid worker number\n");
        return 1;
    }
#endif
    signal(SIGPIPE, SIG_IGN);
    int fd = connect_server(argv[1], port);
    if (fd < 0) {
        perror("Cannot connect to TCP server");
        return 1;
    }
    char line[WORKER_LINE_SIZE];
#ifndef CLIENT_OBSERVER
    char reply[WORKER_LINE_SIZE];
#endif
#ifdef CLIENT_OBSERVER
    snprintf(line, sizeof(line), "HELLO %s\n", ROLE);
#else
    snprintf(line, sizeof(line), "HELLO %s %ld\n", ROLE, id);
#endif
    if (!send_line(fd, line)) {
        close(fd);
        return 1;
    }
    int accepted = 0, finished = 0;
    while (1) {
        int result = read_line(fd, line, sizeof(line));
        if (result == -1) continue;
        if (result == 0) break;
        if (strncmp(line, "FJOB ", 5) != 0 &&
            strncmp(line, "GJOB ", 5) != 0) {
            printf("%s\n", line);
            fflush(stdout);
        }
        if (strcmp(line, "WELCOME") == 0) accepted = 1;
        if (strncmp(line, "ERROR", 5) == 0) break;
        if (strncmp(line, "FINISH", 6) == 0) {
            finished = 1;
            break;
        }
#ifndef CLIENT_OBSERVER
        if (accepted && strncmp(line, "FJOB ", 5) == 0) {
#ifdef CLIENT_FLOWER
            if (!execute_flower_job(line, reply, sizeof(reply)) ||
                !send_line(fd, reply)) break;
#else
            break;
#endif
        } else if (accepted && strncmp(line, "GJOB ", 5) == 0) {
#ifdef CLIENT_GARDENER
            if (!execute_gardener_job(line, reply, sizeof(reply)) ||
                !send_line(fd, reply)) break;
#else
            break;
#endif
        }
#endif
    }
    close(fd);
    return accepted && finished ? 0 : 1;
}
