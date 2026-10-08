# tiny

A tiny bare-metal x86_64 "operating system" that boots, probes the machine, and
shows what it found in a keyboard-driven text UI. It covers the CPU (CPUID,
caches, features, TSC), the memory map, and the firmware (BIOS/UEFI, SMBIOS,
ACPI tables, RTC). It boots from one ISO on BIOS and UEFI machines via
[Limine](https://github.com/Limine-Bootloader/Limine).

## Keys

| Key | Action |
|---|---|
| ← / → or F1–F3 | switch tab (CPU, Memory, Firmware) |
| ↑ / ↓, PgUp / PgDn, Home / End | scroll |
| R, then R again | reboot |
| S, then S again | shut down (ACPI S5) |

Input is PS/2 only. Most PCs emulate PS/2 for USB keyboards, but some
UEFI-only machines don't. On those the info is still shown and the footer
says "No PS/2 keyboard".

## Building

Requirements: `clang`, `ld.lld`, `nasm`, `xorriso`, `git`, `make`, `gcc` (host
tests), `qemu-system-x86_64` and OVMF (`/usr/share/edk2/OvmfX64`), and Python 3
(tools only). On Gentoo: `emerge dev-libs/libisoburn sys-firmware/edk2-bin`.

```sh
make            # build/tiny.iso (fetches Limine v11.4.1 on first run)
make run        # QEMU, legacy BIOS
make run-uefi   # QEMU, UEFI (OVMF)
make test       # host unit tests (gcc + ASan/UBSan)
make smoke      # headless QEMU boots: BIOS, UEFI, panic, no-PS/2, no-ACPI, shutdown
make shots      # screenshots of every tab into build/shots/
```

## Real hardware

Write the ISO to a USB stick. **This erases the stick**: double-check the
device name with `lsblk` first.

```sh
sudo dd if=build/tiny.iso of=/dev/sdX bs=4M conv=fsync status=progress
```

Boot from it in BIOS/CSM or UEFI mode. Secure Boot must be off.

## Layout

- `src/boot.c`: Limine requests
- `src/info/`: hardware probing and table parsing
- `src/ui/`: pages, tabs and the main loop
- `src/gfx/`: framebuffer text grid
- `src/input/`: PS/2 keyboard
- `src/arch/`: x86 helpers and exception handling
- `tests/`: unit tests and smoke tests

Pure modules (formatting, decoding, parsing, paging) are unit-tested on the host.

## Credits

- Limine bootloader (BSD-2-Clause); `src/limine.h` is 0BSD.
- The IBM VGA 8x16 font is extracted from QEMU's SeaBIOS VGA BIOS by
  `tools/extract-font.py`.
