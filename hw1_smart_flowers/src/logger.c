#include "logger.h"

#include "constants.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static int log_fd = -1;

void logger_init(const char *filename) {
    if (filename == NULL) {
        return;
    }

    log_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd == -1) {
        perror("Не удалось открыть файл журнала");
    }
}

void logger_close(void) {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}

void log_event(const char *format, ...) {
    char buffer[LOG_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) {
        return;
    }

    if (length >= (int)sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
    }

    write(STDOUT_FILENO, buffer, (size_t)length);

    if (log_fd != -1) {
        write(log_fd, buffer, (size_t)length);
    }
}
