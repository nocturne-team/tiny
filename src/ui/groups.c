#include "ui/groups.h"
#include "lib/string.h"

int group_of(const struct ui_group *g, size_t ng, size_t page) {
    for (size_t i = 0; i < ng; i++)
        if (page >= g[i].first && page < (size_t)g[i].first + g[i].count)
            return (int)i;
    return -1;
}

size_t group_key(const struct ui_group *g, size_t ng, const uint8_t *last, size_t cur, size_t gi) {
    if (gi >= ng || !g[gi].count)
        return cur;
    if (group_of(g, ng, cur) == (int)gi)
        return g[gi].first + (cur - g[gi].first + 1) % g[gi].count;
    return group_of(g, ng, last[gi]) == (int)gi ? last[gi] : g[gi].first;
}

long group_digit(const struct ui_group *g, size_t ng, size_t cur, int digit) {
    int gi = group_of(g, ng, cur);
    if (gi < 0 || digit < 1 || digit > g[gi].count)
        return -1;
    return (long)g[gi].first + digit - 1;
}

size_t page_step(size_t npages, size_t cur, int delta) {
    if (!npages)
        return cur;
    long n = (long)npages, t = ((long)cur + delta) % n;
    return (size_t)(t < 0 ? t + n : t);
}

int label_at(const char *const *labels, size_t n, int x) {
    int start = 2;
    for (size_t i = 0; i < n; i++) {
        int w = (int)strlen(labels[i]);
        if (x >= start && x < start + w)
            return (int)i;
        start += w + 1;
    }
    return -1;
}
