#include "info/physmap.h"

uint64_t phys_mapped_len(const struct phys_ranges *m, uint64_t phys, uint64_t want) {
    if (!m)
        return want;
    if (phys + want < phys)
        return 0;  // wraps around the address space
    uint64_t cur = phys, end = phys + want;
    while (cur < end) {
        bool found = false;
        for (size_t i = 0; i < m->n; i++) {
            const struct phys_range *r = &m->r[i];
            if (cur >= r->base && cur - r->base < r->len) {
                cur = r->base + r->len;
                found = true;
                break;
            }
        }
        if (!found)
            break;
    }
    return (cur < end ? cur : end) - phys;
}

bool phys_ok(const struct phys_ranges *m, uint64_t phys, uint64_t len) {
    return phys_mapped_len(m, phys, len) == len;
}
