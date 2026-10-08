#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Minimal printf: %d %i %u %x %X %p %s %c %%, length l/ll/z, flags - and 0,
// width and precision as digits or *. Returns the untruncated length and
// always NUL-terminates when cap > 0.
int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int ksnprintf(char *buf, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

// Human-readable binary size with one truncated decimal: "639.0 KiB".
void fmt_size(char *buf, size_t cap, uint64_t bytes);

// Copies at most n bytes of untrusted text (stopping at NUL), replacing
// bytes outside 0x20..0x7e with '?' and trimming trailing spaces.
size_t copy_printable(char *dst, size_t cap, const char *src, size_t n);
