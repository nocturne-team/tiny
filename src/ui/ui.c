#include "ui/ui.h"
#include "arch/x86.h"
#include "gfx/gfx.h"
#include "info/rtc.h"
#include "info/tsc.h"
#include "input/input.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "power.h"
#include "ui/about_art.h"

#define A_FRAME   ATTR(C_LCYAN, C_BLACK)
#define A_TITLE   ATTR(C_WHITE, C_BLACK)
#define A_TEXT    ATTR(C_LGRAY, C_BLACK)
#define A_HEAD    ATTR(C_YELLOW, C_BLACK)
#define A_DIM     ATTR(C_CYAN, C_BLACK)
#define A_WARN    ATTR(C_LRED, C_BLACK)
#define A_ART     ATTR(C_WHITE, C_BLACK)
#define A_TAB_SEL ATTR(C_BLACK, C_CYAN)
#define A_ALERT   ATTR(C_YELLOW, C_RED)

// CP437 double-line box drawing and arrows.
#define G_H  0xCD
#define G_V  0xBA
#define G_TL 0xC9
#define G_TR 0xBB
#define G_BL 0xC8
#define G_BR 0xBC
#define G_LT 0xCC
#define G_RT 0xB9
#define G_UP 0x18
#define G_DN 0x19

#define TABS_END_X 56  // tab labels end before this column; the clock may use the rest

static struct page *pages;
static size_t page_count, cur;
static bool keyboard;
static char pending;  // 'r' or 's' while waiting for confirmation
static int cols, rows;

static size_t visible(void) {
    return (size_t)(rows - 6);
}

static uint8_t style_attr(uint8_t style) {
    switch (style) {
    case STYLE_HEAD: return A_HEAD;
    case STYLE_DIM: return A_DIM;
    case STYLE_WARN: return A_WARN;
    default: return A_TEXT;
    }
}

static void hline(int y, uint8_t left, uint8_t right) {
    gfx_put(0, y, left, A_FRAME);
    gfx_fill(1, y, cols - 2, 1, G_H, A_FRAME);
    gfx_put(cols - 1, y, right, A_FRAME);
}

static void draw_frame(void) {
    gfx_fill(0, 0, cols, rows, ' ', A_TEXT);
    hline(0, G_TL, G_TR);
    hline(2, G_LT, G_RT);
    hline(rows - 3, G_LT, G_RT);
    hline(rows - 1, G_BL, G_BR);
    for (int y = 1; y < rows - 1; y++) {
        if (y == 2 || y == rows - 3)
            continue;
        gfx_put(0, y, G_V, A_FRAME);
        gfx_put(cols - 1, y, G_V, A_FRAME);
    }
    gfx_puts(2, 0, " tiny ", A_TITLE, cols - 4);
}

static void draw_tabs(void) {
    gfx_fill(1, 1, TABS_END_X - 1, 1, ' ', A_TEXT);
    int x = 2;
    for (size_t i = 0; i < page_count; i++) {
        char label[24];
        ksnprintf(label, sizeof label, " F%zu %s ", i + 1, pages[i].title);
        x += gfx_puts(x, 1, label, i == cur ? A_TAB_SEL : A_TEXT, cols - 3 - x) + 1;
    }
}

static void draw_clock(void) {
    char up[24], buf[64];
    struct datetime dt;
    if (tsc_hz())
        fmt_uptime(up, sizeof up, tsc_uptime_seconds());
    else
        ksnprintf(up, sizeof up, "?");
    if (rtc_read(&dt, NULL))
        ksnprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d  up %s", dt.year, dt.month,
                  dt.day, dt.hour, dt.min, dt.sec, up);
    else
        ksnprintf(buf, sizeof buf, "up %s", up);

    int avail = cols - 2 - TABS_END_X;
    gfx_fill(TABS_END_X, 1, avail, 1, ' ', A_TEXT);
    int len = (int)strlen(buf);
    if (len > avail && buf[4] == '-') {  // narrow screen: drop the date
        memmove(buf, buf + 11, strlen(buf + 11) + 1);
        len = (int)strlen(buf);
    }
    if (len <= avail)
        gfx_puts(cols - 2 - len, 1, buf, A_TEXT, len);
}

