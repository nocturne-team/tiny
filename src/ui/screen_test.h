#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "input/input.h"

// Full-screen test patterns for the Screen tab. Pure: no hardware access.

#define SCREEN_PATTERNS 9

const char *screen_pattern_name(int pattern);
// True when every pixel of the pattern has the same colour (draw it as one fill).
bool screen_pattern_solid(int pattern);
// 0xRRGGBB of pixel (x, y) of `pattern` on a w x h screen.
uint32_t screen_pixel(int pattern, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

enum screen_action { SCREEN_NONE, SCREEN_NEXT, SCREEN_PREV, SCREEN_LEAVE };
// What a raw key event does while the patterns are shown. Only presses count.
// Enter does nothing: the press that started the test arrives here too.
enum screen_action screen_key(const struct raw_key *ev);
// The pattern after applying `a` to `cur`; -1 means leave (Esc, or Next on the last).
int screen_step(int cur, enum screen_action a);

enum screen_native { SCREEN_NATIVE_UNKNOWN, SCREEN_NATIVE_YES, SCREEN_NATIVE_NO };
// Compares the framebuffer mode with the monitor's preferred mode (0 x 0 = no EDID).
enum screen_native screen_native(uint64_t fb_w, uint64_t fb_h, uint16_t pref_w, uint16_t pref_h);
