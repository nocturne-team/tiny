#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "input/input.h"

// PS/2 keyboard backend. ps2_init returns false when no 8042 controller responds.
bool ps2_init(void);
// Non-blocking: true and fills *out when a key was pressed.
bool ps2_poll(struct key_event *out);

// Raw key events (key tester), filled while ps2_poll reads bytes.
bool ps2_poll_raw(struct raw_key *out);
uint32_t ps2_raw_dropped(void);
// Sets the keyboard LEDs (PS/2 bits: 0 Scroll, 1 Num, 2 Caps). Logs once if unacknowledged.
void ps2_set_leds(uint8_t bits);
