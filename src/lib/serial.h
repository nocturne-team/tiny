#pragma once

void serial_init(void);
void serial_putc(char c);
// Writes a NUL-terminated string, turning "\n" into "\r\n".
void serial_write(const char *s);
