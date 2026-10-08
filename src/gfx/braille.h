#pragma once
#include <stdint.h>

// Builds an 8x16 glyph for a Unicode braille pattern (U+2800 + pattern):
// 2 columns x 4 rows of 3x3-pixel dots. Bits follow Unicode dot numbering.
void braille_glyph(uint8_t pattern, uint8_t out[16]);
