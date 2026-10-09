# tiny

A tiny bare-metal x86_64 "operating system" that boots, probes the machine, and
shows what it found in a keyboard-driven text UI. It covers the CPU (CPUID,
caches, features, TSC), the memory map and installed RAM, PCI devices, the
monitor (EDID), and the firmware (BIOS/UEFI, SMBIOS,
ACPI tables, RTC). It boots from one ISO on BIOS and UEFI machines via
[Limine](https://github.com/Limine-Bootloader/Limine).

## Keys

| Key | Action |
|---|---|
| ← / → or F1–F6 | switch tab (CPU, Memory, PCI, Firmware, Log, About) |
| ↑ / ↓, PgUp / PgDn, Home / End | scroll |
| R, then R again | reboot |
| S, then S again | shut down (ACPI S5) |

The **Log** tab shows everything tiny logged (the same lines as the serial port,
with timestamps), so on a machine without a serial port you can still see why a
controller or device didn't come up. Problem lines are shown in red, and the
view follows new lines while it is scrolled to the bottom.

A USB mouse works too: the wheel scrolls, and clicking a tab label switches to
it. The pointer is a highlighted cell that appears when the mouse moves.

Input works from a PS/2 keyboard or a USB keyboard on an xHCI controller
(root port or behind one hub; hotplug works). On machines whose firmware has no
PS/2 emulation, plug the keyboard in and go. If the USB driver misbehaves on
some machine, pick **tiny (no USB)** in the boot menu: tiny then leaves the USB
controllers alone and relies on the firmware's emulation.

## Building

Requirements: `clang`, `ld.lld`, `nasm`, `xorriso`, `git`, `make`, `gcc` (host
tests), `qemu-system-x86_64` and OVMF (`/usr/share/edk2/OvmfX64`), and Python 3
(tools only). On Gentoo: `emerge dev-libs/libisoburn sys-firmware/edk2-bin`.

```sh
make            # build/tiny.iso (fetches Limine v11.4.1 on first run)
make run        # QEMU, legacy BIOS
make run-uefi   # QEMU, UEFI (OVMF)
make run-usb    # QEMU, PS/2 off, USB keyboard on an xHCI
make test       # host unit tests (gcc + ASan/UBSan)
make smoke      # headless QEMU boots (BIOS, UEFI, panic, no-PS/2, no-ACPI, shutdown, nousb)
                # plus USB key tests driven over QMP (root port, hub, hotplug)
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
- `src/lib/`: formatting, serial + log buffer, command line
- `src/gfx/`: framebuffer text grid
- `src/input/`: PS/2 backend and the input front
- `src/usb/`: xHCI driver, device enumeration, hubs, HID boot keyboard
- `src/mm/`: DMA page pool and MMIO mapping
- `src/arch/`: x86 helpers and exception handling
- `tests/`: unit tests and smoke tests

Pure modules (formatting, decoding, parsing, paging) are unit-tested on the host.

## Credits

- Limine bootloader (BSD-2-Clause); `src/limine.h` is 0BSD.
- The IBM VGA 8x16 font is extracted from QEMU's SeaBIOS VGA BIOS by
  `tools/extract-font.py`.
