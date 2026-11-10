#include "config_parser.h"
#include "constants.h"
#include "input.h"
#include "logger.h"
#include "simulation.h"
#include "simulation_internal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/** @brief Handles Ctrl+C in the controller. */
static void handle_sigint(int signo) {
    (void)signo;
    request_stop();
}

/** @brief Starts a session that independent workers can join. */
int main(int argc, char *argv[]) {
    /* A session name alone enables interactive input; a config file is optional. */
    const char *session = argc == 2 ? argv[1] : argc == 3 ? argv[2] : NULL;
    if (session == NULL || !valid_session(session)) {
        fprintf(stderr, "Usage: %s [config.cfg] <session>\n", argv[0]);
        return EXIT_FAILURE;
    }
    Settings settings;
    int read_ok;
    if (argc == 2) {
        printf("=== Многопроцессное моделирование умных цветов ===\n\n");
        read_ok = read_settings(&settings);
    } else {
        read_ok = read_settings_from_file(argv[1], &settings);
    }
    if (!read_ok || !validate_settings(&settings)) return EXIT_FAILURE;
    struct sigaction action = {.sa_handler = handle_sigint};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) return EXIT_FAILURE;

    char filename[64];
    snprintf(filename, sizeof(filename), "simulation_%s.log", session);
    /* Only the controller truncates the log; workers append their events. */
    unlink(filename);
    logger_init(filename);
    print_settings(&settings);
    fflush(stdout);
    int ok = run_simulation(&settings, session);
    logger_close();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
