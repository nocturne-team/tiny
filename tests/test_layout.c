#include "test.h"
#include "gfx/gfx.h"

static void expect(uint64_t w, uint64_t h, int cols, int rows, int scale, int x0, int y0) {
    struct gfx_layout l;
    CHECK(gfx_compute_layout(w, h, &l));
    CHECK_EQ(l.cols, cols);
    CHECK_EQ(l.rows, rows);
    CHECK_EQ(l.scale, scale);
    CHECK_EQ(l.x0, x0);
    CHECK_EQ(l.y0, y0);
}

int main(void) {
    expect(1024, 768, 128, 48, 1, 0, 0);
    expect(1920, 1080, 240, 67, 1, 0, 4);
    expect(2560, 1440, 160, 45, 2, 0, 0);
    expect(3840, 2160, 160, 45, 3, 0, 0);
    expect(5120, 1080, 240, 67, 1, 1600, 4);  // ultra-wide: clamped and centred
    expect(640, 480, 80, 30, 1, 0, 0);
    expect(320, 200, 40, 12, 1, 0, 4);        // smallest accepted grid

    struct gfx_layout l;
    CHECK(!gfx_compute_layout(300, 200, &l)); // 37 columns: too narrow
    CHECK(!gfx_compute_layout(640, 160, &l)); // 10 rows: too short
    TEST_MAIN_END();
}
