#pragma once
#include <stdio.h>
#include <string.h>

static int t_run, t_fail;

#define CHECK(cond) do { \
    t_run++; \
    if (!(cond)) { t_fail++; printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    t_run++; \
    if (_a != _b) { t_fail++; printf("%s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #a, _a, _b); } \
} while (0)

#define CHECK_STR(a, b) do { \
    const char *_a = (a), *_b = (b); \
    t_run++; \
    if (strcmp(_a, _b) != 0) { t_fail++; printf("%s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #a, _a, _b); } \
} while (0)

#define TEST_MAIN_END() do { \
    printf("%s: %d checks, %d failed\n", __FILE__, t_run, t_fail); \
    return t_fail != 0; \
} while (0)
