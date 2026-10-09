#include "ui/screen_test.h"

#define GRID_STEP 32

enum { P_CARD, P_BLACK, P_GREY = 6, P_GRADIENTS, P_CHECKER, P_GRID };

static const char *const names[SCREEN_PATTERNS] = {
    "Test card", "Black", "White", "Red", "Green", "Blue", "Grey 50%", "Gradients",
    "Checkerboard 1 px", "Grid 32 px, red edge",
};
static const uint32_t solids[] = { 0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080 };
static const uint32_t bars[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00,
                                  0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };

const char *screen_pattern_name(int pattern) {
    return pattern >= 0 && pattern < SCREEN_PATTERNS ? names[pattern] : "?";
}

bool screen_pattern_solid(int pattern) {
    return pattern >= P_BLACK && pattern <= P_GREY;
}

// a mod m for any sign of a (m > 0).
static int64_t mod(int64_t a, int64_t m) {
    int64_t r = a % m;
    return r < 0 ? r + m : r;
}

// An original card in the spirit of Test Card F (no picture): castellated edge,
// grey background with a grid, and a circle holding colour bars, a dark centre with
// a crosshair (tiny writes its name there), a grey wedge, gratings and a black/white split.
static uint32_t card(int64_t x, int64_t y, int64_t w, int64_t h) {
    int64_t b = h / 20 > 0 ? h / 20 : 1;    // castellation block
    int64_t g = h / 12 > 1 ? h / 12 : 2;    // grid step
    int64_t r = h * 5 / 12;                 // circle radius
    int64_t cx = w / 2, cy = h / 2, dx = x - cx, dy = y - cy;

    if (x < b || y < b || x >= w - b || y >= h - b)
        return (x / b + y / b) & 1 ? 0xFFFFFF : 0x000000;
    int64_t d2 = dx * dx + dy * dy;
    if (r < 10 || d2 > r * r)
        return mod(dx, g) == 0 || mod(dy, g) == 0 ? 0xFFFFFF : 0x7F7F7F;
    if (d2 >= (r - 2) * (r - 2))
        return 0xFFFFFF;  // the circle's outline, 2 px

    int64_t u = x - (cx - r);  // 0 .. 2r across the circle
    if (dy < -r * 2 / 5)
        return bars[u * 8 / (2 * r) & 7];
    if (dy < r / 5)
        return dx == 0 || dy == 0 ? 0xFFFFFF : 0x202020;
    if (dy < r * 2 / 5)
        return (uint32_t)(u * 6 / (2 * r) * 255 / 5) * 0x010101;  // six grey steps
    if (dy >= r * 13 / 30 && dy < r * 7 / 10) {
        int64_t block = u * 6 / (2 * r), half = 6 - block;  // stripes 6 px wide down to 1
        int64_t x0 = cx - r + (block * 2 * r + 5) / 6;
        return (x - x0) / half % 2 == 0 ? 0xFFFFFF : 0x000000;
    }
    if (dy >= r * 11 / 15)
        return dx < 0 ? 0x000000 : 0xFFFFFF;
    return 0x202020;
}

uint32_t screen_pixel(int pattern, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (screen_pattern_solid(pattern))
        return solids[pattern - P_BLACK];
    switch (pattern) {
    case P_CARD:
        return card(x, y, w, h);
    case P_GRADIENTS: {  // red, green, blue and grey ramps, one band each
        uint32_t v = w > 1 ? x * 255 / (w - 1) : 0;
        static const uint32_t mask[4] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF };
        return (v * 0x010101) & mask[h ? y * 4 / h : 0];
    }
    case P_CHECKER:
        return (x ^ y) & 1 ? 0xFFFFFF : 0x000000;
    case P_GRID:
        if (x == 0 || y == 0 || x + 1 == w || y + 1 == h)
            return 0xFF0000;
        return x % GRID_STEP == 0 || y % GRID_STEP == 0 ? 0xFFFFFF : 0x000000;
    default:
        return 0;
    }
}

enum screen_action screen_key(const struct raw_key *ev) {
    if (!ev->down || (ev->flags & RAW_REPEAT))
        return SCREEN_NONE;
    switch (ev->usage) {
    case 0x2C:  // Space
    case 0x4F:  // Right
    case 0x4E:  // PgDn
    case 0x51:  // Down
        return SCREEN_NEXT;
    case 0x2A:  // Backspace
    case 0x50:  // Left
    case 0x4B:  // PgUp
    case 0x52:  // Up
        return SCREEN_PREV;
    case 0x29:  // Esc
    case 0x14:  // Q
        return SCREEN_LEAVE;
    default:
        return SCREEN_NONE;
    }
}

int screen_step(int cur, enum screen_action a) {
    switch (a) {
    case SCREEN_NEXT: return cur + 1 < SCREEN_PATTERNS ? cur + 1 : -1;
    case SCREEN_PREV: return cur > 0 ? cur - 1 : 0;
    case SCREEN_LEAVE: return -1;
    default: return cur;
    }
}

enum screen_native screen_native(uint64_t fb_w, uint64_t fb_h, uint16_t pref_w, uint16_t pref_h) {
    if (!pref_w || !pref_h)
        return SCREEN_NATIVE_UNKNOWN;
    return fb_w == pref_w && fb_h == pref_h ? SCREEN_NATIVE_YES : SCREEN_NATIVE_NO;
}
