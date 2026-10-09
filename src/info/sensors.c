#include "info/sensors.h"
#include "arch/x86.h"
#include "info/cpu.h"
#include "info/pci.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"

#define MSR_APERF 0xE8
#define MSR_MPERF 0xE7
#define MSR_THERM_STATUS 0x19C
#define MSR_TEMPERATURE_TARGET 0x1A2
#define MSR_PKG_THERM_STATUS 0x1B1
#define SMN_THM_TCON_CUR_TMP 0x00059800u

static bool amd_root_ok(void) {
    uint16_t vendor = (uint16_t)pci_config_read(0, 0, 0, 0);
    return vendor == 0x1022 || vendor == 0x1D94;
}

static uint32_t smn_read(uint32_t addr) {
    pci_config_write(0, 0, 0, 0x60, addr);
    return pci_config_read(0, 0, 0, 0x64);
}

static bool k10_ok(void) {
    uint32_t id = pci_config_read(0, 0x18, 3, 0);
    return (id & 0xFFFF) == 0x1022;
}

void sensors_init(struct sensors *s, const struct cpu_info *c) {
    memset(s, 0, sizeof *s);
    s->tsc_hz = c->tsc_hz;
#ifdef SMOKE
    uint64_t junk;  // TCG reads unknown MSRs as 0; KVM raises #GP
    klog("msr: invalid MSR %s", rdmsr_safe(0x4FFFFFFF, &junk) ? "read as 0x0" : "faulted, recovered");
#endif
    struct cpuid_regs l6 = c->max_leaf >= 6 ? cpuid(6, 0) : (struct cpuid_regs){ 0 };
    uint64_t v;
    bool intel = !memcmp(c->vendor, "GenuineIntel", 12);
    bool amd = !memcmp(c->vendor, "AuthenticAMD", 12) || !memcmp(c->vendor, "HygonGenuine", 12);

    if (intel && (l6.eax & 1) && rdmsr_safe(MSR_THERM_STATUS, &v)) {
        s->kind = SENS_INTEL;
        s->has_core = true;
        s->has_pkg = (l6.eax & (1u << 6)) && rdmsr_safe(MSR_PKG_THERM_STATUS, &v);
        uint64_t target = 0;
        s->tjmax = intel_tjmax(rdmsr_safe(MSR_TEMPERATURE_TARGET, &target) ? target : 0,
                               &s->tjmax_assumed);
    } else if (amd && c->sig.family >= 0x17 && amd_root_ok()) {
        s->kind = SENS_AMD_SMN;
        s->has_pkg = true;
    } else if (amd && c->sig.family >= 0x10 && c->sig.family <= 0x16 && k10_ok()) {
        s->kind = SENS_AMD_K10;
        s->has_pkg = true;
    }
    s->has_clock = (l6.ecx & 1) && s->tsc_hz && rdmsr_safe(MSR_APERF, &v) && rdmsr_safe(MSR_MPERF, &v);

#ifdef FAKE_SENSORS  // screenshots: plausible Intel readings with no hardware behind them
    s->kind = SENS_INTEL;
    s->has_core = s->has_pkg = s->has_clock = true;
    s->tjmax = 100;
    if (!s->tsc_hz)
        s->tsc_hz = 3600000000ull;
#endif
    char buf[96];
    switch (s->kind) {
    case SENS_INTEL:
        ksnprintf(buf, sizeof buf, "intel core%s, tjmax %u%s", s->has_pkg ? "+package" : "", s->tjmax,
                  s->tjmax_assumed ? " (assumed)" : "");
        break;
    case SENS_AMD_SMN: ksnprintf(buf, sizeof buf, "amd tctl (smn)"); break;
    case SENS_AMD_K10: ksnprintf(buf, sizeof buf, "amd tctl (d18f3)"); break;
    default: ksnprintf(buf, sizeof buf, "no temperature"); break;
    }
    klog("sensors: %s%s", buf, s->has_clock ? ", aperf/mperf" : "");
}

bool sensors_main_temp(const struct sensors *s, int *mc) {
    if (s->pkg_ok)
        *mc = s->pkg_mc;
    else if (s->core_ok)
        *mc = s->core_mc;
    else
        return false;
    return true;
}

#ifdef FAKE_SENSORS
static uint64_t fake_n;

static bool fake_msr(uint32_t msr, uint64_t *v) {
    static const uint8_t wave[16] = { 48, 47, 45, 42, 40, 38, 36, 35, 34, 35, 37, 40, 43, 46, 52, 55 };
    unsigned t = (unsigned)(fake_n % 16);
    switch (msr) {
    case MSR_THERM_STATUS: *v = 0x80000000ull | (uint64_t)(wave[t] + 3) << 16; return true;
    case MSR_PKG_THERM_STATUS: *v = 0x80000000ull | (uint64_t)wave[t] << 16; return true;
    case MSR_APERF: {
        static uint64_t aperf;
        aperf += 3600000000ull + (fake_n % 5) * 300000000ull;  // read once per sample
        *v = aperf;
        return true;
    }
    case MSR_MPERF: *v = fake_n * 3600000000ull; return true;
    default: return false;
    }
}
#define rdmsr_safe fake_msr
#endif

void sensors_sample(struct sensors *s) {
    uint64_t v;
    int c;
#ifdef FAKE_SENSORS
    fake_n++;
#endif
    s->core_ok = s->pkg_ok = false;
    switch (s->kind) {
    case SENS_INTEL:
        if (rdmsr_safe(MSR_THERM_STATUS, &v) && intel_temp(v, s->tjmax, &c)) {
            s->core_ok = true;
            s->core_mc = c * 1000;
        }
        if (s->has_pkg && rdmsr_safe(MSR_PKG_THERM_STATUS, &v) && intel_temp(v, s->tjmax, &c)) {
            s->pkg_ok = true;
            s->pkg_mc = c * 1000;
        }
        break;
    case SENS_AMD_SMN:
        s->pkg_mc = amd_tctl_mc(smn_read(SMN_THM_TCON_CUR_TMP));
        s->pkg_ok = true;
        break;
    case SENS_AMD_K10:
        s->pkg_mc = amd_k10_mc(pci_config_read(0, 0x18, 3, 0xA4));
        s->pkg_ok = true;
        break;
    default: break;
    }
    int mc;
    if (sensors_main_temp(s, &mc)) {
        if (!s->have_range || mc < s->min_mc)
            s->min_mc = mc;
        if (!s->have_range || mc > s->max_mc)
            s->max_mc = mc;
        s->have_range = true;
        hist_push(&s->temp, (int16_t)(mc / 1000));
    }

    uint64_t a, m;
    if (s->has_clock && rdmsr_safe(MSR_APERF, &a) && rdmsr_safe(MSR_MPERF, &m)) {
        // A counter going backwards (reset, or a hypervisor) gives no reading.
        uint64_t khz = s->primed && a >= s->aperf && m >= s->mperf
            ? effective_khz(a - s->aperf, m - s->mperf, s->tsc_hz) : 0;
        if (khz) {
            s->khz = khz;
            if (khz > s->max_khz)
                s->max_khz = khz;
            hist_push(&s->clock, (int16_t)(khz / 1000 > 32767 ? 32767 : khz / 1000));
        }
        s->aperf = a;
        s->mperf = m;
        s->primed = true;
    }
}
