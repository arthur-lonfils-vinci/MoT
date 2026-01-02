#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "system/logger.h"

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char log_basename[64];
static char current_log_date[16]; // Stores "YYYY-MM-DD"

// Helper to get current date string
static void get_date_str(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d", t);
}

// Helper to open/rotate the log file
static void open_log_file() {
    char date_str[16];
    get_date_str(date_str, sizeof(date_str));

    // If date hasn't changed and file is open, do nothing
    if (log_file && strcmp(date_str, current_log_date) == 0) {
        return;
    }

    // Close old file if open
    if (log_file) {
        fclose(log_file);
    }

    // Create "log" directory if it doesn't exist
    struct stat st = {0};
    if (stat("log", &st) == -1) {
        mkdir("log", 0755);
    }

    // Update current date
    strcpy(current_log_date, date_str);

    // Open new file: log/basename_YYYY-MM-DD.log
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "log/%s_%s.log", log_basename, current_log_date);
    
    log_file = fopen(filepath, "a");
    if (!log_file) {
        fprintf(stderr, "ERROR: Could not open log file %s\n", filepath);
    }
}

void log_update_name(const char *new_basename) {
    pthread_mutex_lock(&log_mutex);
    
    // 1. Close the current file to ensure all data is written
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    // 2. Construct file paths
    char old_filepath[256];
    snprintf(old_filepath, sizeof(old_filepath), "log/%s_%s.log", log_basename, current_log_date);
    
    char new_filepath[256];
    snprintf(new_filepath, sizeof(new_filepath), "log/%s_%s.log", new_basename, current_log_date);

    // 3. Move Content: Read from Old, Append to New
    FILE *f_old = fopen(old_filepath, "rb");
    FILE *f_new = fopen(new_filepath, "ab"); // 'ab' creates if not exists, appends if it does

    if (f_old && f_new) {
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), f_old)) > 0) {
            fwrite(buffer, 1, n, f_new);
        }
    }

    if (f_old) fclose(f_old);
    if (f_new) fclose(f_new);

    // 4. Remove the old file
    remove(old_filepath);

    // 5. Update the basename in memory
    strncpy(log_basename, new_basename, 63);
    log_basename[63] = '\0';
    
    // 6. Re-open the global log_file pointer (points to new path now)
    open_log_file();
    
    pthread_mutex_unlock(&log_mutex);
}

void log_init(const char *basename) {
    pthread_mutex_lock(&log_mutex);
    strncpy(log_basename, basename, 63);
    open_log_file();
    pthread_mutex_unlock(&log_mutex);
}

void log_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_message_internal(LogLevel level, const char *file, int line, const char *fmt, ...) {
    pthread_mutex_lock(&log_mutex);

    // Check for rotation before writing
    open_log_file();

    FILE *out = log_file ? log_file : (level == LOG_ERROR ? stderr : stdout);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    const char *level_str;
    switch(level) {
        case LOG_INFO:  level_str = "INFO"; break;
        case LOG_WARN:  level_str = "WARN"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_DEBUG: level_str = "DEBUG"; break;
        default:        level_str = "UNKNOWN"; break;
    }

    fprintf(out, "[%s] [%s] [%s:%d] ", time_str, level_str, file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    pthread_mutex_unlock(&log_mutex);
}