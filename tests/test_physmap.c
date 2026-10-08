#include "test.h"
#include "info/physmap.h"

int main(void) {
    static const struct phys_range r[] = {
        { 0x1000, 0x1000 },   // 0x1000-0x1fff
        { 0x2000, 0x2000 },   // 0x2000-0x3fff (adjacent: tables may span both)
        { 0x8000, 0x1000 },   // 0x8000-0x8fff
    };
    struct phys_ranges m = { r, 3 };

    CHECK_EQ(phys_mapped_len(&m, 0x1000, 0x10), 0x10);
    CHECK_EQ(phys_mapped_len(&m, 0x1ff0, 0x100), 0x100);   // crosses into the adjacent range
    CHECK_EQ(phys_mapped_len(&m, 0x3f00, 0x1000), 0x100);  // runs off the end
    CHECK_EQ(phys_mapped_len(&m, 0x4000, 0x10), 0);        // gap
    CHECK_EQ(phys_mapped_len(&m, 0x0, 0x10), 0);
    CHECK_EQ(phys_mapped_len(&m, 0x8800, 0x800), 0x800);
    CHECK_EQ(phys_mapped_len(&m, 0x8800, 0x801), 0x800);
    CHECK(phys_ok(&m, 0x1000, 0x3000));
    CHECK(!phys_ok(&m, 0x1000, 0x3001));
    CHECK(!phys_ok(&m, 0xffffffffffffff00ULL, 0x200));     // wraps around: never mapped

    CHECK_EQ(phys_mapped_len(NULL, 0x4000, 0x10), 0x10);   // NULL = trust everything
    CHECK(phys_ok(NULL, 0, 1));
    TEST_MAIN_END();
}
