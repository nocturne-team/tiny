#pragma once
#include <stdbool.h>
#include <stdint.h>

struct pci_device;

static inline uint32_t mmio_rd(volatile uint8_t *base, uint32_t off) {
    return *(volatile uint32_t *)(base + off);
}

static inline void mmio_wr(volatile uint8_t *base, uint32_t off, uint32_t v) {
    *(volatile uint32_t *)(base + off) = v;
}

// Low dword first: fine for registers that only take effect when enabled.
static inline void mmio_wr64(volatile uint8_t *base, uint32_t off, uint64_t v) {
    mmio_wr(base, off, (uint32_t)v);
    mmio_wr(base, off + 4, (uint32_t)(v >> 32));
}

// Polls every 10 µs until (reg & mask) == want, for up to `ms` milliseconds.
bool mmio_wait(volatile uint8_t *base, uint32_t off, uint32_t mask, uint32_t want, unsigned ms);

// Sizes memory BAR `bar` (0-5), enables memory decoding and bus mastering, and maps
// up to `max` bytes of it uncached. NULL for an unassigned, I/O or tiny BAR.
volatile uint8_t *pci_map_bar(const struct pci_device *pd, unsigned bar, uint64_t max, uint64_t *len);
