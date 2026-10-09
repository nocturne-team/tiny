# tiny — milestones

What is done, what is next, and the ideas after that. Each milestone gets a short
design in `docs/superpowers/specs/` when work starts, and is delivered on its
own so it can be tried before the next one.

## Done

- **Sysinfo core:** CPU, Memory (map + installed DIMMs), PCI, Firmware (BIOS/UEFI,
  SMBIOS, ACPI, RTC, monitor EDID), About. BIOS and UEFI from one ISO.
- **USB keyboard:** xHCI, root ports + one hub level, hotplug, typematic repeat,
  `nousb` boot entry.
- **USB mouse:** boot-protocol mice and keyboard+mouse receivers, pointer cell,
  wheel scroll, click a tab.

## Next (in order)

1. **Log page** (done). The kernel log (everything `klog` printed, with timestamps) on
   its own tab, so real hardware without a serial port can still show why
   something didn't come up. Follows new lines while scrolled to the bottom.
2. **Storage page.** NVMe and AHCI/SATA drives: model, serial, firmware,
   capacity, and the drive's SMART health (NVMe health log; ATA SMART status and
   key attributes). Read-only: tiny never writes to a disk.
3. **Temperatures and clocks.** Live CPU package/core temperature and effective
   clock (Intel thermal MSRs + APERF/MPERF; AMD equivalents), refreshed about once
   a second, with a small braille history graph.

## Ideas (not scheduled)

- **USB page:** every enumerated USB device (port, speed, vid:pid, class, hub
  tree) and each controller's version and port count.
- **Key tester:** a page that lights up keys as they're pressed and shows the raw
  scancode / HID usage.
- **Network:** MAC addresses of common Intel and Realtek NICs.
- **PCI device names:** a trimmed `pci.ids` subset instead of bare IDs.
- **Bring up all cores (SMP):** per-core info, hybrid P/E-core topology; groundwork
  for the memory test.
- **Memory test:** pattern write/verify over usable RAM with progress and an
  error list.
- **Report export:** a QR code of the full report to scan with a phone, or a text
  file written to the boot stick (needs FAT writing).
- **Clean drive shutdown:** ATA STANDBY IMMEDIATE before power-off, so SATA drives
  don't count an unsafe power loss.
