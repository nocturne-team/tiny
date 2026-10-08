#pragma once
#include <stdbool.h>
#include "input/input.h"

// PS/2 keyboard backend. ps2_init returns false when no 8042 controller responds.
bool ps2_init(void);
// Non-blocking: true and fills *out when a key was pressed.
bool ps2_poll(struct key_event *out);
