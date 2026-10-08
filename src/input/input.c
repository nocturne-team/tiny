#include "input/input.h"
#include "input/ps2.h"
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
