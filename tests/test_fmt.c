#include "test.h"
#include "lib/fmt.h"
#include <stdint.h>

static char buf[128];

static void test_printf(void) {
    ksnprintf(buf, sizeof buf, "hello");
    CHECK_STR(buf, "hello");
    ksnprintf(buf, sizeof buf, "%d|%i|%u", -42, 7, 3000000000u);
    CHECK_STR(buf, "-42|7|3000000000");
    ksnprintf(buf, sizeof buf, "%x|%X|%lx", 0xbeefu, 0xbeefu, 0x123456789abcdefUL);
    CHECK_STR(buf, "beef|BEEF|123456789abcdef");
    ksnprintf(buf, sizeof buf, "[%5d][%-5d][%05d][%05d]", 42, 42, 42, -42);
    CHECK_STR(buf, "[   42][42   ][00042][-0042]");
    ksnprintf(buf, sizeof buf, "%016lx", 0xffffffff80000000UL);
    CHECK_STR(buf, "ffffffff80000000");
    ksnprintf(buf, sizeof buf, "%lu|%ld|%zu|%llu", 18446744073709551615UL,
              -9223372036854775807L - 1, (size_t)12, 5ULL);
    CHECK_STR(buf, "18446744073709551615|-9223372036854775808|12|5");
    ksnprintf(buf, sizeof buf, "[%s][%8s][%-8s][%.3s]", "abc", "abc", "abc", "abcdef");
    CHECK_STR(buf, "[abc][     abc][abc     ][abc]");
    ksnprintf(buf, sizeof buf, "[%-*.*s][%*d]", 6, 3, "abcdef", 4, 7);
    CHECK_STR(buf, "[abc   ][   7]");
    ksnprintf(buf, sizeof buf, "%c%c%%", 'o', 'k');
    CHECK_STR(buf, "ok%");
    const char *nul = NULL;
    ksnprintf(buf, sizeof buf, "%s", nul);
    CHECK_STR(buf, "(null)");
    ksnprintf(buf, sizeof buf, "%p", (void *)0x1000);
    CHECK_STR(buf, "0x0000000000001000");
    ksnprintf(buf, sizeof buf, "%d", 0);
    CHECK_STR(buf, "0");
    const char *lone = "50%";  // non-literal so gcc does not warn about the trailing '%'
    ksnprintf(buf, sizeof buf, lone);
    CHECK_STR(buf, "50");
}

static void test_truncation(void) {
    char small[6];
    CHECK_EQ(ksnprintf(small, sizeof small, "%s", "truncate me"), 11);
    CHECK_STR(small, "trunc");
    CHECK_EQ(ksnprintf(NULL, 0, "%d", 12345), 5);
}

static void test_size(void) {
    fmt_size(buf, sizeof buf, 0);
    CHECK_STR(buf, "0 B");
    fmt_size(buf, sizeof buf, 1023);
    CHECK_STR(buf, "1023 B");
    fmt_size(buf, sizeof buf, 1024);
    CHECK_STR(buf, "1.0 KiB");
    fmt_size(buf, sizeof buf, 1536);
    CHECK_STR(buf, "1.5 KiB");
    fmt_size(buf, sizeof buf, 654336);
    CHECK_STR(buf, "639.0 KiB");
    fmt_size(buf, sizeof buf, 17045651456ULL);  // 15.875 GiB, truncated
    CHECK_STR(buf, "15.8 GiB");
    fmt_size(buf, sizeof buf, 1ULL << 40);
    CHECK_STR(buf, "1.0 TiB");
    fmt_size(buf, sizeof buf, UINT64_MAX);
    CHECK_STR(buf, "16383.9 PiB");
}

static void test_printable(void) {
    CHECK_EQ(copy_printable(buf, sizeof buf, "AB\x01 C  ", 7), 5);
    CHECK_STR(buf, "AB? C");
    copy_printable(buf, sizeof buf, "BOCHS\0\0", 6);
    CHECK_STR(buf, "BOCHS");
    copy_printable(buf, 4, "ABCDEFG", 7);
    CHECK_STR(buf, "ABC");
    copy_printable(buf, sizeof buf, "\x7f\xff" "ok", 4);
    CHECK_STR(buf, "??ok");
    copy_printable(buf, sizeof buf, "    ", 4);
    CHECK_STR(buf, "");
}

int main(void) {
    test_printf();
    test_truncation();
    test_size();
    test_printable();
    TEST_MAIN_END();
}
