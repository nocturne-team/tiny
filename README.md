# tiny

A tiny bare-metal x86_64 "operating system" that boots, probes the machine, and
shows what it found in a keyboard-driven text UI. It covers the CPU (CPUID,
caches, features, TSC), the memory map and installed RAM, PCI and USB devices, drives
(NVMe and SATA, with SMART health), the monitor (EDID), and the firmware (BIOS/UEFI, SMBIOS,
ACPI tables, RTC). It boots from one ISO on BIOS and UEFI machines via
[Limine](https://github.com/Limine-Bootloader/Limine).

## Keys

| Key | Action |
|---|---|
| ← / → or F1–F9 | switch tab (CPU, Memory, PCI, USB, Firmware, Storage, Sensors, Log, About) |
| ↑ / ↓, PgUp / PgDn, Home / End | scroll |
| R, then R again | reboot |
| S, then S again | shut down (ACPI S5) |

The **USB** tab lists each xHCI controller (version, USB 2 and USB 3 port
counts) and every device that answered on its ports: port, speed, vid:pid,
class, the product name the device reports, and whether tiny uses it (keyboard,
mouse, hub) or not. Devices that failed to come up are listed too, in red. The
list follows plugging and unplugging. Devices behind a hub's USB 3 half or a
second level of hubs are not seen.

The **Storage** tab lists NVMe drives and SATA drives on AHCI controllers:
model, serial, firmware, capacity, and health from the drive's own SMART data
(NVMe health log; ATA SMART status plus reallocated/pending/uncorrectable
sectors, temperature and power-on hours). tiny only sends identify and
health-log commands, never reads or writes the disks' data. Once they are read,
NVMe drives get a normal shutdown and SATA drives a STANDBY IMMEDIATE (hard
disks spin down a few seconds into boot), so however the machine is turned off
afterwards it doesn't add to their unsafe-shutdown count. Drives behind RAID or Intel VMD controllers
can't be seen; switch the firmware's storage mode to AHCI.

The **Sensors** tab shows the CPU temperature (Intel core and package sensors
with TjMax; AMD Tctl) and the effective clock from APERF/MPERF, refreshed every
second with a two-minute graph of each. Only the boot CPU is read, and tiny
keeps it busy, so these are the readings of one loaded core. Virtual machines
usually have no readable sensors.

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
**tiny (safe mode: no USB, no storage)** additionally skips the NVMe/SATA probe,
for a machine where a storage controller misbehaves.

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
make smoke      # headless QEMU boots (BIOS, UEFI, panic, no-PS/2, no-ACPI, shutdown, nousb,
                # NVMe + SATA disks)
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
- `src/info/`: hardware probing and table parsing, NVMe and AHCI identify/SMART
- `src/ui/`: pages, tabs and the main loop
- `src/lib/`: formatting, serial + log buffer, command line
- `src/gfx/`: framebuffer text grid
- `src/input/`: PS/2 backend and the input front
- `src/usb/`: xHCI driver, device enumeration, hubs, HID boot keyboard
- `src/mm/`: DMA page pool, MMIO mapping and BAR helpers
- `src/arch/`: x86 helpers and exception handling
- `tests/`: unit tests and smoke tests

Pure modules (formatting, decoding, parsing, paging) are unit-tested on the host.

## Credits

- Limine bootloader (BSD-2-Clause); `src/limine.h` is 0BSD.
- The IBM VGA 8x16 font is extracted from QEMU's SeaBIOS VGA BIOS by
  `tools/extract-font.py`.
