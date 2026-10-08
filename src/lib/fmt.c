#include "lib/fmt.h"
#include <stdbool.h>

struct out {
    char *buf;
    size_t cap;
    size_t len;
};

static void put(struct out *o, char c) {
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static void put_padded(struct out *o, const char *s, size_t n, int width, bool left, char pad) {
    int fill = width > (int)n ? width - (int)n : 0;
    if (!left)
        while (fill-- > 0)
            put(o, pad);
    for (size_t i = 0; i < n; i++)
        put(o, s[i]);
    if (left)
        while (fill-- > 0)
            put(o, ' ');
}

static void put_num(struct out *o, uint64_t v, bool neg, unsigned base, bool upper,
                    int width, bool left, bool zero) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24], s[25];
    size_t n = 0;
    do {
        tmp[n++] = digits[v % base];
        v /= base;
    } while (v);

    size_t len = 0;
    if (neg && !(zero && !left))
        s[len++] = '-';
    while (n)
        s[len++] = tmp[--n];

    if (neg && zero && !left) {
        put(o, '-');
        put_padded(o, s, len, width - 1, false, '0');
    } else {
        put_padded(o, s, len, width, left, zero && !left ? '0' : ' ');
    }
}

int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap) {
    struct out o = { buf, cap, 0 };

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            put(&o, *p);
            continue;
        }
        p++;

        bool left = false, zero = false;
        for (;; p++) {
            if (*p == '-')
                left = true;
            else if (*p == '0')
                zero = true;
            else
                break;
        }

        int width = 0;
        if (*p == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = true;
                width = -width;
            }
            p++;
        } else {
            while (*p >= '0' && *p <= '9')
                width = width * 10 + (*p++ - '0');
        }

        int prec = -1;
        if (*p == '.') {
            p++;
            prec = 0;
            if (*p == '*') {
                prec = va_arg(ap, int);
                p++;
            } else {
                while (*p >= '0' && *p <= '9')
                    prec = prec * 10 + (*p++ - '0');
            }
        }

        int size = 0;  // 0 int, 1 long, 2 long long, 3 size_t
        if (*p == 'l') {
            size = 1;
            if (*++p == 'l') {
                size = 2;
                p++;
            }
        } else if (*p == 'z') {
            size = 3;
            p++;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            int64_t v = size == 0 ? va_arg(ap, int)
                      : size == 1 ? va_arg(ap, long)
                      : size == 2 ? va_arg(ap, long long)
                      : (int64_t)va_arg(ap, size_t);
            uint64_t mag = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
            put_num(&o, mag, v < 0, 10, false, width, left, zero);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            uint64_t v = size == 0 ? va_arg(ap, unsigned)
                       : size == 1 ? va_arg(ap, unsigned long)
                       : size == 2 ? va_arg(ap, unsigned long long)
                       : va_arg(ap, size_t);
            put_num(&o, v, false, *p == 'u' ? 10 : 16, *p == 'X', width, left, zero);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            put(&o, '0');
            put(&o, 'x');
            put_num(&o, v, false, 16, false, 16, false, true);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            size_t n = 0;
            while (s[n] && (prec < 0 || n < (size_t)prec))
                n++;
            put_padded(&o, s, n, width, left, ' ');
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            put_padded(&o, &c, 1, width, left, ' ');
            break;
        }
        case '%':
            put(&o, '%');
            break;
        case '\0':
            p--;  // lone '%' at the end: stop
            break;
        default:
            put(&o, '%');
            put(&o, *p);
            break;
        }
    }

    if (cap)
        o.buf[o.len < cap ? o.len : cap - 1] = '\0';
    return (int)o.len;
}

int ksnprintf(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}

void fmt_size(char *buf, size_t cap, uint64_t bytes) {
    static const char *const units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    unsigned u = 0;
    while (u < 5 && (bytes >> (10 * (u + 1))) != 0)
        u++;
    if (u == 0) {
        ksnprintf(buf, cap, "%lu B", (unsigned long)bytes);
        return;
    }
    uint64_t whole = bytes >> (10 * u);
    uint64_t rem = bytes - (whole << (10 * u));
    uint64_t tenth = (rem * 10) >> (10 * u);
    ksnprintf(buf, cap, "%lu.%lu %s", (unsigned long)whole, (unsigned long)tenth, units[u]);
}

size_t copy_printable(char *dst, size_t cap, const char *src, size_t n) {
    if (cap == 0)
        return 0;
    size_t k = 0;
    for (size_t i = 0; i < n && src[i] && k + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[k++] = (c < 0x20 || c > 0x7e) ? '?' : (char)c;
    }
    while (k > 0 && dst[k - 1] == ' ')
        k--;
    dst[k] = '\0';
    return k;
}
