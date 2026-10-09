#include "lib/serial.h"
#include "arch/x86.h"
#include <stdbool.h>
#include <stdarg.h>
#include "info/tsc.h"
#include "lib/fmt.h"
#include "lib/logbuf.h"

#define COM1 0x3F8

static struct logbuf klog_buf;
static bool dead;  // no UART here: drop output instead of waiting on it

void serial_init(void) {
    outb(COM1 + 1, 0x00);  // no interrupts
    outb(COM1 + 3, 0x80);  // DLAB on
    outb(COM1 + 0, 0x01);  // divisor 1 = 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  // 8N1, DLAB off
    outb(COM1 + 2, 0xC7);  // enable and clear FIFOs
    outb(COM1 + 4, 0x03);  // DTR + RTS
    outb(COM1 + 7, 0xAE);  // scratch register: reads back only if a UART exists
    dead = inb(COM1 + 7) != 0xAE;
}

void serial_putc(char c) {
    if (dead)
        return;
    int i = 0;
    while (!(inb(COM1 + 5) & 0x20))
        if (++i == 100000) {
            dead = true;  // transmitter never drained: stop waiting on every byte
            return;
        }
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
    logbuf_add(&klog_buf, tsc_ms(), buf);
}

const struct logbuf *klog_buffer(void) {
    return &klog_buf;
}
