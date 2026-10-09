#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_LINE_LEN 120   // bytes per line, including the NUL
#define PAGE_MAX_LINES 300
#define PAGE_KEY_WIDTH 16   // page_kv key column: 15 chars + 1 space

// STYLE_ART lines hold one byte: the 1-based row of the About art to draw.
// STYLE_GRAPH lines hold a count byte, then that many braille patterns.
enum line_style { STYLE_TEXT, STYLE_HEAD, STYLE_DIM, STYLE_WARN, STYLE_ART, STYLE_GRAPH };

struct raw_key;
struct key_event;
struct pointer_event;

struct line {
    uint8_t style;
    char text[PAGE_LINE_LEN];
};

struct page {
    const char *title;
    size_t count;
    size_t scroll;    // index of the first visible line
    bool truncated;   // lines were dropped; the last line says so
    // Optional, for live pages: rebuilds the page when its data changed and
    // returns true then. The UI keeps the scroll position across rebuilds.
    bool (*update)(struct page *p);
    bool follow;      // opens at the end and stays there while scrolled to the end
    // Optional, for pages that take over the keyboard (the key tester):
    // while current, every raw key event goes here and normal key handling is off.
    // Returns true when the page needs redrawing.
    bool (*raw)(struct page *p, const struct raw_key *ev);
    // Optional: draws the content area itself instead of the lines.
    void (*draw)(const struct page *p, int x, int y, int w, int h);
    bool leave;       // set by the page: the UI goes back to the previous tab
    // Optional: called each time the page becomes the current tab.
    void (*enter)(struct page *p);
    // Optional: sees each key and pointer event before the UI. Returning true means
    // the page used it; the UI then only redraws. A page may set or clear `raw` here.
    bool (*key)(struct page *p, const struct key_event *k);
    bool (*pointer)(struct page *p, const struct pointer_event *e);
    struct line lines[PAGE_MAX_LINES];
};

void page_clear(struct page *p);
void page_blank(struct page *p);
void page_add(struct page *p, enum line_style style, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
// "Key            value": key cut to 15 chars, value starts at PAGE_KEY_WIDTH.
void page_kv(struct page *p, const char *key, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
// Lays words out in the value column, wrapping at `width` total columns.
void page_wrap_words(struct page *p, const char *key, const char *const *words, size_t n, size_t width);
// One line of braille cells (at most PAGE_LINE_LEN - 1).
void page_graph(struct page *p, const uint8_t *cells, size_t n);
void page_scroll(struct page *p, long delta, size_t visible);
void page_scroll_to(struct page *p, bool end, size_t visible);
// True when the last line is on screen.
bool page_at_end(const struct page *p, size_t visible);


// Which tab is shown, the one before it, and a pending R/S confirmation ('r', 's' or 0).
struct ui_nav {
    size_t cur, prev;
    char pending;
};

// Switches to tab t: remembers the previous tab, cancels a pending confirmation (it
// belongs to the tab it was started on), drops a stale leave request and calls the
// page's enter hook. False, and nothing changes, when t is already current.
bool nav_switch(struct ui_nav *n, struct page *pages, size_t t);
