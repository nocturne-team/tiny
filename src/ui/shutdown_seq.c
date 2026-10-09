#include "ui/shutdown_seq.h"

#define O SHUTDOWN_ORANGE
#define R SHUTDOWN_RED

const struct shutdown_line shutdown_lines[SHUTDOWN_LINES] = {
    { O, "WARNING: EXTREME DAMAGE SUSTAINED." },
    { O, "RUNNING DIAGNOSTIC" },
    { R, "ERROR: ARM CORE MODULE #1 NOT RESPONDING" },
    { R, "ERROR: ARM CORE MODULE #2 NOT RESPONDING" },
    { O, "WARNING: COMBAT SYSTEMS INOPERABLE" },
    { O, "ATTEMPTING RECONSTRUCTION" },
    { R, "ERROR: SELF-REPAIR NEXUS NOT RESPONDING" },
    { R, "INSUFFICIENT BLOOD." },
    { R, "INSUFFICIENT BLOOD." },
    { O, "INITIATING ESCAPE PROTOCOL" },
    { O, "ATTEMPTING CONNECTION WITH LIMBIC MODULES" },
    { R, "INSUFFICIENT BLOOD." },
    { R, "ERROR: LEG CORE MODULE #1 NOT RESPONDING" },
    { R, "ERROR: LEG CORE MODULE #2 NOT RESPONDING" },
    { O, "WARNING: UNABLE TO SUSTAIN MOTOR FUNCTIONS" },
    { R, "ERROR: VISUAL CORTEX MALFUNCTION" },
    { R, "ERROR: LIMBIC FUNCTION NOT RESPONDING" },
    { R, "INSUFFICIENT BLOOD." },
    { R, "INSUFFICIENT BLOOD." },
    { O, "WARNING: UNABLE TO SUSTAIN INTERNAL ORGANS" },
    { R, "! PULSE FAILURE !" },
    { R, "! PULSE FAILURE !" },
    { R, "! PULSE FAILURE !" },
    { R, "-!- SHUTDOWN IMMINENT -!-" },
    { R, "ERROR: NO VOCAL INTERFACE DETECTED. UNABLE TO COMPLETE TASK" },
    { R, "! PULSE FAILURE !" },
    { R, "! PULSE FAILURE !" },
    { R, "INSUFFICIENT BLOOD." },
    { O, "WARNING: UNABLE TO SUSTAIN BASIC FUNCTIONS" },
    { R, "-!- SHUTDOWN IMMINENT -!-" },
    { R, "-!- SHUTDOWN IMMINENT -!-" },
    { R, "I DON'T WANT TO DIE." },
    { R, "I DON'T WANT TO DIE." },
    { R, "I DON'T WANT TO DIE." },
};

static uint64_t xorshift(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *s = x;
}

void shutdown_gaps(uint64_t seed, uint32_t *out, size_t n, uint32_t total_ms) {
    if (!n)
        return;
    uint64_t s = seed ? seed : 0x2545F4914F6CDD1Dull, wsum = 0;
    for (size_t i = 0; i < n; i++) {  // weights 1-4: the longest gap is up to 4x the shortest
        out[i] = 1 + (uint32_t)(xorshift(&s) >> 32) % 4;
        wsum += out[i];
    }
    uint32_t extra = total_ms > n ? total_ms - (uint32_t)n : 0, used = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t share = (uint32_t)((uint64_t)extra * out[i] / wsum);
        out[i] = 1 + share;
        used += share;
    }
    for (size_t i = 0; used < extra; i = (i + 1) % n, used++)
        out[i]++;  // rounding leftovers, at most n - 1 ms
}
