#pragma once
#include <stddef.h>
#include <stdint.h>

#define POOL_MAX_RANGES 64
#define POOL_PAGE 4096u
#define POOL_LOW (1ull << 20)   // never hand out the first MiB
#define POOL_HIGH (1ull << 32)  // DMA must work for 32-bit-only controllers

struct pool_range {
    uint64_t base, end;  // page aligned, end exclusive
};

// Pure 4 KiB page bump allocator over physical ranges. Nothing is freed.
struct phys_pool {
    struct pool_range r[POOL_MAX_RANGES];
    size_t n, cur;
    uint64_t next;       // next candidate page in r[cur], 0 = start of r[cur]
    uint64_t used, cap;  // bytes
};

void pool_init(struct phys_pool *p, uint64_t cap);
// Adds [base, base+len), clipped to [POOL_LOW, POOL_HIGH) and shrunk to whole pages.
void pool_add(struct phys_pool *p, uint64_t base, uint64_t len);
// Physical address of a fresh page, or 0 when exhausted or over the cap.
uint64_t pool_alloc(struct phys_pool *p);
// Total bytes in all ranges (ignoring the cap).
uint64_t pool_avail(const struct phys_pool *p);
