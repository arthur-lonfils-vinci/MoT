/**
 * @file logger.h
 * @brief Thread-safe logging system with file rotation.
 */

#ifndef LOGGER_H
#define LOGGER_H

/**
 * @brief Log severity levels.
 */
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

/**
 * @brief Initializes the logger.
 * @param basename The base name for log files (e.g., "server", "client_u1").
 */
void log_init(const char *basename);

/**
 * @brief Closes the current log file.
 */
void log_close(void);

/**
 * @brief Changes the log filename (e.g., after user login to include UID).
 */
void log_update_name(const char *new_basename);

// Internal function, use the macro below instead
void log_message_internal(LogLevel level, const char *file, int line, const char *fmt, ...);

/**
 * @brief Logs a message with timestamp, level, and file location.
 * @param level Severity (LOG_INFO, etc.).
 * @param fmt Printf-style format string.
 */
#define log_print(level, fmt, ...) \
    log_message_internal(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif