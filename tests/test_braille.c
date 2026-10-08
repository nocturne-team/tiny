#include "test.h"
#include "gfx/braille.h"

static uint8_t g[16];

// Rows covered by dot row r are 4r..4r+2; row 4r+3 is the gap.
static void expect_rows(uint8_t pattern, const uint8_t want[16]) {
    braille_glyph(pattern, g);
    for (int i = 0; i < 16; i++)
        CHECK_EQ(g[i], want[i]);
}

int main(void) {
    static const uint8_t blank[16] = { 0 };
    expect_rows(0x00, blank);

    static const uint8_t dot1[16] = { 0xE0, 0xE0, 0xE0 };  // left column, top
    expect_rows(0x01, dot1);

    static const uint8_t dot4[16] = { 0x0E, 0x0E, 0x0E };  // right column, top
    expect_rows(0x08, dot4);

    static const uint8_t dots3_6[16] = { [8] = 0xEE, [9] = 0xEE, [10] = 0xEE };  // third row, both
    expect_rows(0x24, dots3_6);

    static const uint8_t dot7[16] = { [12] = 0xE0, [13] = 0xE0, [14] = 0xE0 };  // left, bottom
    expect_rows(0x40, dot7);

    static const uint8_t dot8[16] = { [12] = 0x0E, [13] = 0x0E, [14] = 0x0E };  // right, bottom
    expect_rows(0x80, dot8);

    static const uint8_t full[16] = {
        0xEE, 0xEE, 0xEE, 0, 0xEE, 0xEE, 0xEE, 0,
        0xEE, 0xEE, 0xEE, 0, 0xEE, 0xEE, 0xEE, 0,
    };
    expect_rows(0xFF, full);
    TEST_MAIN_END();
}
