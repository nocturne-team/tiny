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
#include "ui/pointer.h"
#include "ui/farewell.h"

#define A_FRAME   ATTR(C_LCYAN, C_BLACK)
#define A_TITLE   ATTR(C_WHITE, C_BLACK)
#define A_TEXT    ATTR(C_LGRAY, C_BLACK)
#define A_HEAD    ATTR(C_YELLOW, C_BLACK)
#define A_DIM     ATTR(C_CYAN, C_BLACK)
#define A_WARN    ATTR(C_LRED, C_BLACK)
#define A_ART     ATTR(C_WHITE, C_BLACK)
#define A_GRAPH   ATTR(C_LGREEN, C_BLACK)
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

#define CONTENT_Y 4  // first content row: row 1 groups, row 2 sub-tabs, row 3 the rule

static struct page *pages;
static size_t page_count;
static const struct ui_group *groups;
static size_t group_count;
static uint8_t last_page[UI_MAX_GROUPS];  // the page each group showed last
static struct ui_nav nav;  // current and previous page, pending R/S confirmation
static int cols, rows;
static struct pointer ptr;
static char group_labels[UI_MAX_GROUPS][24], sub_labels[UI_MAX_PAGES][24];
static const char *group_label_ptrs[UI_MAX_GROUPS], *sub_label_ptrs[UI_MAX_PAGES];
static size_t sub_count;  // labels of the current group in sub_labels
static int tabs_end;  // first column after the group labels; the clock may use the rest
static uint8_t last_buttons;
static bool farewell_on = true;

void ui_set_farewell(bool on) {
    farewell_on = on;
}

static size_t visible(void) {
    return (size_t)(rows - 7);
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
    hline(3, G_LT, G_RT);
    hline(rows - 3, G_LT, G_RT);
    hline(rows - 1, G_BL, G_BR);
    for (int y = 1; y < rows - 1; y++) {
        if (y == 3 || y == rows - 3)
            continue;
        gfx_put(0, y, G_V, A_FRAME);
        gfx_put(cols - 1, y, G_V, A_FRAME);
    }
    gfx_puts(2, 0, " tiny ", A_TITLE, cols - 4);
}

// Draws `labels` from column 2 on row y, one blank column apart; `sel` highlighted.
// Returns the first column after the last label.
static int draw_labels(int y, const char *const *labels, size_t n, size_t sel) {
    gfx_fill(1, y, cols - 2, 1, ' ', A_TEXT);
    int x = 2;
    for (size_t i = 0; i < n; i++)
        x += gfx_puts(x, y, labels[i], i == sel ? A_TAB_SEL : A_TEXT, cols - 3 - x) + 1;
    return x;
}

static void draw_tabs(void) {
    int gi = group_of(groups, group_count, nav.cur);
    tabs_end = draw_labels(1, group_label_ptrs, group_count, (size_t)gi) + 1;
    const struct ui_group *g = &groups[gi < 0 ? 0 : gi];
    sub_count = g->count;
    for (size_t i = 0; i < sub_count; i++) {
        ksnprintf(sub_labels[i], sizeof sub_labels[i], " %zu %s ", i + 1, pages[g->first + i].title);
        sub_label_ptrs[i] = sub_labels[i];
    }
    draw_labels(2, sub_label_ptrs, sub_count, nav.cur - g->first);
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

    int avail = cols - 2 - tabs_end;
    if (avail <= 0)
        return;
    gfx_fill(tabs_end, 1, avail, 1, ' ', A_TEXT);
    int len = (int)strlen(buf);
    if (len > avail && buf[4] == '-') {  // narrow screen: drop the date
        memmove(buf, buf + 11, strlen(buf + 11) + 1);
        len = (int)strlen(buf);
    }
    if (len <= avail)
        gfx_puts(cols - 2 - len, 1, buf, A_TEXT, len);
}

