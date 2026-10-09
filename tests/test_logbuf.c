#include "test.h"
#include "lib/logbuf.h"

static struct logbuf lb;

static void test_empty(void) {
    memset(&lb, 0, sizeof lb);
    CHECK_EQ(logbuf_count(&lb), 0);
    CHECK_EQ(logbuf_dropped(&lb), 0);
    CHECK_EQ(lb.seq, 0);
}

static void test_add_and_split(void) {
    memset(&lb, 0, sizeof lb);
    logbuf_add(&lb, 5, "hello");
    CHECK_EQ(logbuf_count(&lb), 1);
    CHECK_STR(logbuf_get(&lb, 0)->text, "hello");
    CHECK_EQ(logbuf_get(&lb, 0)->ms, 5);

    logbuf_add(&lb, 7, "a\n\nb\n");  // trailing newline doesn't add an empty line
    CHECK_EQ(logbuf_count(&lb), 4);
    CHECK_STR(logbuf_get(&lb, 1)->text, "a");
    CHECK_STR(logbuf_get(&lb, 2)->text, "");
    CHECK_STR(logbuf_get(&lb, 3)->text, "b");
    CHECK_EQ(logbuf_get(&lb, 3)->ms, 7);
    CHECK_EQ(lb.seq, 4);

    logbuf_add(&lb, 8, "");  // an empty message is still one (empty) line
    CHECK_EQ(logbuf_count(&lb), 5);
    CHECK_STR(logbuf_get(&lb, 4)->text, "");
    CHECK(logbuf_get(&lb, 5) == NULL);
}

static void test_truncate(void) {
    memset(&lb, 0, sizeof lb);
    char longline[300];
    memset(longline, 'x', sizeof longline - 1);
    longline[sizeof longline - 1] = '\0';
    logbuf_add(&lb, 0, longline);
    CHECK_EQ(logbuf_count(&lb), 1);
    CHECK_EQ(strlen(logbuf_get(&lb, 0)->text), LOGBUF_LINE_LEN - 1);
}

static void test_control_chars(void) {
    memset(&lb, 0, sizeof lb);
    logbuf_add(&lb, 0, "a\rb\tc\x01");
    CHECK_STR(logbuf_get(&lb, 0)->text, "ab c?");  // CR dropped, tab -> space, others -> '?'
}

static void test_wrap(void) {
    memset(&lb, 0, sizeof lb);
    char buf[16];
    for (int i = 0; i < LOGBUF_LINES + 10; i++) {
        snprintf(buf, sizeof buf, "line %d", i);
        logbuf_add(&lb, (uint64_t)i, buf);
    }
    CHECK_EQ(logbuf_count(&lb), LOGBUF_LINES);
    CHECK_EQ(logbuf_dropped(&lb), 10);
    CHECK_STR(logbuf_get(&lb, 0)->text, "line 10");
    CHECK_EQ(logbuf_get(&lb, 0)->ms, 10);
    snprintf(buf, sizeof buf, "line %d", LOGBUF_LINES + 9);
    CHECK_STR(logbuf_get(&lb, LOGBUF_LINES - 1)->text, buf);
}

int main(void) {
    test_empty();
    test_add_and_split();
    test_truncate();
    test_control_chars();
    test_wrap();
    TEST_MAIN_END();
}
