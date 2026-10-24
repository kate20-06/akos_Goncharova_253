#include "logger.h"

#include "constants.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static int log_fd = -1;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* A write() is allowed to transfer fewer bytes than requested. */
static void write_all(int fd, const char *buffer, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, buffer, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        buffer += written;
        length -= (size_t)written;
    }
}

/** @brief Opens or truncates the event log for the current run. */
void logger_init(const char *filename) {
    if (filename == NULL) return;

    pthread_mutex_lock(&log_mutex);
    log_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd == -1) perror("Не удалось открыть файл журнала");
    pthread_mutex_unlock(&log_mutex);
}

/** @brief Closes the event log after all worker threads have stopped. */
void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
    pthread_mutex_unlock(&log_mutex);
}

/** @brief Serializes one formatted event to the console and log file. */
void log_event(const char *format, ...) {
    char buffer[LOG_BUFFER_SIZE];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length < 0) return;
    if (length >= (int)sizeof(buffer)) length = (int)sizeof(buffer) - 1;

    /* One complete event cannot interleave with another worker's event. */
    pthread_mutex_lock(&log_mutex);
    write_all(STDOUT_FILENO, buffer, (size_t)length);
    if (log_fd != -1) write_all(log_fd, buffer, (size_t)length);
    pthread_mutex_unlock(&log_mutex);
}