static void draw_content(void) {
    struct page *p = &pages[cur];
    size_t vis = visible();
    for (size_t i = 0; i < vis; i++) {
        int y = 3 + (int)i;
        gfx_fill(1, y, cols - 2, 1, ' ', A_TEXT);
        size_t li = p->scroll + i;
        if (li >= p->count)
            continue;
        const struct line *l = &p->lines[li];
        if (l->style == STYLE_ART) {
            int row = (unsigned char)l->text[0] - 1;
            for (int c = 0; row >= 0 && row < ABOUT_ART_ROWS && c < ABOUT_ART_COLS && c < cols - 4; c++)
                gfx_put_braille(2 + c, y, about_art[row][c], A_ART);
        } else {
            gfx_puts(2, y, l->text, style_attr(l->style), cols - 4);
        }
    }
    // Scroll hints in the right border.
    gfx_put(cols - 1, 3, p->scroll > 0 ? G_UP : G_V, A_FRAME);
    gfx_put(cols - 1, 3 + (int)vis - 1, p->scroll + vis < p->count ? G_DN : G_V, A_FRAME);
}

static void draw_footer(void) {
    const char *msg = "\x1b\x1a/F1-F5 tab  \x18\x19 PgUp/PgDn Home/End scroll  R reboot  S shut down";
    uint8_t attr = A_TEXT;
    if (pending == 'r') {
        msg = "Press R again to REBOOT - any other key cancels";
        attr = A_ALERT;
    } else if (pending == 's') {
        msg = "Press S again to SHUT DOWN - any other key cancels";
        attr = A_ALERT;
    } else if (!keyboard) {
        msg = "No PS/2 keyboard - USB input not yet supported";
        attr = A_ALERT;
    }
    gfx_fill(1, rows - 2, cols - 2, 1, ' ', A_TEXT);
    gfx_puts(2, rows - 2, msg, attr, cols - 4);
}

static void handle(const struct key_event *k) {
    if (pending) {
        char want = pending;
        pending = 0;
        if (k->key == KEY_CHAR && k->ch == want) {
            if (want == 'r')
                power_reboot();
            power_shutdown();
        }
        draw_footer();
        return;
    }

    size_t vis = visible();
    long page_step = vis > 1 ? (long)vis - 1 : 1;
    struct page *p = &pages[cur];
    switch (k->key) {
    case KEY_LEFT: cur = (cur + page_count - 1) % page_count; break;
    case KEY_RIGHT: cur = (cur + 1) % page_count; break;
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
        if ((size_t)(k->key - KEY_F1) < page_count)
            cur = (size_t)(k->key - KEY_F1);
        break;
    case KEY_UP: page_scroll(p, -1, vis); break;
    case KEY_DOWN: page_scroll(p, 1, vis); break;
    case KEY_PGUP: page_scroll(p, -page_step, vis); break;
    case KEY_PGDN: page_scroll(p, page_step, vis); break;
    case KEY_HOME: page_scroll_to(p, false, vis); break;
    case KEY_END: page_scroll_to(p, true, vis); break;
    case KEY_CHAR:
        if (k->ch == 'r' || k->ch == 's')
            pending = k->ch;
        break;
    default: break;
    }
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
}

void ui_run(struct page *pg, size_t count, bool have_keyboard) {
    pages = pg;
    page_count = count;
    keyboard = have_keyboard;
    cols = gfx_cols();
    rows = gfx_rows();

    draw_frame();
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
    gfx_flush();
    klog("UI READY");
#if defined(SMOKE_SHUTDOWN)
    power_shutdown();
#elif defined(SMOKE)
    qemu_debug_exit(0x10);
#endif

    uint64_t hz = tsc_hz(), last = rdtsc();
    unsigned long spins = 0;
    for (;;) {
        bool dirty = false;
        struct key_event k;
        while (input_poll(&k)) {
            handle(&k);
            dirty = true;
        }
        bool tick = hz ? rdtsc() - last >= hz / 2 : ++spins >= (1ul << 22);
        if (tick) {
            last = rdtsc();
            spins = 0;
            draw_clock();
            dirty = true;
        }
        if (dirty)
            gfx_flush();
        cpu_pause();
    }
}
