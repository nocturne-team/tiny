#include "test.h"
#include "ui/pointer.h"

int main(void) {
    struct pointer p;
    pointer_init(&p, 80, 25);
    CHECK_EQ(p.x, 40);
    CHECK_EQ(p.y, 12);
    CHECK(!p.visible);
    // The first motion shows the pointer even without a cell change.
    CHECK(pointer_move(&p, 1, 0, 80, 25));
    CHECK(p.visible);
    CHECK_EQ(p.x, 40);
    // Sub-cell motion accumulates: 1 + 4 + 1 = 6 counts = one column.
    CHECK(!pointer_move(&p, 4, 0, 80, 25));
    CHECK_EQ(p.x, 40);
    CHECK(pointer_move(&p, 1, 0, 80, 25));
    CHECK_EQ(p.x, 41);
    // Rows take 12 counts; negative motion works the same way.
    pointer_move(&p, 0, -24, 80, 25);
    CHECK_EQ(p.y, 10);
    pointer_move(&p, -12, 0, 80, 25);
    CHECK_EQ(p.x, 39);
    // Clamped at the edges, and the leftover is dropped there.
    pointer_move(&p, -10000, -10000, 80, 25);
    CHECK_EQ(p.x, 0);
    CHECK_EQ(p.y, 0);
    pointer_move(&p, 5, 11, 80, 25);
    CHECK_EQ(p.x, 0);
    CHECK_EQ(p.y, 0);
    pointer_move(&p, 1, 1, 80, 25);
    CHECK_EQ(p.x, 1);
    CHECK_EQ(p.y, 1);
    pointer_move(&p, 100000, 100000, 80, 25);
    CHECK_EQ(p.x, 79);
    CHECK_EQ(p.y, 24);

    CHECK_EQ(pointer_wheel_lines(1), -3);
    CHECK_EQ(pointer_wheel_lines(-2), 6);
    CHECK_EQ(pointer_wheel_lines(0), 0);

    TEST_MAIN_END();
}
