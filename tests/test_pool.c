#include "test.h"
#include "mm/pool.h"

int main(void) {
    struct phys_pool p;

    pool_init(&p, 1 << 20);
    pool_add(&p, 0, 0x9F000);               // below 1 MiB: dropped
    pool_add(&p, 0x100800, 0x3000);         // unaligned: 0x101000..0x103000
    pool_add(&p, 0x200000, 0x800);          // less than a page after alignment: dropped
    pool_add(&p, 0xFFFFE000, 0x10000);      // crosses 4 GiB: clipped to two pages
    pool_add(&p, 0x100000000ull, 0x100000); // above 4 GiB: dropped
    pool_add(&p, 0xFFFFFFFFFFFFF000ull, 0x2000);  // wraps: dropped, no crash
    CHECK_EQ(pool_avail(&p), 4 * 4096);
    CHECK_EQ(pool_alloc(&p), 0x101000);
    CHECK_EQ(pool_alloc(&p), 0x102000);
    CHECK_EQ(pool_alloc(&p), 0xFFFFE000);
    CHECK_EQ(pool_alloc(&p), 0xFFFFF000);
    CHECK_EQ(pool_alloc(&p), 0);
    CHECK_EQ(pool_alloc(&p), 0);

    // Unsorted ranges: a lower range after a higher one is still used.
    pool_init(&p, 1 << 20);
    pool_add(&p, 0x400000, 0x1000);
    pool_add(&p, 0x300000, 0x1000);
    CHECK_EQ(pool_alloc(&p), 0x400000);
    CHECK_EQ(pool_alloc(&p), 0x300000);
    CHECK_EQ(pool_alloc(&p), 0);

    // The cap limits the total handed out.
    pool_init(&p, 8192);
    pool_add(&p, 0x200000, 0x100000);
    CHECK(pool_alloc(&p) != 0);
    CHECK(pool_alloc(&p) != 0);
    CHECK_EQ(pool_alloc(&p), 0);

    // More ranges than fit: extra ones are ignored, nothing overflows.
    pool_init(&p, 1 << 30);
    for (int i = 0; i < POOL_MAX_RANGES + 10; i++)
        pool_add(&p, 0x1000000 + (uint64_t)i * 0x10000, 0x1000);
    CHECK_EQ(pool_avail(&p), (uint64_t)POOL_MAX_RANGES * 4096);

    TEST_MAIN_END();
}
