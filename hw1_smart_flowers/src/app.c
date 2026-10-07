#include "app.h"

#include "constants.h"
#include "input.h"
#include "logger.h"
#include "simulation.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief Handles Ctrl+C by requesting safe simulation termination. */
static void handle_sigint(int signal_number) {
    (void)signal_number;
    request_stop();
}

/** @brief Reads settings either interactively or from a configuration file. */
static int load_settings(int argc, char *argv[], Settings *settings) {
    if (argc > 2) {
        fprintf(stderr, "Использование: %s [файл_параметров]\n", argv[0]);
        return 0;
    }

    if (argc == 2) {
        return read_settings_from_file(argv[1], settings);
    }

    return read_settings(settings);
}

/** @brief Installs the Ctrl+C handler. */
static int install_signal_handlers(void) {
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("Не удалось установить обработчик SIGINT");
        return 0;
    }

    return 1;
}

int run_application(int argc, char *argv[]) {
    Settings settings;

    printf("=== Моделирование умных цветов ===\n\n");

    if (!load_settings(argc, argv, &settings)) {
        fprintf(stderr, "Ошибка ввода данных.\n");
        return EXIT_FAILURE;
    }

    if (!validate_settings(&settings)) {
        fprintf(stderr, "Введены недопустимые параметры.\n");
        return EXIT_FAILURE;
    }

    if (!install_signal_handlers()) {
        return EXIT_FAILURE;
    }

    logger_init(DEFAULT_LOG_FILE);

    print_settings(&settings);
    printf("Для досрочного завершения нажмите Ctrl+C.\n");
    printf("Журнал событий: %s\n", DEFAULT_LOG_FILE);

    run_simulation(&settings);

    logger_close();
    return EXIT_SUCCESS;
}
