#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "input/input.h"

#define HID_REPEAT_DELAY_MS 500
#define HID_REPEAT_RATE_MS 33

// State of one boot-protocol keyboard (8-byte reports: modifiers, reserved, 6 usages).
struct hid_kbd {
    uint8_t prev[8];          // last accepted report
    uint8_t held;             // usage currently auto-repeating, 0 if none
    uint64_t next_repeat_ms;
    uint8_t raw_prev[8];      // last report seen by hid_kbd_diff
    bool raw_rollover;        // the last report was ErrorRollOver
};

// Feeds one report. Writes a key_event for every newly pressed mapped key to
// out[] (room for 6) and returns how many.
size_t hid_kbd_report(struct hid_kbd *k, const uint8_t *r, size_t len, uint64_t now_ms,
                      struct key_event *out);
// Typematic: true and fills *out (repeat = true) when the held key is due.
bool hid_kbd_tick(struct hid_kbd *k, uint64_t now_ms, struct key_event *out);
// Stops any auto-repeat: after a transfer error the release report may be lost.
void hid_kbd_cancel_repeat(struct hid_kbd *k);

#define HID_RAW_MAX 20  // 8 modifiers + 6 presses + 6 releases

// Key tester: compares a report with the previous one and writes a raw_key per changed
// modifier bit and per usage that appeared (presses first) or disappeared. A run of
// ErrorRollOver reports gives one RAW_ROLLOVER event and keeps the previous state.
// Keeps its own state, independent of hid_kbd_report. Returns the event count.
size_t hid_kbd_diff(struct hid_kbd *k, const uint8_t *r, size_t len, struct raw_key *out);
