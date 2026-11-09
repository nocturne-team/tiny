# tiny milestones

What is done and what might come next. Each milestone is delivered on its own
so it can be tried before the next one starts.

## Done

- **System info:** CPU, Memory (map and installed DIMMs), PCI, Firmware
  (BIOS/UEFI, SMBIOS, ACPI, RTC, monitor EDID), About. BIOS and UEFI from one ISO.
- **USB keyboard:** xHCI, root ports and one hub level, hotplug, typematic
  repeat, `nousb` boot entry.
- **USB mouse:** boot-protocol mice and keyboard+mouse receivers, pointer cell,
  wheel scroll, click a label to switch page.
- **Log page:** everything `klog` printed, with timestamps, so real hardware
  without a serial port can still show why something didn't come up.
- **Storage page:** NVMe and AHCI/SATA drives with model, serial, firmware,
  capacity and SMART health. Read-only: tiny never touches disk data.
- **Clean drive shutdown:** NVMe shutdown and ATA STANDBY IMMEDIATE right after
  the probe, so drives don't count an unsafe power loss however the machine is
  turned off.
- **Sensors page:** live CPU temperature and effective clock (Intel thermal
  MSRs, AMD Tctl, APERF/MPERF), refreshed once a second, with a braille history
  graph.
- **USB page:** every xHCI controller and every device on its ports, used or
  not, live across hotplug.
- **Keys page:** keyboard tester with held/pressed keys and raw scancodes or
  HID usages.
- **Screen page:** framebuffer mode vs. EDID preferred mode, and full-screen
  test patterns.
- **PCI device names:** every vendor and device from `pci.ids` (about 20k
  devices, ~0.8 MB), generated into `src/info/pci_ids.c`.
- **Microcode and mitigations:** Security section on the CPU page.
- **Page groups:** F1 to F5 pick System, Devices, Live, Tests and About; a
  second row holds each group's pages.

## Next

- **DIMM SPD over SMBus:** a DIMMs page under System with part numbers and
  JEDEC/XMP/EXPO timings.
- **HD Audio codecs:** codec IDs and which jacks are plugged in.
- **TPM:** presence (ACPI TPM2), version, manufacturer.
- **Network:** MAC addresses of common Intel and Realtek NICs.

## Ideas (not scheduled)

- **Bring up all cores (SMP):** per-core info, hybrid P/E-core topology;
  groundwork for the memory test.
- **Memory test:** pattern write/verify over usable RAM with progress and an
  error list.
- **Report export:** a QR code of the full report to scan with a phone, or a
  text file written to the boot stick (needs FAT writing).
