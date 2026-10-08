#include "lib/serial.h"
#include "arch/x86.h"
#include <stdarg.h>
#include "lib/fmt.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);  // no interrupts
    outb(COM1 + 3, 0x80);  // DLAB on
    outb(COM1 + 0, 0x01);  // divisor 1 = 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  // 8N1, DLAB off
    outb(COM1 + 2, 0xC7);  // enable and clear FIFOs
    outb(COM1 + 4, 0x03);  // DTR + RTS
}

void serial_putc(char c) {
    // Bounded wait: on machines without a UART the status port floats.
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++)
        ;
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    for (; *s; s++) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s);
    }
}

void klog(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    serial_write(buf);
    serial_write("\n");
}
