#include "lib/cmdline.h"
#include "lib/string.h"

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

bool cmdline_has(const char *s, const char *word) {
    size_t n = strlen(word);
    if (!s || !n)
        return false;
    while (*s) {
        while (is_space(*s))
            s++;
        size_t len = 0;
        while (s[len] && !is_space(s[len]))
            len++;
        if (len == n && memcmp(s, word, n) == 0)
            return true;
        s += len;
    }
    return false;
}
