#pragma once
#include <stddef.h>
#include <stdint.h>

#define LOGBUF_LINES 256
#define LOGBUF_LINE_LEN 120  // bytes per line, including the NUL

struct log_entry {
    uint64_t ms;  // timestamp, milliseconds since boot
    char text[LOGBUF_LINE_LEN];
};

// Ring of the most recent log lines. Zero-initialised is empty.
struct logbuf {
    uint64_t seq;  // lines ever added; changes whenever a line arrives
    struct log_entry e[LOGBUF_LINES];
};

// Pure: appends `text` split at '\n' (a trailing '\n' adds no empty line). Long
// lines are cut; CR is dropped, tab becomes a space, other control bytes '?'.
void logbuf_add(struct logbuf *lb, uint64_t ms, const char *text);
// Lines currently held (at most LOGBUF_LINES).
size_t logbuf_count(const struct logbuf *lb);
// Lines pushed out of the ring.
uint64_t logbuf_dropped(const struct logbuf *lb);
// Line i, 0 = oldest held; NULL when i >= logbuf_count.
const struct log_entry *logbuf_get(const struct logbuf *lb, size_t i);
