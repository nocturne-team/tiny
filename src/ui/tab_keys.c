#include "gfx/gfx.h"
#include "info/tsc.h"
#include "input/input.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "ui/kbd_layout.h"
#include "ui/keys_state.h"
#include "ui/tabs.h"
#include "usb/usb.h"

#define K_TEXT ATTR(C_LGRAY, C_BLACK)
#define K_HEAD ATTR(C_YELLOW, C_BLACK)
#define K_DIM ATTR(C_CYAN, C_BLACK)
#define K_WARN ATTR(C_LRED, C_BLACK)
#define K_BORDER ATTR(C_DGRAY, C_BLACK)
#define K_TESTED ATTR(C_LGREEN, C_BLACK)
#define K_TESTED_BORDER ATTR(C_GREEN, C_BLACK)
#define K_HELD ATTR(C_BLACK, C_WHITE)
#define K1_UNTESTED ATTR(C_WHITE, C_DGRAY)  // one-row keys: the background is the key
#define K1_TESTED ATTR(C_BLACK, C_GREEN)
#define K_MODAL ATTR(C_WHITE, C_BLUE)
#define K_MODAL_HEAD ATTR(C_YELLOW, C_BLUE)

// CP437 single-line box drawing.
#define B_H 0xC4
#define B_V 0xB3
#define B_TL 0xDA
#define B_TR 0xBF
#define B_BL 0xC0
#define B_BR 0xD9
// CP437 double-line box drawing, for the start box.
#define D_H 0xCD
#define D_V 0xBA
#define D_TL 0xC9
#define D_TR 0xBB
#define D_BL 0xC8
#define D_BR 0xBC

static struct keys_state ks;
static struct kbd_geom geom;
static uint64_t spins;

// Milliseconds; without a TSC rate, ~8k loop iterations count as one.
static uint64_t now_ms(void) {
    return tsc_hz() ? tsc_ms() : spins >> 13;
}

static void label(int x, int y, int inner, const char *s, uint8_t attr) {
    int n = (int)strlen(s);
    if (n > inner)
        n = inner;
    gfx_puts(x + (inner - n) / 2, y, s, attr, n);
}

static void draw_key(int bx, int by, const struct kbd_rect *r, const struct kbd_key *k) {
    bool held = keys_is_held(&ks, k->usage), tested = keys_is_tested(&ks, k->usage);
    int x0 = bx + r->x, y0 = by + r->y;
    if (geom.key_h == 1) {
        uint8_t a = held ? K_HELD : tested ? K1_TESTED : K1_UNTESTED;
        gfx_fill(x0, y0, r->w - 1, r->h, ' ', a);  // the last column is the gap to the next key
        label(x0, y0 + r->h / 2, r->w - 1, k->label, a);
        return;
    }
    uint8_t b = held ? K_HELD : tested ? K_TESTED_BORDER : K_BORDER;
    uint8_t t = held ? K_HELD : tested ? K_TESTED : K_TEXT;
    int x1 = x0 + r->w - 1, y1 = y0 + r->h - 1;
    gfx_fill(x0, y0, r->w, r->h, ' ', b);
    gfx_fill(x0 + 1, y0, r->w - 2, 1, B_H, b);
    gfx_fill(x0 + 1, y1, r->w - 2, 1, B_H, b);
    gfx_fill(x0, y0 + 1, 1, r->h - 2, B_V, b);
    gfx_fill(x1, y0 + 1, 1, r->h - 2, B_V, b);
    gfx_put(x0, y0, B_TL, b);
    gfx_put(x1, y0, B_TR, b);
    gfx_put(x0, y1, B_BL, b);
    gfx_put(x1, y1, B_BR, b);
    label(x0 + 1, y0 + r->h / 2, r->w - 2, k->label, t);
}

static const char *const modal_text[] = {
    "Keyboard tester",
    "",
    "Press Enter to start.",
    "Every key is then tested; hold Esc for 1 s to stop.",
};
#define MODAL_LINES (int)(sizeof modal_text / sizeof modal_text[0])

