#include "logger.h"
#include <stdarg.h>
#include <string.h>
#include <errno.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Get log level string
static const char *get_level_str(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR";
        default:          return "UNKNOWN";
    }
}

void logger_init(const char *log_file_path) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
    }
    log_file = fopen(log_file_path, "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
    }
    pthread_mutex_unlock(&log_mutex);
}

void logger_close() {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

void logger_log(log_level_t level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Print to log file if open
    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", time_str, get_level_str(level));
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    
    // Always print errors to stderr
    if (level == LOG_ERROR) {
        fprintf(stderr, "[%s] [%s] ", time_str, get_level_str(level));
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
    
    pthread_mutex_unlock(&log_mutex);
} 