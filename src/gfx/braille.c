#include "gfx/braille.h"

// Unicode dot order: bits 0-2 = left column rows 0-2, bits 3-5 = right column
// rows 0-2, bit 6 = left row 3, bit 7 = right row 3.
static const uint8_t left_bit[4] = { 0x01, 0x02, 0x04, 0x40 };
static const uint8_t right_bit[4] = { 0x08, 0x10, 0x20, 0x80 };

void braille_glyph(uint8_t pattern, uint8_t out[16]) {
    for (int r = 0; r < 4; r++) {
        uint8_t bits = (uint8_t)((pattern & left_bit[r] ? 0xE0 : 0) | (pattern & right_bit[r] ? 0x0E : 0));
        out[r * 4 + 0] = bits;
        out[r * 4 + 1] = bits;
        out[r * 4 + 2] = bits;
        out[r * 4 + 3] = 0;
    }
}
