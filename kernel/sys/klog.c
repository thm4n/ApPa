#include "klog.h"
#include "../../libc/stdio.h"
#include "../../libc/stdarg.h"
#include "../../libc/string.h"
#include "../../drivers/screen.h"
#include "../../fs/simplefs.h"

// Circular buffer for log entries
static klog_entry_t log_buffer[KLOG_BUFFER_SIZE];
static uint32_t log_head = 0;  // Next write position
static uint32_t log_count = 0; // Total messages logged (can exceed buffer size)
static uint32_t current_timestamp = 0;  // Simple counter until timer is implemented

// Log level names for display
static const char* level_names[] = {
    "[DEBUG]",
    "[INFO] ",
    "[WARN] ",
    "[ERROR]"
};

// Log level colors (can be used later)
static const char level_prefixes[] = {
    'D',  // Debug
    'I',  // Info
    'W',  // Warn
    'E'   // Error
};

void klog_init() {
    // Clear the log buffer
    memset(log_buffer, 0, sizeof(log_buffer));
    log_head = 0;
    log_count = 0;
    current_timestamp = 0;
}

void klog(klog_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Get current log entry
    klog_entry_t* entry = &log_buffer[log_head];
    
    // Set level and timestamp
    entry->level = level;
    entry->timestamp = current_timestamp++;
    
    // Format the message using a simple approach
    // We'll build the message character by character
    char temp_buffer[KLOG_MSG_MAX_LEN];
    int pos = 0;
    
    for (int i = 0; format[i] != '\0' && pos < KLOG_MSG_MAX_LEN - 1; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;  // Move to format specifier
            
            switch (format[i]) {
                case 'd':
                case 'i': {
                    int val = va_arg(args, int);
                    char num_buf[32];
                    itoa(val, num_buf, 10);
                    for (int j = 0; num_buf[j] != '\0' && pos < KLOG_MSG_MAX_LEN - 1; j++) {
                        temp_buffer[pos++] = num_buf[j];
                    }
                    break;
                }
                
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    char num_buf[32];
                    utoa(val, num_buf, 10);
                    for (int j = 0; num_buf[j] != '\0' && pos < KLOG_MSG_MAX_LEN - 1; j++) {
                        temp_buffer[pos++] = num_buf[j];
                    }
                    break;
                }
                
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char num_buf[32];
                    utoa(val, num_buf, 16);
                    for (int j = 0; num_buf[j] != '\0' && pos < KLOG_MSG_MAX_LEN - 1; j++) {
                        temp_buffer[pos++] = num_buf[j];
                    }
                    break;
                }
                
                case 's': {
                    char* str = va_arg(args, char*);
                    if (str) {
                        for (int j = 0; str[j] != '\0' && pos < KLOG_MSG_MAX_LEN - 1; j++) {
                            temp_buffer[pos++] = str[j];
                        }
                    }
                    break;
                }
                
                case 'c': {
                    char c = (char)va_arg(args, int);
                    temp_buffer[pos++] = c;
                    break;
                }
                
                case '%': {
                    temp_buffer[pos++] = '%';
                    break;
                }
                
                default:
                    temp_buffer[pos++] = '%';
                    temp_buffer[pos++] = format[i];
                    break;
            }
        } else {
            temp_buffer[pos++] = format[i];
        }
    }
    temp_buffer[pos] = '\0';
    
    va_end(args);
    
    // Copy to log entry
    strncpy(entry->message, temp_buffer, KLOG_MSG_MAX_LEN - 1);
    entry->message[KLOG_MSG_MAX_LEN - 1] = '\0';
    
    // Also print to screen with level prefix (use k print to avoid recursion)
    kprint(level_names[level]);
    kprint(" ");
    kprint(entry->message);
    kprint("\n");
    
    // Advance circular buffer
    log_head = (log_head + 1) % KLOG_BUFFER_SIZE;
    log_count++;
}

void klog_get_stats(uint32_t* total_entries, uint32_t* current_size) {
    if (total_entries) {
        *total_entries = log_count;
    }
    if (current_size) {
        *current_size = (log_count < KLOG_BUFFER_SIZE) ? log_count : KLOG_BUFFER_SIZE;
    }
}

