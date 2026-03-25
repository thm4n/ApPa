#ifndef KLOG_H
#define KLOG_H

#include "../../libc/stdint.h"

// Log buffer size (number of messages)
#define KLOG_BUFFER_SIZE 256
#define KLOG_MSG_MAX_LEN 128

// Log levels
typedef enum {
    KLOG_DEBUG = 0,
    KLOG_INFO = 1,
    KLOG_WARN = 2,
    KLOG_ERROR = 3
} klog_level_t;

// Log entry structure
typedef struct {
    klog_level_t level;
    char message[KLOG_MSG_MAX_LEN];
    uint32_t timestamp;  // Can be tick count when timer is implemented
} klog_entry_t;

// Initialize kernel log system
void klog_init();

// Log a message with specific level
void klog(klog_level_t level, const char* format, ...);

// Convenience macros for different log levels
#define klog_debug(fmt, ...) klog(KLOG_DEBUG, fmt, ##__VA_ARGS__)
#define klog_info(fmt, ...)  klog(KLOG_INFO, fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)  klog(KLOG_WARN, fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...) klog(KLOG_ERROR, fmt, ##__VA_ARGS__)

// Get log statistics
void klog_get_stats(uint32_t* total_entries, uint32_t* current_size);

// Dump all log entries to screen
void klog_dump();

// Clear the log buffer
void klog_clear();

// Get a specific log entry (for reading back)
klog_entry_t* klog_get_entry(uint32_t index);

/**
 * klog_flush_to_file - Write all buffered log entries to a file
 * @filename: Target filename on the mounted filesystem (e.g. "klog.txt")
 *
 * Formats each entry as: "[LEVEL] <timestamp> <message>\n"
 * Creates the file if needed, then overwrites its contents.
 *
 * Returns: 0 on success, -1 on error
 */
int klog_flush_to_file(const char* filename);

#endif
