#include "input/input.h"
#include "input/ps2.h"
#include "input/ps2_decode.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "usb/usb.h"

static bool ps2, last_have;

void input_init(void) {
    ps2 = ps2_init();
    last_have = input_have_keyboard();
}

bool input_poll(struct key_event *out) {
    return ps2_poll(out) || usb_poll(out);
}

bool input_have_keyboard(void) {
    return ps2 || usb_keyboards() > 0;
}

bool input_keyboard_changed(void) {
    bool have = input_have_keyboard();
    if (have == last_have)
        return false;
    last_have = have;
    return true;
}

bool input_poll_pointer(struct pointer_event *out) {
    return usb_poll_pointer(out);
}

#ifdef SMOKE
// "keys: usb 2 press 0x04", "keys: ps2 E0 48 make 0x52": what the QMP key tests look for.
static void log_raw(const struct raw_key *e) {
    if (e->src == RAW_USB) {
        if (e->flags & RAW_ROLLOVER)
            klog("keys: usb %s rollover", e->port);
        else
            klog("keys: usb %s %s 0x%02x", e->port, e->down ? "press" : "release", e->usage);
        return;
    }
    char bytes[24] = "";
    size_t n = 0;
    for (size_t i = 0; i < e->len && n + 4 < sizeof bytes; i++)
        n += (size_t)ksnprintf(bytes + n, sizeof bytes - n, "%s%02X", i ? " " : "", e->bytes[i]);
    if (e->flags & RAW_UNMAPPED)
        klog("keys: ps2 %s %s unmapped", bytes, e->down ? "make" : "break");
    else
        klog("keys: ps2 %s %s 0x%02x", bytes, e->down ? "make" : "break", e->usage);
}
#endif

bool input_poll_raw(struct raw_key *out) {
    if (!ps2_poll_raw(out) && !usb_poll_raw(out))
        return false;
#ifdef SMOKE
    log_raw(out);
#endif
    return true;
}

uint32_t input_raw_dropped(void) {
    return ps2_raw_dropped() + usb_raw_dropped();
}

void input_set_leds(uint8_t hid_bits) {
    if (ps2)
        ps2_set_leds(ps2_led_bits(hid_bits));
    usb_set_leds(hid_bits);
}
