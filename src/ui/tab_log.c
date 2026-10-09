#include "lib/logbuf.h"
#include "ui/tabs.h"

// Case-insensitive: does `s` contain `word` (lowercase)?
static bool contains(const char *s, const char *word) {
    for (; *s; s++) {
        size_t i = 0;
        while (word[i] && (s[i] | 0x20) == word[i])
            i++;
        if (!word[i])
            return true;
    }
    return false;
}

static bool is_problem(const char *s) {
    static const char *const words[] = { "timeout", "fail", "error", "panic", "halting" };
    for (size_t i = 0; i < sizeof words / sizeof *words; i++)
        if (contains(s, words[i]))
            return true;
    return false;
}

void tab_log_build(struct page *p, const struct logbuf *lb) {
    page_clear(p);
    p->title = "Log";
    size_t n = logbuf_count(lb);
    uint64_t dropped = logbuf_dropped(lb);
    if (dropped)
        page_add(p, STYLE_HEAD, "Kernel log (%zu lines, %llu older dropped)", n,
                 (unsigned long long)dropped);
    else
        page_add(p, STYLE_HEAD, "Kernel log (%zu lines)", n);
    for (size_t i = 0; i < n; i++) {
        const struct log_entry *e = logbuf_get(lb, i);
        uint64_t s = e->ms / 1000;
        page_add(p, is_problem(e->text) ? STYLE_WARN : STYLE_TEXT, "%5llu.%03llu  %s",
                 (unsigned long long)s, (unsigned long long)(e->ms % 1000), e->text);
    }
}
