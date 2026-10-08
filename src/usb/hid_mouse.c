#include "usb/hid_mouse.h"

bool hid_mouse_report(const uint8_t *r, size_t len, struct pointer_event *out) {
    if (len < 3)
        return false;
    out->buttons = r[0] & 0x07;
    out->dx = (int8_t)r[1];
    out->dy = (int8_t)r[2];
    out->wheel = len >= 4 ? (int8_t)r[3] : 0;
    return true;
}
