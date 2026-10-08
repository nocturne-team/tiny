#include "test.h"
#include "lib/cmdline.h"

int main(void) {
    CHECK(!cmdline_has(NULL, "nousb"));
    CHECK(!cmdline_has("", "nousb"));
    CHECK(cmdline_has("nousb", "nousb"));
    CHECK(cmdline_has("quiet nousb", "nousb"));
    CHECK(cmdline_has("  nousb  ", "nousb"));
    CHECK(cmdline_has("a\tnousb\tb", "nousb"));
    CHECK(!cmdline_has("nousbx", "nousb"));
    CHECK(!cmdline_has("xnousb", "nousb"));
    CHECK(!cmdline_has("nous", "nousb"));
    CHECK(!cmdline_has("nousb", ""));
    TEST_MAIN_END();
}