static void draw_content(void) {
    struct page *p = &pages[nav.cur];
    size_t vis = visible();
    if (p->draw) {
        gfx_fill(1, CONTENT_Y, cols - 2, (int)vis, ' ', A_TEXT);
        p->draw(p, 2, CONTENT_Y, cols - 4, (int)vis);
        gfx_put(cols - 1, CONTENT_Y, G_V, A_FRAME);
        gfx_put(cols - 1, CONTENT_Y + (int)vis - 1, G_V, A_FRAME);
        return;
    }
    for (size_t i = 0; i < vis; i++) {
        int y = CONTENT_Y + (int)i;
        gfx_fill(1, y, cols - 2, 1, ' ', A_TEXT);
        size_t li = p->scroll + i;
        if (li >= p->count)
            continue;
        const struct line *l = &p->lines[li];
        if (l->style == STYLE_ART) {
            int row = (unsigned char)l->text[0] - 1;
            for (int c = 0; row >= 0 && row < ABOUT_ART_ROWS && c < ABOUT_ART_COLS && c < cols - 4; c++)
                gfx_put_braille(2 + c, y, about_art[row][c], A_ART);
        } else if (l->style == STYLE_GRAPH) {
            int n = (unsigned char)l->text[0];
            for (int c = 0; c < n && c < cols - 4 - PAGE_KEY_WIDTH; c++)
                gfx_put_braille(2 + PAGE_KEY_WIDTH + c, y, (uint8_t)l->text[1 + c], A_GRAPH);
        } else {
            gfx_puts(2, y, l->text, style_attr(l->style), cols - 4);
        }
    }
    // Scroll hints in the right border.
    gfx_put(cols - 1, CONTENT_Y, p->scroll > 0 ? G_UP : G_V, A_FRAME);
    gfx_put(cols - 1, CONTENT_Y + (int)vis - 1, p->scroll + vis < p->count ? G_DN : G_V, A_FRAME);
}

static void draw_footer(void) {
    char keys[96];
    ksnprintf(keys, sizeof keys,
              "\x1b\x1a page  F1-F%zu group  1-9 sub-tab  \x18\x19 PgUp/PgDn Home/End scroll  R reboot  S shut down",
              group_count);
    char shortkeys[48];
    ksnprintf(shortkeys, sizeof shortkeys, "\x1b\x1a page  F1-F%zu group  \x18\x19 scroll  R reboot  S off",
              group_count);
    const char *msg = cols >= 72 ? keys : shortkeys;
    uint8_t attr = A_TEXT;
    if (nav.pending == 'r') {
        msg = "Press R again to REBOOT - any other key cancels";
        attr = A_ALERT;
    } else if (nav.pending == 's') {
        msg = "Press S again to SHUT DOWN - any other key cancels";
        attr = A_ALERT;
    } else if (!input_have_keyboard()) {
        msg = "No keyboard found - plug in a USB keyboard";
        attr = A_ALERT;
    } else if (pages[nav.cur].raw) {
        msg = "Hold Esc to stop  -  keys light up as you press them";
    }
    gfx_fill(1, rows - 2, cols - 2, 1, ' ', A_TEXT);
    gfx_puts(2, rows - 2, msg, attr, cols - 4);
}

// After a page hook took over or gave back the screen.
static void draw_all(void) {
    draw_frame();
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
}

// Shows page t: remembers it as its group's page, logs it, redraws.
static void go_to(size_t t) {
    if (!nav_switch(&nav, pages, t))
        return;
    group_remember(groups, group_count, last_page, t);
    int gi = group_of(groups, group_count, t);
    klog("ui: page %s/%s", gi >= 0 ? groups[gi].title : "?", pages[t].title);
#ifdef SMOKE_KEYS
    // Entering a page that takes keys (key tester, screen test) keeps the test going.
    if (!pages[nav.cur].raw && !pages[nav.cur].key)
        qemu_debug_exit(0x10);
#endif
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
}

static void handle(const struct key_event *k) {
    // Letter keys are commands (R/S confirm); a held key must not confirm itself.
    if (k->key == KEY_CHAR && k->repeat)
        return;
    if (nav.pending) {
        char want = nav.pending;
        nav.pending = 0;
        if (k->key == KEY_CHAR && k->ch == want) {
            if (farewell_on)
                farewell_play();
            if (want == 'r')
                power_reboot();
            power_shutdown();
        }
        draw_footer();
        return;
    }

    size_t next = nav.cur;
    size_t vis = visible();
    long page_step_lines = vis > 1 ? (long)vis - 1 : 1;
    struct page *p = &pages[nav.cur];
    if (p->key && p->key(p, k)) {
        draw_all();
        return;
    }
    switch (k->key) {
    case KEY_LEFT: next = page_step(page_count, nav.cur, -1); break;
    case KEY_RIGHT: next = page_step(page_count, nav.cur, 1); break;
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
    case KEY_F6:
    case KEY_F7:
    case KEY_F8:
    case KEY_F9:
        next = group_key(groups, group_count, last_page, nav.cur, (size_t)(k->key - KEY_F1));
        break;
    case KEY_UP: page_scroll(p, -1, vis); break;
    case KEY_DOWN: page_scroll(p, 1, vis); break;
    case KEY_PGUP: page_scroll(p, -page_step_lines, vis); break;
    case KEY_PGDN: page_scroll(p, page_step_lines, vis); break;
    case KEY_HOME: page_scroll_to(p, false, vis); break;
    case KEY_END: page_scroll_to(p, true, vis); break;
    case KEY_CHAR:
        if (k->ch == 'r' || k->ch == 's') {
            nav.pending = k->ch;
        } else if (k->ch >= '1' && k->ch <= '9') {
            long t = group_digit(groups, group_count, nav.cur, k->ch - '0');
            if (t >= 0)
                next = (size_t)t;
        }
        break;
    default: break;
    }
    if (next != nav.cur) {
        go_to(next);
        return;
    }
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
}

