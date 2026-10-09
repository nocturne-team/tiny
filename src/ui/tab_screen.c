#include "gfx/gfx.h"
#include "info/edid.h"
#include "info/tsc.h"
#include "input/input.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "ui/screen_test.h"
#include "ui/tabs.h"

#define LABEL_MS 1500  // the pattern name stays up this long

static int cur = -1;  // pattern on screen, -1 = showing the tab
static bool label;
static uint64_t label_since, spins;
static uint8_t buttons;

// Milliseconds; without a TSC rate, ~8k loop iterations count as one.
static uint64_t now_ms(void) {
    return tsc_hz() ? tsc_ms() : spins >> 13;
}

static uint32_t pixel(uint32_t x, uint32_t y, void *ctx) {
    const uint32_t *wh = ctx;
    return screen_pixel(cur, x, y, wh[0], wh[1]);
}

// White on the card's dark centre.
static void centre_text(const char *s, uint32_t y, uint32_t w, uint32_t scale) {
    uint32_t tw = (uint32_t)strlen(s) * 8 * scale;
    gfx_text_rgb(tw < w ? (w - tw) / 2 : 0, y, s, 0xFFFFFF, 0x202020);
}

static void draw_pattern(void) {
    uint32_t wh[2];
    gfx_fb_size(&wh[0], &wh[1]);
    if (screen_pattern_solid(cur))
        gfx_fill_rgb(0, 0, wh[0], wh[1], screen_pixel(cur, 0, 0, wh[0], wh[1]));
    else
        gfx_paint(pixel, wh);
    uint32_t scale = (uint32_t)gfx_get_layout()->scale, th = 16 * scale;
    char buf[96];
    if (cur == 0) {  // the test card's centre: name and mode, above the crosshair
        uint32_t y = wh[1] / 2 - wh[1] * 5 / 12 * 3 / 10;
        ksnprintf(buf, sizeof buf, "%u x %u", wh[0], wh[1]);
        centre_text("tiny", y, wh[0], scale);
        centre_text(buf, y + th + th / 2, wh[0], scale);
    }
    if (!label)
        return;
    ksnprintf(buf, sizeof buf, " %d/%d %s  -  Space next, Backspace back, Esc leave ", cur + 1,
              SCREEN_PATTERNS, screen_pattern_name(cur));
    uint32_t tw = (uint32_t)strlen(buf) * 8 * scale;
    gfx_text_rgb(tw < wh[0] ? (wh[0] - tw) / 2 : 0, th < wh[1] ? (wh[1] - th) / 2 : 0, buf, 0xFFFFFF,
                 0x404040);
}

static void show(int pattern) {
    cur = pattern;
    label = true;
    label_since = now_ms();
    klog("screen: pattern %d %s", cur + 1, screen_pattern_name(cur));
    draw_pattern();
}

static bool screen_raw(struct page *p, const struct raw_key *ev);

static void start(struct page *p) {
    gfx_pause(true);
    buttons = 0;
    p->raw = screen_raw;
    show(0);
}

static void stop(struct page *p) {
    cur = -1;
    p->raw = NULL;
    gfx_pause(false);
    klog("screen: done");
}

// Applies a step; true when that ended the test.
static bool step(struct page *p, enum screen_action a) {
    if (a == SCREEN_NONE)
        return false;
    int n = screen_step(cur, a);
    if (n < 0) {
        stop(p);
        return true;
    }
    if (n != cur)
        show(n);
    return false;
}

static bool screen_raw(struct page *p, const struct raw_key *ev) {
    return step(p, screen_key(ev));
}

static bool screen_key_hook(struct page *p, const struct key_event *k) {
    if (k->key != KEY_ENTER || k->repeat)
        return false;
    start(p);
    return true;
}

static bool screen_pointer(struct page *p, const struct pointer_event *e) {
    if (cur < 0)
        return false;
    uint8_t pressed = e->buttons & (uint8_t)~buttons;
    buttons = e->buttons;
    if (pressed & 1)
        step(p, SCREEN_NEXT);
    else if (pressed & 2)
        step(p, SCREEN_PREV);
    return true;  // while the patterns are up, the mouse belongs to them
}

// Takes the pattern name down once it has been up for LABEL_MS.
static bool screen_update(struct page *p) {
    (void)p;
    spins++;
    if (cur >= 0 && label && now_ms() - label_since >= LABEL_MS) {
        label = false;
        draw_pattern();
    }
    return false;
}

void tab_screen_init(struct page *p, const struct limine_framebuffer *fb, const struct edid_info *edid) {
    page_clear(p);
    p->title = "Screen";
    p->key = screen_key_hook;
    p->pointer = screen_pointer;
    p->update = screen_update;

    const struct gfx_layout *l = gfx_get_layout();
    page_add(p, STYLE_HEAD, "Display mode");
    page_kv(p, "Framebuffer", "%lu x %lu, %u bpp, pitch %lu bytes", (unsigned long)fb->width,
            (unsigned long)fb->height, fb->bpp, (unsigned long)fb->pitch);
    page_kv(p, "Pixel format", "red %u bits at %u, green %u at %u, blue %u at %u", fb->red_mask_size,
            fb->red_mask_shift, fb->green_mask_size, fb->green_mask_shift, fb->blue_mask_size,
            fb->blue_mask_shift);
    page_kv(p, "Text grid", "%d x %d cells, font scale %d", l->cols, l->rows, l->scale);
    uint16_t pw = edid ? edid->pref_width : 0, ph = edid ? edid->pref_height : 0;
    if (pw && ph)
        page_kv(p, "Monitor wants", "%u x %u at %u.%02u Hz (EDID preferred mode)", pw, ph,
                edid->pref_centihz / 100, edid->pref_centihz % 100);
    else
        page_kv(p, "Monitor wants", "unknown (no EDID preferred mode)");
    switch (screen_native(fb->width, fb->height, pw, ph)) {
    case SCREEN_NATIVE_YES:
        page_add(p, STYLE_DIM, "Native resolution: one pixel of a pattern is one pixel of the panel.");
        break;
    case SCREEN_NATIVE_NO:
        page_add(p, STYLE_WARN, "Not the panel's native resolution: the screen scales the picture, so the");
        page_add(p, STYLE_WARN, "checkerboard and grid will look soft or uneven. Colours are still fine.");
        break;
    default:
        break;
    }

    page_blank(p);
    page_add(p, STYLE_HEAD, "Test patterns (full screen)");
    page_kv(p, "Test card", "an overview in the style of Test Card F: colour bars, grey steps,");
    page_kv(p, "", "gratings from 6 px to 1 px wide, a grid and a castellated edge");
    page_kv(p, "Black ... Blue", "dead (always dark) and stuck (always lit) pixels, backlight bleed");
    page_kv(p, "Grey 50%", "uniformity: clouding, darker corners, tint");
    page_kv(p, "Gradients", "red, green, blue and grey ramps: banding means fewer than 8 bits");
    page_kv(p, "Checkerboard", "1 px black/white: blur from scaling, flicker");
    page_kv(p, "Grid", "32 px lines; the red line is the outermost pixel row/column, so");
    page_kv(p, "", "if part of it is missing the screen crops (overscan)");

    page_blank(p);
    page_add(p, STYLE_HEAD, "Keys");
    page_kv(p, "Enter", "start");
    page_kv(p, "Next", "Space, \x1a, \x19, PgDn or left click (after the last, back here)");
    page_kv(p, "Previous", "Backspace, \x1b, \x18, PgUp or right click");
    page_kv(p, "Leave", "Esc or Q");
}
