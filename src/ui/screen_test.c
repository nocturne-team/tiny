#include "ui/screen_test.h"

#define GRID_STEP 32

static const char *const names[SCREEN_PATTERNS] = {
    "Black", "White", "Red", "Green", "Blue", "Grey 50%", "Gradients", "Checkerboard 1 px",
    "Grid 32 px, red edge",
};
static const uint32_t solids[] = { 0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080 };
#define SOLIDS (int)(sizeof solids / sizeof solids[0])

const char *screen_pattern_name(int pattern) {
    return pattern >= 0 && pattern < SCREEN_PATTERNS ? names[pattern] : "?";
}

bool screen_pattern_solid(int pattern) {
    return pattern >= 0 && pattern < SOLIDS;
}

uint32_t screen_pixel(int pattern, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (screen_pattern_solid(pattern))
        return solids[pattern];
    switch (pattern) {
    case 6: {  // red, green, blue and grey ramps, one band each
        uint32_t v = w > 1 ? x * 255 / (w - 1) : 0;
        static const uint32_t mask[4] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF };
        return (v * 0x010101) & mask[h ? y * 4 / h : 0];
    }
    case 7:
        return (x ^ y) & 1 ? 0xFFFFFF : 0x000000;
    case 8:
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
