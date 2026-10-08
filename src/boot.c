#include "boot.h"
#include "lib/serial.h"

#define REQUEST __attribute__((used, section(".limine_requests")))

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

REQUEST static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

REQUEST static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_rsdp_request rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_smbios_request smbios_req = {
    .id = LIMINE_SMBIOS_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_mp_request mp_req = {
    .id = LIMINE_MP_REQUEST_ID, .revision = 0, .flags = 0 };
REQUEST static volatile struct limine_firmware_type_request fwtype_req = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_bootloader_info_request blinfo_req = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_date_at_boot_request date_req = {
    .id = LIMINE_DATE_AT_BOOT_REQUEST_ID, .revision = 0 };
REQUEST static volatile struct limine_tsc_frequency_request tsc_req = {
    .id = LIMINE_TSC_FREQUENCY_REQUEST_ID, .revision = 0 };

REQUEST static volatile struct limine_executable_cmdline_request cmdline_req = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0 };

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

struct bootinfo bootinfo;

bool boot_init(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        serial_write("tiny: bootloader does not support Limine base revision 6\n");
        return false;
    }

    struct limine_framebuffer_response *fb = fb_req.response;
    if (fb && fb->framebuffer_count > 0)
        bootinfo.fb = fb->framebuffers[0];
    if (hhdm_req.response)
        bootinfo.hhdm = hhdm_req.response->offset;
    bootinfo.memmap = memmap_req.response;
    if (rsdp_req.response)
        bootinfo.rsdp = rsdp_req.response->address;
#ifdef SMOKE_NO_ACPI
    bootinfo.rsdp = 0;  // smoke test: behave as if the bootloader found no RSDP
#endif
    if (smbios_req.response) {
        bootinfo.smbios32 = smbios_req.response->entry_32;
        bootinfo.smbios64 = smbios_req.response->entry_64;
    }
    if (mp_req.response) {
        bootinfo.have_mp = true;
        bootinfo.cpu_count = mp_req.response->cpu_count;
        bootinfo.bsp_lapic_id = mp_req.response->bsp_lapic_id;
    }
    bootinfo.firmware = fwtype_req.response ? (int)fwtype_req.response->firmware_type : -1;
    if (blinfo_req.response) {
        bootinfo.bl_name = blinfo_req.response->name;
        bootinfo.bl_version = blinfo_req.response->version;
    }
    if (date_req.response) {
        bootinfo.have_boot_time = true;
        bootinfo.boot_unix = date_req.response->timestamp;
    }
    if (tsc_req.response)
        bootinfo.tsc_hz = tsc_req.response->frequency;
    if (cmdline_req.response)
        bootinfo.cmdline = cmdline_req.response->cmdline;
    return true;
}
