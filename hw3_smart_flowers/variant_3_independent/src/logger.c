#include "logger.h"
#include "constants.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static int log_fd = -1;

/** @brief Writes the full event, retrying interrupted writes. */
static void write_all(int fd, const char *buffer, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, buffer, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        buffer += written;
        length -= (size_t)written;
    }
}

/** @brief Opens the shared log file before child processes are created. */
void logger_init(const char *filename) {
    if (filename == NULL) return;
    log_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) perror("Не удалось открыть файл журнала");
}

/** @brief Closes the event log in the parent process. */
void logger_close(void) {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}

/** @brief Writes one event to the terminal and log file. */
void log_event(const char *format, ...) {
    char buffer[LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length < 0) return;
    if (length >= (int)sizeof(buffer)) length = (int)sizeof(buffer) - 1;

    /* Model updates are logged while holding the shared semaphore. */
    write_all(STDOUT_FILENO, buffer, (size_t)length);
    if (log_fd != -1) write_all(log_fd, buffer, (size_t)length);
}
