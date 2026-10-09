#include "test.h"
#include "info/sensors_calc.h"

static void test_intel(void) {
    int c = -1;
    CHECK(!intel_temp(0x00280000, 100, &c));               // readout 40, valid bit clear
    CHECK(intel_temp(0x80280000ull, 100, &c));
    CHECK_EQ(c, 60);
    CHECK(intel_temp(0x88000000ull | (5u << 16), 105, &c));  // other status bits ignored
    CHECK_EQ(c, 100);

    bool assumed = false;
    CHECK_EQ(intel_tjmax(0x00640000, &assumed), 100);
    CHECK(!assumed);
    CHECK_EQ(intel_tjmax(0x0F5A0000, &assumed), 90);  // offset bits above 23 ignored
    CHECK(!assumed);
    CHECK_EQ(intel_tjmax(0, &assumed), 100);
    CHECK(assumed);
    CHECK_EQ(intel_tjmax(0x00FF0000, &assumed), 100);
    CHECK(assumed);
}

static void test_amd(void) {
    CHECK_EQ(amd_tctl_mc(400u << 21), 50000);                 // 400 * 0.125
    CHECK_EQ(amd_tctl_mc((400u << 21) | (1u << 19)), 1000);   // range select: -49 C
    CHECK_EQ(amd_tctl_mc((401u << 21) | 0x1FFFF), 50125);     // low bits ignored
    CHECK_EQ(amd_k10_mc(360u << 21), 45000);
}

static void test_clock(void) {
    CHECK_EQ(effective_khz(100, 0, 3000000000ull), 0);
    CHECK_EQ(effective_khz(1000, 1000, 3600000000ull), 3600000);
    CHECK_EQ(effective_khz(1200, 1000, 3600000000ull), 4320000);
    // A second's worth of counts at 5 GHz: no overflow.
    CHECK_EQ(effective_khz(5000000000ull, 3000000000ull, 3000000000ull), 5000000);
    CHECK_EQ(effective_khz(UINT64_MAX / 2, UINT64_MAX / 4, 1000000000ull), 2000000);
    // Implausible ratios (counter reset, backwards step seen as a huge delta): no reading.
    CHECK_EQ(effective_khz(5000, 1000, 3000000000ull), 0);
    CHECK_EQ(effective_khz(UINT64_MAX - 5, 1000, 3000000000ull), 0);
    CHECK_EQ(effective_khz(4000, 1000, 3000000000ull), 12000000);  // 4x turbo: still believed
}

static void test_hist(void) {
    struct hist h;
    memset(&h, 0, sizeof h);
    hist_push(&h, 5);
    CHECK_EQ(h.n, 1);
    CHECK_EQ(hist_get(&h, 0), 5);
    for (int i = 0; i < HIST_LEN + 3; i++)
        hist_push(&h, (int16_t)i);
    CHECK_EQ(h.n, HIST_LEN);
    CHECK_EQ(hist_get(&h, 0), 3);
    CHECK_EQ(hist_get(&h, HIST_LEN - 1), HIST_LEN + 2);
}

static void test_graph(void) {
    struct hist h;
    uint8_t out[2 * 3];
    memset(&h, 0, sizeof h);
    graph_render(&h, 0, 80, 3, 2, out);
    for (int i = 0; i < 6; i++)
        CHECK_EQ(out[i], 0);

    // One full-scale sample: rightmost cell, right column, all 8 dot rows.
    hist_push(&h, 80);
    graph_render(&h, 0, 80, 3, 2, out);
    CHECK_EQ(out[2], 0x08 | 0x10 | 0x20 | 0x80);  // top row, cell 2
    CHECK_EQ(out[5], 0x08 | 0x10 | 0x20 | 0x80);  // bottom row, cell 2
    CHECK_EQ(out[0] | out[1] | out[3] | out[4], 0);

    // Half scale (4 of 8 dots) then above-range (clamped to full).
    hist_push(&h, 40);
    hist_push(&h, 200);
    graph_render(&h, 0, 80, 3, 2, out);
    // slots: ... [80][40][200] -> cell 1 right = 80, cell 2 left = 40, cell 2 right = 200
    CHECK_EQ(out[1], 0x08 | 0x10 | 0x20 | 0x80);
    CHECK_EQ(out[4], 0x08 | 0x10 | 0x20 | 0x80);
    CHECK_EQ(out[2], 0x08 | 0x10 | 0x20 | 0x80);  // top: only the clamped sample
    CHECK_EQ(out[5], 0xFF);                         // bottom: both columns full
    // Below range: nothing. Slots now ... [80][40] | [200][-10].
    hist_push(&h, -10);
    graph_render(&h, 0, 80, 3, 2, out);
    CHECK_EQ(out[2], 0x01 | 0x02 | 0x04 | 0x40);   // left = 200 (full), right empty
    CHECK_EQ(out[5], 0x01 | 0x02 | 0x04 | 0x40);
    CHECK_EQ(out[1], 0x01 | 0x02 | 0x04 | 0x40);   // cell 1: left 80 full, right 40 half
    CHECK_EQ(out[4], 0xFF);

    // One dot: 10 of 80 over 8 dot rows = 1 dot, the bottom one of the left column.
    memset(&h, 0, sizeof h);
    hist_push(&h, 10);
    hist_push(&h, 0);
    graph_render(&h, 0, 80, 3, 2, out);
    CHECK_EQ(out[5], 0x40);
    CHECK_EQ(out[2], 0);

    graph_render(&h, 50, 50, 3, 2, out);  // empty range: no division by zero
    CHECK_EQ(out[5], 0);
}

int main(void) {
    test_intel();
    test_amd();
    test_clock();
    test_hist();
    test_graph();
    TEST_MAIN_END();
}
