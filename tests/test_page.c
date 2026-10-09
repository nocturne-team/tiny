#include "test.h"
#include "ui/page.h"

static struct page p;

static void expect_kv(const char *line, const char *key, const char *value) {
    size_t k = strlen(key);
    CHECK(strncmp(line, key, k) == 0);
    for (size_t i = k; i < PAGE_KEY_WIDTH; i++)
        CHECK(line[i] == ' ');
    CHECK_STR(line + PAGE_KEY_WIDTH, value);
}

static void test_add_and_kv(void) {
    page_clear(&p);
    page_add(&p, STYLE_HEAD, "Processor %d", 1);
    page_kv(&p, "Vendor", "%s", "GenuineIntel");
    page_blank(&p);
    page_kv(&p, "AVeryLongKeyNameIndeed", "x");
    CHECK_EQ(p.count, 4);
    CHECK_STR(p.lines[0].text, "Processor 1");
    CHECK_EQ(p.lines[0].style, STYLE_HEAD);
    expect_kv(p.lines[1].text, "Vendor", "GenuineIntel");
    CHECK_STR(p.lines[2].text, "");
    CHECK(strncmp(p.lines[3].text, "AVeryLongKeyNam ", 16) == 0);  // key cut to 15
    CHECK_STR(p.lines[3].text + 16, "x");
}

static void test_long_value(void) {
    char big[300];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    page_clear(&p);
    page_add(&p, STYLE_TEXT, "%s", big);
    page_kv(&p, "Key", "%s", big);
    CHECK_EQ(strlen(p.lines[0].text), PAGE_LINE_LEN - 1);
    CHECK_EQ(strlen(p.lines[1].text), PAGE_LINE_LEN - 1);
}

static void test_overflow(void) {
    page_clear(&p);
    for (int i = 0; i < 400; i++)
        page_add(&p, STYLE_TEXT, "line %d", i);
    CHECK_EQ(p.count, PAGE_MAX_LINES);
    CHECK(p.truncated);
    CHECK_EQ(p.lines[PAGE_MAX_LINES - 1].style, STYLE_WARN);
    CHECK(strncmp(p.lines[PAGE_MAX_LINES - 1].text, "...", 3) == 0);
    CHECK_STR(p.lines[PAGE_MAX_LINES - 2].text, "line 298");
}

static void test_scroll(void) {
    page_clear(&p);
    for (int i = 0; i < 10; i++)
        page_add(&p, STYLE_TEXT, "%d", i);
    page_scroll(&p, 100, 4);
    CHECK_EQ(p.scroll, 6);
    page_scroll(&p, -3, 4);
    CHECK_EQ(p.scroll, 3);
    page_scroll(&p, -100, 4);
    CHECK_EQ(p.scroll, 0);
    page_scroll_to(&p, true, 4);
    CHECK_EQ(p.scroll, 6);
    page_scroll_to(&p, false, 4);
    CHECK_EQ(p.scroll, 0);
    page_scroll(&p, 5, 20);  // fits on screen: never scrolls
    CHECK_EQ(p.scroll, 0);

    CHECK(!page_at_end(&p, 4));
    page_scroll(&p, 5, 4);
    CHECK(!page_at_end(&p, 4));
    page_scroll(&p, 1, 4);
    CHECK(page_at_end(&p, 4));
    page_scroll_to(&p, false, 20);
    CHECK(page_at_end(&p, 20));  // everything fits
}

static void test_wrap(void) {
    const char *words[] = { "sse", "sse2", "avx" };
    page_clear(&p);
    page_wrap_words(&p, "Features", words, 3, 26);  // 10 columns for words
    CHECK_EQ(p.count, 2);
    expect_kv(p.lines[0].text, "Features", "sse sse2");
    expect_kv(p.lines[1].text, "", "avx");

    page_clear(&p);
    page_wrap_words(&p, "Features", words, 0, 80);
    CHECK_EQ(p.count, 1);
    expect_kv(p.lines[0].text, "Features", "");

    const char *longw[] = { "averyveryverylongword" };
    page_clear(&p);
    page_wrap_words(&p, "", longw, 1, 20);  // 8 columns minimum
    expect_kv(p.lines[0].text, "", "averyver");
}

static void test_wrap_list(void) {
    const char *names[] = { "Meltdown", "L1TF", "MDS" };
    page_clear(&p);
    page_wrap_list(&p, "Not affected", names, 3, 34);  // 18 columns, comma kept on the line
    CHECK_EQ(p.count, 2);
    expect_kv(p.lines[0].text, "Not affected", "Meltdown, L1TF,");
    expect_kv(p.lines[1].text, "", "MDS");

    page_clear(&p);
    page_wrap_list(&p, "Not affected", names, 3, 80);
    CHECK_EQ(p.count, 1);
    expect_kv(p.lines[0].text, "Not affected", "Meltdown, L1TF, MDS");

    page_clear(&p);
    page_wrap_list(&p, "Not affected", names, 0, 80);
    expect_kv(p.lines[0].text, "Not affected", "");
}

static void test_graph(void) {
    page_clear(&p);
    uint8_t cells[3] = { 0x00, 0xFF, 0x40 };
    page_graph(&p, cells, 3);
    CHECK_EQ(p.count, 1);
    CHECK_EQ(p.lines[0].style, STYLE_GRAPH);
    CHECK_EQ((uint8_t)p.lines[0].text[0], 3);
    CHECK_EQ((uint8_t)p.lines[0].text[1], 0x00);  // a zero pattern is kept
    CHECK_EQ((uint8_t)p.lines[0].text[2], 0xFF);
    CHECK_EQ((uint8_t)p.lines[0].text[3], 0x40);

    uint8_t wide[200] = { 0 };
    page_graph(&p, wide, sizeof wide);  // cut to fit the line
    CHECK_EQ((uint8_t)p.lines[1].text[0], PAGE_LINE_LEN - 1);
}

static int entered;
static void count_enter(struct page *pg) {
    (void)pg;
    entered++;
}

// Tab switches from the keyboard, the mouse, or a page asking to leave.
static void test_nav(void) {
    static struct page pages[3];
    memset(pages, 0, sizeof pages);
    pages[2].enter = count_enter;
    pages[2].leave = true;  // stale: set while the page was not current
    struct ui_nav n = { .cur = 0, .prev = 0, .pending = 'r' };

    CHECK(nav_switch(&n, pages, 2));
    CHECK_EQ(n.cur, 2);
    CHECK_EQ(n.prev, 0);
    CHECK_EQ(n.pending, 0);        // a pending reboot/shutdown confirmation is cancelled
    CHECK(!pages[2].leave);        // a stale leave request is dropped on entry
    CHECK_EQ(entered, 1);

    CHECK(!nav_switch(&n, pages, 2));  // already there: nothing happens
    CHECK_EQ(entered, 1);

    CHECK(nav_switch(&n, pages, n.prev));  // back to the previous tab
    CHECK_EQ(n.cur, 0);
    CHECK_EQ(n.prev, 2);
    CHECK(nav_switch(&n, pages, 1));  // a page without an enter hook
    CHECK_EQ(entered, 1);
}

int main(void) {
    test_nav();
    test_graph();
    test_add_and_kv();
    test_long_value();
    test_overflow();
    test_scroll();
    test_wrap();
    test_wrap_list();
    TEST_MAIN_END();
}