// The box over the keyboard picture while the tester is idle.
static void draw_modal(int x, int y, int w, int h) {
    int inner = 0;
    for (int i = 0; i < MODAL_LINES; i++)
        if ((int)strlen(modal_text[i]) > inner)
            inner = (int)strlen(modal_text[i]);
    int bw = inner + 6, bh = MODAL_LINES + 4;
    if (bw > w)
        bw = w;
    if (bh > h)
        bh = h;
    int bx = x + (w - bw) / 2, by = y + (h - bh) / 2;
    gfx_fill(bx, by, bw, bh, ' ', K_MODAL);
    gfx_fill(bx + 1, by, bw - 2, 1, D_H, K_MODAL);
    gfx_fill(bx + 1, by + bh - 1, bw - 2, 1, D_H, K_MODAL);
    gfx_fill(bx, by + 1, 1, bh - 2, D_V, K_MODAL);
    gfx_fill(bx + bw - 1, by + 1, 1, bh - 2, D_V, K_MODAL);
    gfx_put(bx, by, D_TL, K_MODAL);
    gfx_put(bx + bw - 1, by, D_TR, K_MODAL);
    gfx_put(bx, by + bh - 1, D_BL, K_MODAL);
    gfx_put(bx + bw - 1, by + bh - 1, D_BR, K_MODAL);
    for (int i = 0; i < MODAL_LINES && 2 + i < bh - 1; i++)
        label(bx + 1, by + 2 + i, bw - 2, modal_text[i], i == 0 ? K_MODAL_HEAD : K_MODAL);
}

static void keys_draw(const struct page *p, int x, int y, int w, int h) {
    int ty = y, end = y + h;
    char buf[128];
    bool geom_ok = kbd_place(w, h, &geom);
    if (geom_ok) {
        int bx = x + (w - geom.width) / 2;
        for (int i = 0; i < KBD_KEYS; i++)
            draw_key(bx, y, &geom.r[i], &kbd_keys[i]);
        ty = y + geom.height + 1;
    } else {
        gfx_puts(x, ty, "Screen too small for the keyboard picture", K_DIM, w);
        ty += 2;
    }
    if (ty < end) {
        int n = ksnprintf(buf, sizeof buf, "Held now %u   most at once %u", ks.held_n, ks.max_held);
        if (usb_keyboards() && n > 0 && (size_t)n < sizeof buf)
            ksnprintf(buf + n, sizeof buf - (size_t)n, "   (USB boot keyboards report at most 6)");
        gfx_puts(x, ty++, buf, K_TEXT, w);
    }
    if (ty < end) {
        ksnprintf(buf, sizeof buf, "Lock LEDs: Caps %s, Num %s, Scroll %s", ks.leds & 2 ? "on" : "off",
                  ks.leds & 1 ? "on" : "off", ks.leds & 4 ? "on" : "off");
        gfx_puts(x, ty++, buf, K_TEXT, w);
    }
    if (ty < end && ks.tested_n) {
        unsigned left = keys_untested(&ks);
        if (left)
            ksnprintf(buf, sizeof buf, "Untested: %u key%s", left, left == 1 ? "" : "s");
        else
            ksnprintf(buf, sizeof buf, "Every key on the picture has been pressed");
        gfx_puts(x, ty++, buf, K_TEXT, w);
    }
    uint32_t dropped = input_raw_dropped();
    if (ty < end && dropped) {
        ksnprintf(buf, sizeof buf, "%u events dropped (a key may look held until pressed again)", dropped);
        gfx_puts(x, ty++, buf, K_WARN, w);
    }
    ty++;
    if (ty < end && (ks.nlines || p->raw))
        gfx_puts(x, ty++, ks.nlines ? "Recent events" : "Press any key.", ks.nlines ? K_HEAD : K_DIM, w);
    for (size_t i = 0; i < ks.nlines && ty < end; i++) {
        keys_line_text(&ks.lines[i], buf, sizeof buf);
        gfx_puts(x, ty++, buf, ks.lines[i].ev.flags & RAW_ROLLOVER ? K_WARN : K_TEXT, w);
    }
    if (!p->raw)
        draw_modal(x, y, w, geom_ok ? geom.height : h);
}

static bool keys_raw(struct page *p, const struct raw_key *ev) {
    (void)p;
    if (keys_apply(&ks, ev, now_ms()))
        input_set_leds(ks.leds);
    return true;
}

static void stop(struct page *p) {
    keys_reset_held(&ks);
    p->raw = NULL;
}

// Holding Esc stops the run and brings the start box back.
static bool keys_update(struct page *p) {
    spins++;
    if (!p->raw || !keys_esc_held(&ks, now_ms()))
        return false;
    stop(p);
    klog("keys: stopped, %u keys tested", ks.tested_n);
    return true;
}

// Idle until Enter: arriving on the page (or walking past it) must not grab the keyboard.
static bool keys_key(struct page *p, const struct key_event *k) {
    if (p->raw || k->key != KEY_ENTER || k->repeat)
        return false;
    keys_start(&ks);
    p->raw = keys_raw;
    klog("keys: started");
    return true;
}

// A run left by the mouse ends there; keys held meanwhile would otherwise stay lit.
static void keys_enter(struct page *p) {
    stop(p);
}

void tab_keys_init(struct page *p) {
    page_clear(p);
    p->title = "Keys";
    p->key = keys_key;
    p->draw = keys_draw;
    p->update = keys_update;
    p->enter = keys_enter;
}
