#include "test.h"
#include "ui/kbd_layout.h"

static struct kbd_geom g;

static int find(uint8_t usage) {
    for (int i = 0; i < KBD_KEYS; i++)
        if (kbd_keys[i].usage == usage)
            return i;
    return -1;
}

static bool overlap(const struct kbd_rect *a, const struct kbd_rect *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static void check_fit(int cols, int rows, int unit, int key_h) {
    CHECK(kbd_place(cols, rows, &g));
    CHECK_EQ(g.unit, unit);
    CHECK_EQ(g.key_h, key_h);
    CHECK(g.width <= cols);
    CHECK(g.height <= rows);
    for (int i = 0; i < KBD_KEYS; i++) {
        const struct kbd_rect *r = &g.r[i];
        CHECK(r->x >= 0 && r->y >= 0 && r->x + r->w <= g.width && r->y + r->h <= g.height);
        CHECK(r->w >= 3);
        for (int j = i + 1; j < KBD_KEYS; j++)
            if (overlap(r, &g.r[j])) {
                printf("overlap at %d x %d: %s / %s\n", cols, rows, kbd_keys[i].label, kbd_keys[j].label);
                CHECK(false);
            }
    }
}

int main(void) {
    // 640x480, 1024x768, 1280x720 content areas.
    check_fit(76, 24, 3, 1);
    check_fit(124, 42, 5, 3);
    check_fit(156, 39, 6, 3);
    check_fit(124, 24, 5, 1);  // short screen: one-row keys
    CHECK_EQ(g.height, 11);
    CHECK(kbd_place(156, 39, &g));
    CHECK_EQ(g.height, 19);
    CHECK(!kbd_place(67, 39, &g));  // narrower than 3 columns per unit
    CHECK(kbd_place(68, 39, &g));
    CHECK(!kbd_place(76, 10, &g));  // shorter than the board

    // Table: unique non-zero usages, a label and a name for each.
    for (int i = 0; i < KBD_KEYS; i++) {
        CHECK(kbd_keys[i].usage);
        CHECK(kbd_keys[i].label && kbd_keys[i].label[0]);
        CHECK(kbd_usage_name(kbd_keys[i].usage) != NULL);
        for (int j = i + 1; j < KBD_KEYS; j++)
            CHECK(kbd_keys[i].usage != kbd_keys[j].usage);
        CHECK(kbd_keys[i].x + kbd_keys[i].w <= KBD_WIDTH_Q);
    }
    // ANSI + ISO specifics.
    CHECK(find(0x64) >= 0);              // ISO <>
    CHECK_EQ(kbd_keys[find(0x57)].h, 2); // keypad + is two rows tall
    CHECK_EQ(kbd_keys[find(0x58)].h, 2); // keypad Enter too
    CHECK_EQ(kbd_keys[find(0x2C)].w, 25);// Space, 6.25 units
    CHECK(find(0x32) < 0);               // ISO #~ is not drawn, only named

    CHECK_STR(kbd_usage_name(0x04), "A");
    CHECK_STR(kbd_usage_name(0x27), "0");
    CHECK_STR(kbd_usage_name(0xE1), "Left Shift");
    CHECK_STR(kbd_usage_name(0x32), "ISO #~");
    CHECK_STR(kbd_usage_name(0x87), "Ro");
    CHECK_STR(kbd_usage_name(0x7F), "Mute");
    CHECK_STR(kbd_usage_name(0x52), "Up");
    CHECK(kbd_usage_name(0x00) == NULL);
    CHECK(kbd_usage_name(0xA5) == NULL);
    TEST_MAIN_END();
}
