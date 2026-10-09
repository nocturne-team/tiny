#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "info/sensors_calc.h"

struct cpu_info;

enum sensor_kind { SENS_NONE, SENS_INTEL, SENS_AMD_SMN, SENS_AMD_K10 };

// Temperatures and clock of the bootstrap CPU. Temperatures in millidegrees C.
struct sensors {
    enum sensor_kind kind;
    bool has_core, has_pkg, has_clock;  // what this CPU offers
    unsigned tjmax;                     // Intel
    bool tjmax_assumed;
    uint64_t tsc_hz;

    bool core_ok, pkg_ok;               // latest reading valid
    int core_mc, pkg_mc;
    bool have_range;                    // min/max of the main temperature
    int min_mc, max_mc;
    uint64_t khz, max_khz;              // effective clock; 0 until two samples
    struct hist temp, clock;            // °C and MHz, one sample a second

    uint64_t aperf, mperf;              // previous counter values
    bool primed;
};

// Hardware: works out which sensors this CPU has. Logs "sensors: ...".
void sensors_init(struct sensors *s, const struct cpu_info *c);
// Hardware: reads everything once and appends to the histories.
void sensors_sample(struct sensors *s);
// Pure: the temperature the graph and min/max follow (package, else core).
bool sensors_main_temp(const struct sensors *s, int *mc);
