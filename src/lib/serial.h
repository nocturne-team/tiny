#pragma once

void serial_init(void);
void serial_putc(char c);
// Writes a NUL-terminated string, turning "\n" into "\r\n".
void serial_write(const char *s);

// printf-style log line to COM1; a newline is appended.
struct logbuf;
// Everything klog printed so far (the most recent LOGBUF_LINES lines).
const struct logbuf *klog_buffer(void);
// Prints a line to the serial port and keeps it in the log buffer.
void klog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
