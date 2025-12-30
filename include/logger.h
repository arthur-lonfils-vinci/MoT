/* include/logger.h */
#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

void log_init(const char *basename);
void log_close(void);

void log_update_name(const char *new_basename);

// Internal function, use the macro below instead
void log_message_internal(LogLevel level, const char *file, int line, const char *fmt, ...);

// Macro to automatically capture file and line number
#define log_print(level, fmt, ...) \
    log_message_internal(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif