#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "input/input.h"

struct ps2_decoder {
    bool e0;            // previous byte was the 0xE0 extended prefix
    uint8_t down[2][16]; // held keys: [extended][make code / 8] bitmap
};

// Feeds one scancode-set-1 byte. Returns true when it completes a key press.
bool ps2_decode(struct ps2_decoder *d, uint8_t scancode, struct key_event *out);

// Raw path for the key tester: every make and break as a HID usage, with its bytes.
// Fed the same bytes as ps2_decode; the two keep separate state.
struct ps2_raw {
    uint8_t seq[6], len;      // bytes of the sequence in progress
    uint8_t down[2][16];      // held keys: [extended][code / 8]
};

// Feeds one byte. Writes 0-2 events to out[] (Pause gives its press and release at
// once, because the key sends no break code) and returns how many.
size_t ps2_raw_feed(struct ps2_raw *r, uint8_t sc, struct raw_key out[2]);
// Set 1 make code (bit 7 clear) to its HID usage; 0 when it has none.
uint8_t ps2_set1_usage(uint8_t code, bool ext);
// HID LED bits (0 Num, 1 Caps, 2 Scroll) to the PS/2 ED argument (0 Scroll, 1 Num, 2 Caps).
uint8_t ps2_led_bits(uint8_t hid);
