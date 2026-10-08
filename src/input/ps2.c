#include "input/ps2.h"
#include "arch/x86.h"
#include "input/ps2_decode.h"
#include "lib/serial.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64
#define ST_OUT_FULL 0x01
#define ST_IN_FULL 0x02
#define TIMEOUT 100000

static struct ps2_decoder decoder;
static bool present;

static bool wait_write(void) {
    for (int i = 0; i < TIMEOUT; i++) {
        if (!(inb(PS2_STATUS) & ST_IN_FULL))
            return true;
        io_wait();
    }
    return false;
}

static bool wait_read(void) {
    for (int i = 0; i < TIMEOUT; i++) {
        if (inb(PS2_STATUS) & ST_OUT_FULL)
            return true;
        io_wait();
    }
    return false;
}

static bool command(uint8_t c) {
    if (!wait_write())
        return false;
    outb(PS2_CMD, c);
    return true;
}

static bool write_data(uint8_t v) {
    if (!wait_write())
        return false;
    outb(PS2_DATA, v);
    return true;
}

static int read_data(void) {
    return wait_read() ? inb(PS2_DATA) : -1;
}

bool ps2_init(void) {
    if (inb(PS2_STATUS) == 0xFF) {
        klog("ps2: no controller (status port reads 0xff)");
        return false;
    }
    if (!command(0xAD) || !command(0xA7))  // disable both ports
        goto fail;
    for (int i = 0; i < 16 && (inb(PS2_STATUS) & ST_OUT_FULL); i++)
        inb(PS2_DATA);  // flush stale bytes

    if (!command(0x20))
        goto fail;
    int config = read_data();
    if (config < 0)
        goto fail;
    config &= ~0x03;  // no IRQs: we poll
    config &= ~0x10;  // port 1 clock on
    config |= 0x40;   // keep set 2 -> set 1 translation

    if (!command(0xAA) || read_data() != 0x55)  // controller self-test
        goto fail;
    // The self-test can reset the config byte, so write ours afterwards.
    if (!command(0x60) || !write_data((uint8_t)config) || !command(0xAE))
        goto fail;
    write_data(0xF4);  // keyboard: enable scanning (some firmware leaves it off)
    read_data();       // ACK (0xFA) or nothing; either is fine

    present = true;
    klog("ps2: keyboard controller ok");
    return true;
fail:
    klog("ps2: no controller responding, keyboard disabled");
    return false;
}

bool ps2_poll(struct key_event *out) {
    if (!present)
        return false;
    // Bounded so a wedged controller (OUT_FULL stuck on) cannot freeze the UI.
    for (int i = 0; i < 32 && (inb(PS2_STATUS) & ST_OUT_FULL); i++) {
        if (ps2_decode(&decoder, inb(PS2_DATA), out))
            return true;
    }
    return false;
}
