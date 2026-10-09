#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The farewell shown after R R / S S, an homage to ULTRAKILL. Pure: no hardware access.

#define SHUTDOWN_LINES 34
#define SHUTDOWN_MS 1000     // from the first line to the last
#define SHUTDOWN_TAIL_MS 150 // glitching after the last line, before the CRT switch-off

#define SHUTDOWN_ORANGE 0xFF8000
#define SHUTDOWN_RED 0xFF0000

struct shutdown_line {
    uint32_t rgb;
    const char *text;
};

extern const struct shutdown_line shutdown_lines[SHUTDOWN_LINES];

#define SHUTDOWN_IMMINENT_LINE 23  // "-!- SHUTDOWN IMMINENT -!-": glitches escalate from here
#define CRT_OFF_STEPS 12

// A filled rectangle of one colour.
struct fx_rect {
    uint32_t x, y, w, h, rgb;
};

// Frame `step` of the CRT switch-off on a w x h screen: a bright band squashing to a
// line (steps 0-5), the line shrinking to a dot (6-9), the dot fading (10-11).
void crt_off_step(int step, uint32_t w, uint32_t h, struct fx_rect *r);
// Chance out of 256 of a glitch band on a frame while `shown` lines are up.
uint32_t glitch_chance(int shown);

// Splits total_ms into n random gaps of at least 1 ms each that add up to total_ms
// exactly (needs total_ms >= n). The same seed gives the same gaps.
void shutdown_gaps(uint64_t seed, uint32_t *out, size_t n, uint32_t total_ms);
