#include "info/sensors_calc.h"

bool intel_temp(uint64_t therm_status, unsigned tjmax, int *celsius) {
    if (!(therm_status & (1ull << 31)))
        return false;
    *celsius = (int)tjmax - (int)((therm_status >> 16) & 0x7F);
    return true;
}

unsigned intel_tjmax(uint64_t target, bool *assumed) {
    unsigned t = (unsigned)(target >> 16) & 0xFF;
    *assumed = t < 50 || t > 150;
    return *assumed ? 100 : t;
}

int amd_tctl_mc(uint32_t reg) {
    int mc = (int)(reg >> 21) * 125;
    return reg & (1u << 19) ? mc - 49000 : mc;
}

int amd_k10_mc(uint32_t reg) {
    return (int)(reg >> 21) * 125;
}

uint64_t effective_khz(uint64_t daperf, uint64_t dmperf, uint64_t tsc_hz) {
    uint64_t tsc_khz = tsc_hz / 1000;
    if (!dmperf || !tsc_khz || daperf / 4 > dmperf)
        return 0;
    // Same ratio with fewer bits, so tsc_khz * daperf fits in 64 bits.
    while (daperf > UINT64_MAX / tsc_khz) {
        daperf >>= 1;
        dmperf >>= 1;
    }
    return dmperf ? tsc_khz * daperf / dmperf : 0;
}

void hist_push(struct hist *h, int16_t v) {
    if (h->n < HIST_LEN) {
        h->v[(h->head + h->n++) % HIST_LEN] = v;
    } else {
        h->v[h->head] = v;
        h->head = (h->head + 1) % HIST_LEN;
    }
}

int16_t hist_get(const struct hist *h, size_t i) {
    return h->v[(h->head + i) % HIST_LEN];
}

static const uint8_t left_bit[4] = { 0x01, 0x02, 0x04, 0x40 };   // dot rows top to bottom
static const uint8_t right_bit[4] = { 0x08, 0x10, 0x20, 0x80 };

void graph_render(const struct hist *h, int lo, int hi, size_t cells, size_t rows, uint8_t *out) {
    for (size_t i = 0; i < cells * rows; i++)
        out[i] = 0;
    if (hi <= lo)
        return;
    long dots = (long)rows * 4;
    size_t slots = cells * 2;
    for (size_t s = 0; s < slots; s++) {
        if (s + h->n < slots)
            continue;  // no sample this far back
        long v = hist_get(h, s + h->n - slots);
        long level = ((v - lo) * dots + (hi - lo) / 2) / (hi - lo);
        if (level > dots)
            level = dots;
        const uint8_t *bit = s % 2 ? right_bit : left_bit;
        for (long d = 0; d < level; d++) {  // d = dot row counted from the bottom
            size_t row = rows - 1 - (size_t)(d / 4);
            out[row * cells + s / 2] |= bit[3 - d % 4];
        }
    }
}
