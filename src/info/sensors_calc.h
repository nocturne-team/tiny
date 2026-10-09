#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Intel IA32_(PACKAGE_)THERM_STATUS: false when the reading-valid bit is clear.
bool intel_temp(uint64_t therm_status, unsigned tjmax, int *celsius);
// MSR_TEMPERATURE_TARGET bits 23:16; 100 with *assumed set when out of 50..150.
unsigned intel_tjmax(uint64_t target, bool *assumed);
// AMD family 17h+ THM_TCON_CUR_TMP, in millidegrees C (range select subtracts 49 C).
int amd_tctl_mc(uint32_t reg);
// AMD family 10h-16h D18F3xA4, in millidegrees C.
int amd_k10_mc(uint32_t reg);
// TSC frequency scaled by the APERF/MPERF ratio, in kHz; 0 when dmperf is 0 or the
// ratio is above 4 (no CPU turbos that far over its TSC rate: a counter glitch).
uint64_t effective_khz(uint64_t daperf, uint64_t dmperf, uint64_t tsc_hz);

#define HIST_LEN 120

// The last HIST_LEN samples. Zero-initialised is empty.
struct hist {
    int16_t v[HIST_LEN];
    size_t n, head;  // samples held; index of the oldest
};

void hist_push(struct hist *h, int16_t v);
// Sample i, 0 = oldest held.
int16_t hist_get(const struct hist *h, size_t i);

// Braille area graph of the newest 2*cells samples, newest at the right, scaled so
// `lo` is empty and `hi` fills all rows*4 dot rows. `out` gets rows*cells patterns,
// top row first. Slots without a sample stay empty.
void graph_render(const struct hist *h, int lo, int hi, size_t cells, size_t rows, uint8_t *out);
