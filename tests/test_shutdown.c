#include "test.h"
#include "ui/shutdown_seq.h"

static uint32_t sum(const uint32_t *g, size_t n) {
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++)
        s += g[i];
    return s;
}

int main(void) {
    // The lines, in order, with their colours.
    CHECK_STR(shutdown_lines[0].text, "WARNING: EXTREME DAMAGE SUSTAINED.");
    CHECK_EQ(shutdown_lines[0].rgb, SHUTDOWN_ORANGE);
    CHECK_STR(shutdown_lines[2].text, "ERROR: ARM CORE MODULE #1 NOT RESPONDING");
    CHECK_EQ(shutdown_lines[2].rgb, SHUTDOWN_RED);
    CHECK_STR(shutdown_lines[20].text, "! PULSE FAILURE !");
    CHECK_STR(shutdown_lines[24].text, "ERROR: NO VOCAL INTERFACE DETECTED. UNABLE TO COMPLETE TASK");
    CHECK_STR(shutdown_lines[SHUTDOWN_LINES - 1].text, "I DON'T WANT TO DIE.");
    unsigned orange = 0, red = 0;
    for (int i = 0; i < SHUTDOWN_LINES; i++) {
        CHECK(shutdown_lines[i].text != NULL);
        orange += shutdown_lines[i].rgb == SHUTDOWN_ORANGE;
        red += shutdown_lines[i].rgb == SHUTDOWN_RED;
    }
    CHECK_EQ(orange, 9);
    CHECK_EQ(red, 25);

    // Gaps: exact total, each at least 1 ms, deterministic per seed, not all equal.
    uint32_t a[SHUTDOWN_LINES - 1], b[SHUTDOWN_LINES - 1];
    size_t n = SHUTDOWN_LINES - 1;
    for (uint64_t seed = 0; seed < 50; seed++) {
        shutdown_gaps(seed * 0x9E3779B97F4A7C15ull, a, n, SHUTDOWN_MS);
        CHECK_EQ(sum(a, n), SHUTDOWN_MS);
        uint32_t lo = a[0], hi = a[0];
        for (size_t i = 0; i < n; i++) {
            lo = a[i] < lo ? a[i] : lo;
            hi = a[i] > hi ? a[i] : hi;
        }
        CHECK(lo >= 1);
        CHECK(hi > lo);
    }
    shutdown_gaps(12345, a, n, SHUTDOWN_MS);
    shutdown_gaps(12345, b, n, SHUTDOWN_MS);
    CHECK(memcmp(a, b, sizeof a) == 0);
    shutdown_gaps(54321, b, n, SHUTDOWN_MS);
    CHECK(memcmp(a, b, sizeof a) != 0);

    shutdown_gaps(7, a, 5, 5);  // as tight as allowed
    for (size_t i = 0; i < 5; i++)
        CHECK_EQ(a[i], 1);
    shutdown_gaps(7, a, 1, 300);
    CHECK_EQ(a[0], 300);
    TEST_MAIN_END();
}