void klog_dump() {
    kprintf("\n=== Kernel Log Dump ===\n");
    
    uint32_t total, size;
    klog_get_stats(&total, &size);
    
    kprintf("Total messages logged: %u\n", total);
    kprintf("Messages in buffer: %u\n", size);
    kprintf("\n");
    
    // Determine start position for reading
    uint32_t start_pos;
    if (log_count <= KLOG_BUFFER_SIZE) {
        start_pos = 0;
    } else {
        start_pos = log_head;  // Start from oldest message
    }
    
    // Print all messages
    for (uint32_t i = 0; i < size; i++) {
        uint32_t index = (start_pos + i) % KLOG_BUFFER_SIZE;
        klog_entry_t* entry = &log_buffer[index];
        
        kprintf("[%u] %s %s\n", 
                entry->timestamp,
                level_names[entry->level],
                entry->message);
    }
    
    kprintf("\n=== End of Log ===\n");
}

void klog_clear() {
    memset(log_buffer, 0, sizeof(log_buffer));
    log_head = 0;
    log_count = 0;
    current_timestamp = 0;
    kprintf("[INFO]  Log buffer cleared\n");
}

klog_entry_t* klog_get_entry(uint32_t index) {
    uint32_t size = (log_count < KLOG_BUFFER_SIZE) ? log_count : KLOG_BUFFER_SIZE;
    
    if (index >= size) {
        return NULL;
    }
    
    uint32_t start_pos;
    if (log_count <= KLOG_BUFFER_SIZE) {
        start_pos = 0;
    } else {
        start_pos = log_head;
    }
    
    uint32_t actual_index = (start_pos + index) % KLOG_BUFFER_SIZE;
    return &log_buffer[actual_index];
}

int klog_flush_to_file(const char* filename) {
    if (!filename) return -1;

    uint32_t total, size;
    klog_get_stats(&total, &size);
    if (size == 0) return 0;  /* Nothing to flush */

    /* Build the entire log as a single text blob.
     * Max per line: "[ERROR] " (8) + timestamp ~10 + " " + msg 128 + "\n" ≈ 150
     * Worst case: 256 entries × 150 = 38400 bytes.
     * We use a 4 KB static buffer and flush in chunks. */
    #define FLUSH_BUF_SIZE 4096
    static char flush_buf[FLUSH_BUF_SIZE];
    uint32_t pos = 0;

    /* Determine start position (oldest entry in ring buffer) */
    uint32_t start_pos;
    if (log_count <= KLOG_BUFFER_SIZE) {
        start_pos = 0;
    } else {
        start_pos = log_head;
    }

    /* First pass: build complete output in flush_buf.
     * If it overflows, truncate (acceptable for v1). */
    for (uint32_t i = 0; i < size && pos < FLUSH_BUF_SIZE - 2; i++) {
        uint32_t idx = (start_pos + i) % KLOG_BUFFER_SIZE;
        klog_entry_t* entry = &log_buffer[idx];

        /* "[LEVEL] " */
        const char* lvl = level_names[entry->level];
        for (int j = 0; lvl[j] && pos < FLUSH_BUF_SIZE - 2; j++)
            flush_buf[pos++] = lvl[j];
        if (pos < FLUSH_BUF_SIZE - 2) flush_buf[pos++] = ' ';

        /* Timestamp as decimal */
        char ts_buf[16];
        utoa(entry->timestamp, ts_buf, 10);
        for (int j = 0; ts_buf[j] && pos < FLUSH_BUF_SIZE - 2; j++)
            flush_buf[pos++] = ts_buf[j];
        if (pos < FLUSH_BUF_SIZE - 2) flush_buf[pos++] = ' ';

        /* Message */
        for (int j = 0; entry->message[j] && pos < FLUSH_BUF_SIZE - 2; j++)
            flush_buf[pos++] = entry->message[j];

        /* Newline */
        if (pos < FLUSH_BUF_SIZE - 1) flush_buf[pos++] = '\n';
    }
    flush_buf[pos] = '\0';

    /* Ensure file exists */
    fs_entry_t stat;
    if (fs_stat(filename, &stat) != 0) {
        if (fs_create(filename, FS_TYPE_FILE) != 0) return -1;
    }

    /* Write the log data */
    if (fs_write_file(filename, flush_buf, pos) != 0) return -1;

    return 0;
    #undef FLUSH_BUF_SIZE
}
