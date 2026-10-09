#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The farewell shown after R R / S S, an homage to ULTRAKILL. Pure: no hardware access.

#define SHUTDOWN_LINES 34
#define SHUTDOWN_MS 1000     // from the first line to the last
#define SHUTDOWN_HOLD_MS 250 // the last line stays up this long

#define SHUTDOWN_ORANGE 0xFF8000
#define SHUTDOWN_RED 0xFF0000

struct shutdown_line {
    uint32_t rgb;
    const char *text;
};

extern const struct shutdown_line shutdown_lines[SHUTDOWN_LINES];

// Splits total_ms into n random gaps of at least 1 ms each that add up to total_ms
// exactly (needs total_ms >= n). The same seed gives the same gaps.
void shutdown_gaps(uint64_t seed, uint32_t *out, size_t n, uint32_t total_ms);
