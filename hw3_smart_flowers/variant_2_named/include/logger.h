#ifndef LOGGER_H
#define LOGGER_H

/**
 * @brief Opens the log file.
 * @param filename Log file name.
 */
void logger_init(const char *filename);

/**
 * @brief Closes the log file.
 */
void logger_close(void);

/**
 * @brief Writes an event to both the console and the log file.
 * @param format Format string compatible with printf.
 */
void log_event(const char *format, ...);

#endif
