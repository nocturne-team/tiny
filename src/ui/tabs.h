#pragma once
#include <stddef.h>
#include "limine.h"
#include "ui/page.h"

struct cpu_info;
struct fw_info;
struct pci_list;
struct smbios_info;

// `width` is the usable content width in columns (used to wrap feature flags).
void tab_cpu_build(struct page *p, const struct cpu_info *c, size_t width);
// `smb` may be NULL; its type 17 entries become the "Installed modules" section.
void tab_mem_build(struct page *p, const struct limine_memmap_response *map, const struct smbios_info *smb);
void tab_pci_build(struct page *p, const struct pci_list *l);
void tab_fw_build(struct page *p, const struct fw_info *f);
void tab_about_build(struct page *p);
struct logbuf;
struct storage_list;
struct sensors;
void tab_sensors_build(struct page *p, const struct sensors *s);
void tab_storage_build(struct page *p, const struct storage_list *s);
void tab_log_build(struct page *p, const struct logbuf *lb);
struct usb_summary;
void tab_usb_build(struct page *p, const struct usb_summary *s);
