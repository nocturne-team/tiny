#include "test.h"
#include "ui/groups.h"

// The real layout: System 0-2, Devices 3-5, Live 6-7, Tests 8-9, About 10.
static const struct ui_group g[] = {
    { "System", 0, 3 }, { "Devices", 3, 3 }, { "Live", 6, 2 }, { "Tests", 8, 2 }, { "About", 10, 1 },
};
#define NG 5
#define NP 11

static void test_group_of(void) {
    CHECK_EQ(group_of(g, NG, 0), 0);
    CHECK_EQ(group_of(g, NG, 2), 0);
    CHECK_EQ(group_of(g, NG, 3), 1);
    CHECK_EQ(group_of(g, NG, 9), 3);
    CHECK_EQ(group_of(g, NG, 10), 4);
    CHECK_EQ(group_of(g, NG, 11), -1);
    CHECK_EQ(group_of(g, 0, 0), -1);
}

static void test_group_key(void) {
    uint8_t last[NG] = { 0, 3, 6, 8, 10 };
    CHECK_EQ(group_key(g, NG, last, 0, 1), 3);    // System/CPU -> F2: Devices' last (PCI)
    last[1] = 5;
    CHECK_EQ(group_key(g, NG, last, 0, 1), 5);    // remembered Storage
    CHECK_EQ(group_key(g, NG, last, 0, 0), 1);    // F1 inside System: next page
    CHECK_EQ(group_key(g, NG, last, 1, 0), 2);
    CHECK_EQ(group_key(g, NG, last, 2, 0), 0);    // wraps within the group
    CHECK_EQ(group_key(g, NG, last, 10, 4), 10);  // one-page group stays put
    CHECK_EQ(group_key(g, NG, last, 4, 7), 4);    // no such group: unchanged
    last[2] = 9;                                  // stale: outside Live
    CHECK_EQ(group_key(g, NG, last, 0, 2), 6);    // falls back to Live's first page
}

static void test_group_remember(void) {
    uint8_t last[NG] = { 0, 3, 6, 8, 10 };
    group_remember(g, NG, last, 4, false);
    CHECK_EQ(last[1], 4);                         // Devices/USB remembered
    group_remember(g, NG, last, 9, true);         // Tests/Keys takes the keyboard:
    CHECK_EQ(last[3], 8);                         // F4 must keep opening on Screen
    group_remember(g, NG, last, 11, false);       // outside every group: ignored
    CHECK_EQ(last[4], 10);
}

static void test_group_digit(void) {
    CHECK_EQ(group_digit(g, NG, 0, 1), 0);
    CHECK_EQ(group_digit(g, NG, 0, 3), 2);
    CHECK_EQ(group_digit(g, NG, 4, 1), 3);        // in Devices: 1 = PCI
    CHECK_EQ(group_digit(g, NG, 4, 4), -1);       // only 3 pages
    CHECK_EQ(group_digit(g, NG, 0, 0), -1);
    CHECK_EQ(group_digit(g, NG, 10, 1), 10);      // About
    CHECK_EQ(group_digit(g, NG, 10, 2), -1);
    CHECK_EQ(group_digit(g, NG, 11, 1), -1);      // page outside every group
}

static void test_page_step(void) {
    CHECK_EQ(page_step(NP, 0, 1), 1);
    CHECK_EQ(page_step(NP, 0, -1), 10);           // wraps to About
    CHECK_EQ(page_step(NP, 10, 1), 0);
    CHECK_EQ(page_step(NP, 5, 0), 5);
    CHECK_EQ(page_step(0, 0, 1), 0);
}

static void test_label_at(void) {
    // " F1 System " = columns 2-12, gap 13, " F2 Devices " = 14-25, gap 26, " F3 Live " = 27-35.
    const char *labels[] = { " F1 System ", " F2 Devices ", " F3 Live " };
    CHECK_EQ(label_at(labels, 3, 1), -1);
    CHECK_EQ(label_at(labels, 3, 2), 0);
    CHECK_EQ(label_at(labels, 3, 12), 0);
    CHECK_EQ(label_at(labels, 3, 13), -1);
    CHECK_EQ(label_at(labels, 3, 14), 1);
    CHECK_EQ(label_at(labels, 3, 25), 1);
    CHECK_EQ(label_at(labels, 3, 26), -1);
    CHECK_EQ(label_at(labels, 3, 27), 2);
    CHECK_EQ(label_at(labels, 3, 35), 2);
    CHECK_EQ(label_at(labels, 3, 36), -1);
    CHECK_EQ(label_at(labels, 0, 2), -1);
}

int main(void) {
    test_group_of();
    test_group_key();
    test_group_remember();
    test_group_digit();
    test_page_step();
    test_label_at();
    TEST_MAIN_END();
}
