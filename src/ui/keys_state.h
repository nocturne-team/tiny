#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "input/input.h"

#define KEYS_LINES 10      // recent events kept
#define KEYS_HOLD_MS 1000  // hold Esc this long to leave the tester

struct keys_line {
    struct raw_key ev;
    uint32_t count;        // > 1: PS/2 typematic repeats folded into this line
};

struct keys_state {
    uint8_t held[32], tested[32];  // usage bitmaps
    unsigned held_n, max_held, tested_n;
    struct keys_line lines[KEYS_LINES];  // [0] is the newest
    size_t nlines;
    bool esc_down;
    uint64_t esc_since;
    uint8_t leds;          // HID bits: 0 Num, 1 Caps, 2 Scroll
    bool skip_enter;       // the Enter that started this run is still down
};

// Applies one event at `now` (ms). True when a lock key flipped s->leds (send it).
bool keys_apply(struct keys_state *s, const struct raw_key *ev, uint64_t now);
// True once Esc has been held for KEYS_HOLD_MS.
bool keys_esc_held(const struct keys_state *s, uint64_t now);
// Starts a run: forgets held keys (as keys_reset_held) and ignores the Enter (or
// keypad Enter) that started it, up to and including its release.
void keys_start(struct keys_state *s);
// Forgets the held keys and Esc: after leaving, their releases go to another tab.
void keys_reset_held(struct keys_state *s);
bool keys_is_held(const struct keys_state *s, uint8_t usage);
bool keys_is_tested(const struct keys_state *s, uint8_t usage);
// Keys on the picture that were never pressed.
unsigned keys_untested(const struct keys_state *s);
// "USB 2.1                  press    0x04  A" and friends.
void keys_line_text(const struct keys_line *l, char *buf, size_t cap);
