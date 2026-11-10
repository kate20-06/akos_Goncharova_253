#include "simulation_internal.h"
#include "constants.h"
#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifndef WORKER_GARDENER
#define WORKER_GARDENER 0
#endif

/** @brief Parses a one-based worker number. */
static int parse_index(const char *text) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || !*text || *end || value < 1 ||
        value > (WORKER_GARDENER ? MAX_GARDENERS : MAX_FLOWERS)) return -1;
    return (int)value - 1;
}

/** @brief Joins an existing IPC session and runs the worker. */
int main(int argc, char *argv[]) {
    if (argc != 3 || !valid_session(argv[1])) {
        fprintf(stderr, "Usage: %s <session> <index>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int index = parse_index(argv[2]);
    if (index < 0) {
        fprintf(stderr, "Invalid worker index.\n");
        return EXIT_FAILURE;
    }
    /* An exclusive named semaphore prevents duplicate worker registration. */
    char ownership_name[80];
    snprintf(ownership_name, sizeof(ownership_name), "/sf4_%s_owner_%d_%d",
             argv[1], WORKER_GARDENER, index);
    sem_t *ownership = sem_open(ownership_name, O_CREAT | O_EXCL, 0600, 1);
    if (ownership == SEM_FAILED) {
        fprintf(stderr, "Worker %d is already registered.\n", index + 1);
        return EXIT_FAILURE;
    }
    SimulationContext simulation;
    int connected = 0;
    for (int retry = 0; retry < 50; ++retry) {
        if (connect_worker(&simulation, argv[1], WORKER_GARDENER, index)) {
            connected = 1;
            break;
        }
        struct timespec delay = {.tv_nsec = 20000000L};
        nanosleep(&delay, NULL);
    }
    if (!connected) {
        fprintf(stderr, "Cannot connect to session %s.\n", argv[1]);
        sem_close(ownership);
        sem_unlink(ownership_name);
        return EXIT_FAILURE;
    }
    char filename[64];
    snprintf(filename, sizeof(filename), "simulation_%s.log", argv[1]);
    logger_init(filename);
    int ok = worker_loop(&simulation, WORKER_GARDENER, index);
    logger_close();
    close_simulation(&simulation, 0);
    sem_close(ownership);
    sem_unlink(ownership_name);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
