#include "ui/page.h"
#include <stdarg.h>
#include "lib/fmt.h"
#include "lib/string.h"

void page_clear(struct page *p) {
    p->title = "";
    p->count = 0;
    p->scroll = 0;
    p->truncated = false;
}

static struct line *next_line(struct page *p) {
    if (p->truncated)
        return NULL;
    if (p->count == PAGE_MAX_LINES - 1) {
        struct line *l = &p->lines[p->count++];
        l->style = STYLE_WARN;
        ksnprintf(l->text, sizeof l->text, "... (more lines not shown)");
        p->truncated = true;
        return NULL;
    }
    return &p->lines[p->count++];
}

void page_blank(struct page *p) {
    struct line *l = next_line(p);
    if (l) {
        l->style = STYLE_TEXT;
        l->text[0] = '\0';
    }
}

void page_add(struct page *p, enum line_style style, const char *fmt, ...) {
    struct line *l = next_line(p);
    if (!l)
        return;
    l->style = (uint8_t)style;
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(l->text, sizeof l->text, fmt, ap);
    va_end(ap);
}

void page_kv(struct page *p, const char *key, const char *fmt, ...) {
    struct line *l = next_line(p);
    if (!l)
        return;
    char value[PAGE_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(value, sizeof value, fmt, ap);
    va_end(ap);
    l->style = STYLE_TEXT;
    ksnprintf(l->text, sizeof l->text, "%-*.*s %s", PAGE_KEY_WIDTH - 1, PAGE_KEY_WIDTH - 1, key, value);
}

void page_wrap_words(struct page *p, const char *key, const char *const *words, size_t n, size_t width) {
    size_t avail = width > PAGE_KEY_WIDTH + 8 ? width - PAGE_KEY_WIDTH : 8;
    if (avail > PAGE_LINE_LEN - PAGE_KEY_WIDTH - 1)
        avail = PAGE_LINE_LEN - PAGE_KEY_WIDTH - 1;

    char buf[PAGE_LINE_LEN];
    size_t len = 0;
    bool first = true;
    for (size_t i = 0; i <= n; i++) {
        size_t wl = i < n ? strlen(words[i]) : 0;
        if (i == n || (len && len + 1 + wl > avail)) {
            if (len || (i == n && first)) {
                buf[len] = '\0';
                page_kv(p, first ? key : "", "%s", buf);
                first = false;
            }
            len = 0;
            if (i == n)
                break;
        }
        if (len)
            buf[len++] = ' ';
        size_t copy = wl > avail ? avail : wl;
        memcpy(buf + len, words[i], copy);
        len += copy;
    }
}

static size_t max_scroll(const struct page *p, size_t visible) {
    return p->count > visible ? p->count - visible : 0;
}

void page_scroll(struct page *p, long delta, size_t visible) {
    long target = (long)p->scroll + delta;
    long max = (long)max_scroll(p, visible);
    if (target < 0)
        target = 0;
    if (target > max)
        target = max;
    p->scroll = (size_t)target;
}

void page_scroll_to(struct page *p, bool end, size_t visible) {
    p->scroll = end ? max_scroll(p, visible) : 0;
}

bool page_at_end(const struct page *p, size_t visible) {
    return p->scroll >= max_scroll(p, visible);
}

void page_graph(struct page *p, const uint8_t *cells, size_t n) {
    struct line *l = next_line(p);
    if (!l)
        return;
    if (n > PAGE_LINE_LEN - 1)
        n = PAGE_LINE_LEN - 1;
    l->style = STYLE_GRAPH;
    l->text[0] = (char)n;
    memcpy(l->text + 1, cells, n);
}
