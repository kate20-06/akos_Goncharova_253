#include "simulation_internal.h"
#include "logger.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef WORKER_GARDENER
#define WORKER_GARDENER 0
#endif

/** @brief Reads a positive one-based flower or gardener index. */
static int parse_index(const char *text) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || !*text || *end || value < 1 || value > INT_MAX) return -1;
    return (int)value - 1;
}

/** @brief Attaches a worker to the requested session and runs its model. */
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
    SimulationContext simulation;
    int connected = 0;
    for (int retry = 0; retry < 50; ++retry) {
        if (connect_simulation(&simulation, argv[1])) {
            connected = 1;
            break;
        }
        struct timespec delay = {.tv_nsec = 20000000L};
        nanosleep(&delay, NULL);
    }
    if (!connected) {
        fprintf(stderr, "Cannot connect to session %s.\n", argv[1]);
        return EXIT_FAILURE;
    }
    int count = WORKER_GARDENER ? simulation.shared->settings.gardener_count
                                : simulation.shared->settings.flower_count;
    if (index >= count || !register_worker(&simulation, WORKER_GARDENER, index)) {
        fprintf(stderr, "Invalid or already registered worker %d.\n", index + 1);
        close_simulation(&simulation, 0);
        return EXIT_FAILURE;
    }
    char filename[64];
    snprintf(filename, sizeof(filename), "simulation_%s.log", argv[1]);
    logger_init(filename);
    int ok = WORKER_GARDENER
        ? gardener_worker(&simulation, index) : flower_worker(&simulation, index);
    logger_close();
    close_simulation(&simulation, 0);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
