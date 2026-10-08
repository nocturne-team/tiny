#pragma once
#include <stdbool.h>

// True when `word` is one of the space/tab separated words of `cmdline` (NULL = empty).
bool cmdline_has(const char *cmdline, const char *word);
