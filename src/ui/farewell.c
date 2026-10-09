#include "ui/farewell.h"
#include "arch/x86.h"
#include "gfx/gfx.h"
#include "info/tsc.h"
#include "lib/serial.h"
#include "ui/shutdown_seq.h"

// The farewell before a reboot or shutdown (an homage to ULTRAKILL): the lines appear
// one at a time from the top over SHUTDOWN_MS, like a failing VCR picture. New lines
// shake with colour fringes for a few frames, bands of the screen tear sideways or
// turn to static (more often once shutdown is imminent), and it ends with a CRT
// switch-off. Everything is redrawn from the lines: reading the framebuffer is slow.

#define FRAME_MS 10
#define JITTER_FRAMES 3
#define CRT_FRAME_MS 16

static uint64_t rng, fake_ms;  // fake_ms: the clock when the TSC rate is unknown
static uint32_t w, h, th, margin, scale, fit;
static bool scan;
static int base;                            // first line on screen (a short screen restarts)
static uint8_t jitter[SHUTDOWN_LINES];      // frames of shaking left
static int band_y = -1, band_h;             // last frame's glitch band, to undo

static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 32);
}

static uint64_t now_ms(void) {
    return tsc_hz() ? tsc_ms() : fake_ms;
}

static void wait_until(uint64_t due) {
    if (!tsc_hz()) {
        if (due > fake_ms)
            tsc_delay_us((due - fake_ms) * 1000);
        fake_ms = due > fake_ms ? due : fake_ms;
        return;
    }
    while (tsc_ms() < due)
        cpu_pause();
}

static int slot_y(int i) {
    return (int)margin + (i - base) * (int)th;
}

// Redraws line i shifted by dx pixels, with red/cyan fringes when `fringe`.
static void draw_line(int i, int dx, bool fringe) {
    int y = slot_y(i), x = (int)margin + dx;
    const struct shutdown_line *l = &shutdown_lines[i];
    gfx_fill_rgb(0, (uint32_t)y, w, th, 0);
    if (fringe) {
        gfx_text_fx(x - 2 * (int)scale, y, l->text, 0x00A0A0, scan);
        gfx_text_fx(x + 2 * (int)scale, y, l->text, 0xA00020, scan);
    }
    gfx_text_fx(x, y, l->text, l->rgb, scan);
}

static int shake(void) {
    return ((int)(rnd() % 7) - 3) * (int)scale;
}

// Lines [base, shown) that overlap pixel rows [y, y + hh).
static void lines_in(int y, int hh, int shown, int *first, int *last) {
    *first = base + (y - (int)margin) / (int)th;
    *last = base + (y + hh - 1 - (int)margin) / (int)th;
    if (*first < base)
        *first = base;
    if (*last >= shown)
        *last = shown - 1;
}

static uint32_t noise(uint32_t x, uint32_t y, void *ctx) {
    (void)x, (void)y, (void)ctx;
    uint32_t r = rnd();
    uint32_t v = (r & 0xFF) < 24 ? 0xC0 + (r >> 8 & 0x3F) : (r >> 8 & 0x3F);  // mostly dark, some sparks
    return v * 0x010101;
}

static void undo_band(int shown) {
    if (band_y < 0)
        return;
    gfx_fill_rgb(0, (uint32_t)band_y, w, (uint32_t)band_h, 0);
    int first, last;
    lines_in(band_y, band_h, shown, &first, &last);
    for (int i = first; i <= last; i++)
        draw_line(i, 0, false);
    band_y = -1;
}

static void glitch(int shown) {
    if (rnd() % 256 >= glitch_chance(shown))
        return;
    band_h = (int)((4 + rnd() % 28) * scale);
    band_y = (int)(rnd() % h);
    if (band_y + band_h > (int)h)
        band_h = (int)h - band_y;
    if (rnd() & 1) {  // static
        gfx_paint_rect(0, (uint32_t)band_y, w, (uint32_t)band_h, noise, NULL);
        return;
    }
    int first, last, dx = (int)((16 + rnd() % 48) * scale) * (rnd() & 1 ? 1 : -1);
    lines_in(band_y, band_h, shown, &first, &last);
    for (int i = first; i <= last; i++)  // tear: whole lines jump sideways
        draw_line(i, dx, true);
}

static void crt_off(void) {
    struct fx_rect r, prev = { 0, 0, w, h, 0 };
    uint64_t due = now_ms();
    for (int step = 0; step < CRT_OFF_STEPS; step++) {
        crt_off_step(step, w, h, &r);
        gfx_fill_rgb(prev.x, prev.y, prev.w, prev.h, 0);
        gfx_fill_rgb(r.x, r.y, r.w, r.h, r.rgb);
        prev = r;
        due += CRT_FRAME_MS;
        wait_until(due);
    }
    gfx_fill_rgb(prev.x, prev.y, prev.w, prev.h, 0);
}

void farewell_play(void) {
    uint32_t gaps[SHUTDOWN_LINES - 1];
    rng = rdtsc() | 1;
    shutdown_gaps(rng, gaps, SHUTDOWN_LINES - 1, SHUTDOWN_MS);
    gfx_pause(true);
    gfx_fb_size(&w, &h);
    scale = (uint32_t)gfx_get_layout()->scale;
    th = 16 * scale;
    margin = 8 * scale;
    fit = h > 2 * margin + th ? (h - 2 * margin) / th : 1;
    scan = scale >= 2;
    base = 0;
    band_y = -1;
    gfx_fill_rgb(0, 0, w, h, 0);

    uint64_t t0 = now_ms(), due[SHUTDOWN_LINES];
    due[0] = t0;
    for (int i = 1; i < SHUTDOWN_LINES; i++)
        due[i] = due[i - 1] + gaps[i - 1];
    uint64_t end = due[SHUTDOWN_LINES - 1] + SHUTDOWN_TAIL_MS, frame = t0;
    int shown = 0;
    for (;;) {
        uint64_t t = now_ms();
        undo_band(shown);
        while (shown < SHUTDOWN_LINES && due[shown] <= t) {
            if (shown - base == (int)fit) {  // a short screen: start again at the top
                gfx_fill_rgb(0, 0, w, h, 0);
                base = shown;
            }
            jitter[shown] = JITTER_FRAMES + 1;
            klog("shutdown: %s", shutdown_lines[shown].text);
            shown++;
        }
        for (int i = base; i < shown; i++) {
            if (!jitter[i])
                continue;
            jitter[i]--;
            draw_line(i, jitter[i] ? shake() : 0, jitter[i] != 0);  // the last frame settles
        }
        if (shown == SHUTDOWN_LINES && t >= end)
            break;
        glitch(shown);
        frame += FRAME_MS;
        wait_until(frame);
    }
    undo_band(shown);
    crt_off();
}
