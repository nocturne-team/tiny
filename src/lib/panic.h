#pragma once

struct int_frame;

// Red panic screen + serial dump, then halt. Exits QEMU with 0x11 in SMOKE builds.
__attribute__((noreturn, format(printf, 1, 2))) void panic(const char *fmt, ...);
__attribute__((noreturn)) void panic_exception(const struct int_frame *f);
