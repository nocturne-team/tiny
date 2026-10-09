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
2. **Storage page** (done). NVMe and AHCI/SATA drives: model, serial, firmware,
   capacity, and the drive's SMART health (NVMe health log; ATA SMART status and
   key attributes). Read-only: tiny never writes to a disk.
3. **Temperatures and clocks** (done, Sensors tab). Live CPU package/core temperature and effective
   clock (Intel thermal MSRs + APERF/MPERF; AMD equivalents), refreshed about once
   a second, with a small braille history graph.
4. **USB page** (done, F4). Every xHCI controller (version, USB 2/3 port counts)
   and every device on its ports, used or not: port, speed, vid:pid, class,
   product strings, status. Live across hotplug.
5. **Clean drive shutdown.** ATA STANDBY IMMEDIATE before power-off, so SATA drives
   don't count an unsafe power loss.
6. **Key tester.** A page that lights up keys as they're pressed and shows the raw
   scancode / HID usage.

## Ideas (not scheduled)

- **Network:** MAC addresses of common Intel and Realtek NICs.
- **PCI device names:** a trimmed `pci.ids` subset instead of bare IDs.
- **Bring up all cores (SMP):** per-core info, hybrid P/E-core topology; groundwork
  for the memory test.
- **Memory test:** pattern write/verify over usable RAM with progress and an
  error list.
- **Report export:** a QR code of the full report to scan with a phone, or a text
  file written to the boot stick (needs FAT writing).
