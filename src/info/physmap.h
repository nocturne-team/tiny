#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Physical ranges known to be mapped in the HHDM (built from the memory map).
struct phys_range {
    uint64_t base, len;
};

struct phys_ranges {
    const struct phys_range *r;
    size_t n;
};

// Bytes readable from `phys` (at most `want`) through contiguous mapped ranges.
// A NULL `m` means "no map available": everything is trusted.
uint64_t phys_mapped_len(const struct phys_ranges *m, uint64_t phys, uint64_t want);
bool phys_ok(const struct phys_ranges *m, uint64_t phys, uint64_t len);
