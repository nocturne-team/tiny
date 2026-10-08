#pragma once
#include <stdint.h>

// IBM VGA 8x16, code page 437: one byte per pixel row, MSB is the leftmost pixel.
extern const uint8_t font8x16[256][16];
