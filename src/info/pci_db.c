#include "info/pci_db.h"

static const struct pci_db_vendor *find_vendor(const struct pci_db *db, uint16_t vendor) {
    size_t lo = 0, hi = db->nvendors;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (db->vendors[mid].id < vendor)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < db->nvendors && db->vendors[lo].id == vendor ? &db->vendors[lo] : NULL;
}

const char *pci_db_vendor(const struct pci_db *db, uint16_t vendor) {
    const struct pci_db_vendor *v = find_vendor(db, vendor);
    return v ? db->strings + v->name : NULL;
}

const char *pci_db_device(const struct pci_db *db, uint16_t vendor, uint16_t device) {
    const struct pci_db_vendor *v = find_vendor(db, vendor);
    if (!v)
        return NULL;
    size_t lo = v->first, hi = (size_t)v->first + v->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (db->devices[mid].id < device)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < (size_t)v->first + v->count && db->devices[lo].id == device)
        return db->strings + db->devices[lo].name;
    return NULL;
}