static void handle_pointer(const struct pointer_event *e) {
    struct page *p = &pages[nav.cur];
    uint8_t prev_buttons = last_buttons;
    last_buttons = e->buttons;
    if (p->pointer && p->pointer(p, e)) {
        draw_all();
        return;
    }
    if (pointer_move(&ptr, e->dx, e->dy, cols, rows))
        gfx_set_cursor(ptr.x, ptr.y);
    if (e->wheel) {
        page_scroll(&pages[nav.cur], pointer_wheel_lines(e->wheel), visible());
        draw_content();
    }
    bool click = (e->buttons & 1) && !(prev_buttons & 1);
    if (click && ptr.y == 1) {
        int gi = label_at(group_label_ptrs, group_count, ptr.x);
        if (gi >= 0)
            go_to(group_key(groups, group_count, last_page, nav.cur, (size_t)gi));
    } else if (click && ptr.y == 2) {
        int si = label_at(sub_label_ptrs, sub_count, ptr.x);
        int gi = group_of(groups, group_count, nav.cur);
        if (si >= 0 && gi >= 0)
            go_to(groups[gi].first + (size_t)si);
    }
}

// Lets live pages rebuild themselves; true when the current page was redrawn.
static bool update_pages(void) {
    bool redraw = false;
    size_t vis = visible();
    for (size_t i = 0; i < page_count; i++) {
        struct page *p = &pages[i];
        if (!p->update)
            continue;
        size_t scroll = p->scroll;
        bool end = p->follow && page_at_end(p, vis);
        if (!p->update(p))
            continue;
        p->scroll = scroll;
        if (end)
            page_scroll_to(p, true, vis);
        else
            page_scroll(p, 0, vis);
        if (i == nav.cur) {
            draw_content();
            draw_footer();  // e.g. the key tester stopping changes the footer
            redraw = true;
        }
    }
    return redraw;
}

void ui_run(struct page *pg, size_t count, const struct ui_group *grp, size_t ngroups) {
    pages = pg;
    page_count = count < UI_MAX_PAGES ? count : UI_MAX_PAGES;
    groups = grp;
    group_count = ngroups < UI_MAX_GROUPS ? ngroups : UI_MAX_GROUPS;
    cols = gfx_cols();
    rows = gfx_rows();
    for (size_t i = 0; i < group_count; i++) {
        ksnprintf(group_labels[i], sizeof group_labels[i], " F%zu %s ", i + 1, groups[i].title);
        group_label_ptrs[i] = group_labels[i];
        last_page[i] = groups[i].first;
    }
    for (size_t i = 0; i < page_count; i++)
        if (pg[i].follow)
            page_scroll_to(&pg[i], true, visible());
    pointer_init(&ptr, cols, rows);

    draw_frame();
    draw_tabs();
    draw_clock();
    draw_content();
    draw_footer();
    gfx_flush();
    klog("UI READY");
#if defined(SMOKE_SHUTDOWN)
    power_shutdown();
#elif defined(SMOKE) && !defined(SMOKE_KEYS)
    qemu_debug_exit(0x10);
#endif

    uint64_t hz = tsc_hz(), last = rdtsc();
    unsigned long spins = 0;
    for (;;) {
        bool dirty = false;
        struct key_event k;
        while (input_poll(&k)) {
            if (!pages[nav.cur].raw)
                handle(&k);
            dirty = true;
        }
        struct raw_key rk;
        while (input_poll_raw(&rk)) {
            struct page *p = &pages[nav.cur];
            if (p->raw && p->raw(p, &rk)) {
                if (p->raw)
                    draw_content();
                else
                    draw_all();  // the page gave the keyboard (and screen) back
                dirty = true;
            }
        }
        struct pointer_event pe;
        while (input_poll_pointer(&pe)) {
            handle_pointer(&pe);
            dirty = true;
        }
        if (update_pages())
            dirty = true;
        if (pages[nav.cur].leave) {
            pages[nav.cur].leave = false;
            go_to(nav.prev);
            dirty = true;
        }
        if (input_keyboard_changed()) {
            klog("ui: keyboard %s", input_have_keyboard() ? "found" : "missing");
            draw_footer();
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
