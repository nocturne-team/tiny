#include "ui/pointer.h"
#include "lib/fmt.h"

void pointer_init(struct pointer *p, int cols, int rows) {
    p->x = cols / 2;
    p->y = rows / 2;
    p->fx = p->fy = 0;
    p->visible = false;
}

// Moves one axis by whole cells from the accumulated counts, keeping the remainder.
static int axis(int pos, int *frac, int delta, int per_cell, int limit) {
    int total = *frac + delta;
    int cells = total / per_cell;  // truncates toward zero
    *frac = total - cells * per_cell;
    pos += cells;
    if (pos < 0) {
        pos = 0;
        *frac = 0;
    } else if (pos > limit - 1) {
        pos = limit - 1;
        *frac = 0;
    }
    return pos;
}

bool pointer_move(struct pointer *p, int dx, int dy, int cols, int rows) {
    int ox = p->x, oy = p->y;
    bool was_visible = p->visible;
    p->x = axis(p->x, &p->fx, dx, POINTER_COUNTS_X, cols);
    p->y = axis(p->y, &p->fy, dy, POINTER_COUNTS_Y, rows);
    p->visible = true;
    return !was_visible || p->x != ox || p->y != oy;
}

long pointer_wheel_lines(int wheel) {
    return -(long)wheel * POINTER_WHEEL_LINES;
}

int pointer_tab_at(const char *const *titles, size_t n, int x) {
    int start = 2;
    for (size_t i = 0; i < n; i++) {
        char label[24];
        int w = ksnprintf(label, sizeof label, " F%zu %s ", i + 1, titles[i]);
        if (w > (int)sizeof label - 1)
            w = (int)sizeof label - 1;  // draw_tabs shows the truncated label
        if (x >= start && x < start + w)
            return (int)i;
        start += w + 1;
    }
    return -1;
}
