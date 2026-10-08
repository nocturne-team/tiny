#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "input/input.h"

// Decodes a boot-protocol mouse report: buttons, dx, dy, and the wheel when a 4th byte is present.
bool hid_mouse_report(const uint8_t *r, size_t len, struct pointer_event *out);
