#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "input/input.h"

struct ps2_decoder {
    bool e0;  // previous byte was the 0xE0 extended prefix
};

// Feeds one scancode-set-1 byte. Returns true when it completes a key press.
bool ps2_decode(struct ps2_decoder *d, uint8_t scancode, struct key_event *out);
