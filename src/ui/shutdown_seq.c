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

static uint32_t clamp(uint32_t v, uint32_t lo, uint32_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

void crt_off_step(int step, uint32_t w, uint32_t h, struct fx_rect *r) {
    uint32_t thick = h / 270 > 2 ? h / 270 : 2;  // the collapsed line and the dot
    uint32_t rw = w, rh = thick, rgb = 0xFFFFFF;
    if (step < 6) {  // squash: h/2 down to h/72, getting brighter
        uint32_t k = (uint32_t)(6 - step);
        rh = h * k * k / 72;
        uint32_t v = 0x90 + (0xFF - 0x90) * (uint32_t)step / 5;
        rgb = v * 0x010101;
    } else if (step < 10) {  // the line shrinks to the middle
        uint32_t k = (uint32_t)(10 - step);
        rw = w * k * k / 25;
    } else {  // the dot fades
        rw = thick;
        rgb = step == 10 ? 0x909090 : 0x404040;
    }
    if (rh < thick)
        rh = thick;
    r->w = clamp(rw, 1, w ? w : 1);
    r->h = clamp(rh, 1, h ? h : 1);
    r->x = (w - r->w) / 2;
    r->y = (h - r->h) / 2;
    r->rgb = rgb;
}

uint32_t glitch_chance(int shown) {
    if (shown < SHUTDOWN_IMMINENT_LINE)
        return 16;
    uint32_t c = 16 + (uint32_t)(shown - SHUTDOWN_IMMINENT_LINE + 1) * 16;
    return c > 192 ? 192 : c;
}
