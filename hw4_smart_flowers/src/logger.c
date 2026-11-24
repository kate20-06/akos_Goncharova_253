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

/** @brief Writes a complete event, retrying interrupted calls. */
static void write_all(int fd, const char *text, size_t length) {
    while (length) {
        ssize_t n = write(fd, text, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        text += n;
        length -= (size_t)n;
    }
}

/** @brief Opens the log file for the server. */
void logger_init(const char *filename) {
    pthread_mutex_lock(&log_mutex);
    if (filename) log_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    pthread_mutex_unlock(&log_mutex);
}

/** @brief Closes the server log. */
void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_fd >= 0) close(log_fd);
    log_fd = -1;
    pthread_mutex_unlock(&log_mutex);
}

/** @brief Prints and logs one event without interleaving threads. */
void log_event(const char *format, ...) {
    char buffer[LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n <= 0) return;
    if (n >= (int)sizeof(buffer)) n = (int)sizeof(buffer) - 1;
    pthread_mutex_lock(&log_mutex);
    write_all(STDOUT_FILENO, buffer, (size_t)n);
    if (log_fd >= 0) write_all(log_fd, buffer, (size_t)n);
    pthread_mutex_unlock(&log_mutex);
}
