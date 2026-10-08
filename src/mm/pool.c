#include "mm/pool.h"
#include "lib/string.h"

void pool_init(struct phys_pool *p, uint64_t cap) {
    memset(p, 0, sizeof *p);
    p->cap = cap;
}

void pool_add(struct phys_pool *p, uint64_t base, uint64_t len) {
    if (len > UINT64_MAX - base)
        return;  // wraps around the address space
    uint64_t end = base + len;
    if (base < POOL_LOW)
        base = POOL_LOW;
    if (end > POOL_HIGH)
        end = POOL_HIGH;
    base = (base + POOL_PAGE - 1) & ~(uint64_t)(POOL_PAGE - 1);
    end &= ~(uint64_t)(POOL_PAGE - 1);
    if (base >= end || p->n == POOL_MAX_RANGES)
        return;
    p->r[p->n].base = base;
    p->r[p->n].end = end;
    p->n++;
}

uint64_t pool_alloc(struct phys_pool *p) {
    if (p->used + POOL_PAGE > p->cap)
        return 0;
    while (p->cur < p->n) {
        const struct pool_range *r = &p->r[p->cur];
        if (p->next < r->base)
            p->next = r->base;
        if (p->next + POOL_PAGE <= r->end) {
            uint64_t page = p->next;
            p->next += POOL_PAGE;
            p->used += POOL_PAGE;
            return page;
        }
        p->cur++;
        p->next = 0;
    }
    return 0;
}

uint64_t pool_avail(const struct phys_pool *p) {
    uint64_t sum = 0;
    for (size_t i = 0; i < p->n; i++)
        sum += p->r[i].end - p->r[i].base;
    return sum;
}
