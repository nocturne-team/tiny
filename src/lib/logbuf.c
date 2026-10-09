#include "lib/logbuf.h"

static struct log_entry *next_entry(struct logbuf *lb, uint64_t ms) {
    struct log_entry *e = &lb->e[lb->seq++ % LOGBUF_LINES];
    e->ms = ms;
    e->text[0] = '\0';
    return e;
}

void logbuf_add(struct logbuf *lb, uint64_t ms, const char *text) {
    struct log_entry *e = next_entry(lb, ms);
    size_t len = 0;
    for (const char *s = text; *s; s++) {
        char c = *s;
        if (c == '\n') {
            if (s[1])
                e = next_entry(lb, ms), len = 0;
            continue;
        }
        if (c == '\r')
            continue;
        if (c == '\t')
            c = ' ';
        else if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e)
            c = '?';
        if (len < LOGBUF_LINE_LEN - 1) {
            e->text[len++] = c;
            e->text[len] = '\0';
        }
    }
}

size_t logbuf_count(const struct logbuf *lb) {
    return lb->seq < LOGBUF_LINES ? (size_t)lb->seq : LOGBUF_LINES;
}

uint64_t logbuf_dropped(const struct logbuf *lb) {
    return lb->seq - logbuf_count(lb);
}

const struct log_entry *logbuf_get(const struct logbuf *lb, size_t i) {
    if (i >= logbuf_count(lb))
        return NULL;
    return &lb->e[(logbuf_dropped(lb) + i) % LOGBUF_LINES];
}
