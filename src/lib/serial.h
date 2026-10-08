#pragma once

void serial_init(void);
void serial_putc(char c);
// Writes a NUL-terminated string, turning "\n" into "\r\n".
void serial_write(const char *s);

// printf-style log line to COM1; a newline is appended.
void klog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
