#!/usr/bin/env python3
"""Convert pci.ids into the vendor/device name table for tiny.

usage: tools/pciids2c.py [/usr/share/hwdata/pci.ids]   (writes src/info/pci_ids.c)

Keeps vendor and device lines; drops subsystems and the class list. Names are
folded to ASCII (the UI font is CP437 without accents).
"""
import re
import sys
import unicodedata

src = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/hwdata/pci.ids'
out = 'src/info/pci_ids.c'

version = '?'
vendors = {}  # id -> (name, {device id -> name})
cur = None
for line in open(src, encoding='utf-8', errors='replace'):
    line = line.rstrip('\n')
    m = re.match(r'#\s*Version:\s*(\S+)', line)
    if m:
        version = m.group(1)
    if line.startswith('C '):
        break  # the class list follows the vendors
    if not line or line.startswith('#') or line.startswith('\t\t'):
        continue
    m = re.match(r'(\t?)([0-9a-fA-F]{4})  (.+)$', line)
    if not m:
        sys.exit('%s: cannot parse %r' % (src, line))
    ident, name = int(m.group(2), 16), m.group(3).strip()
    if not m.group(1):
        cur = vendors.setdefault(ident, (name, {}))
    elif cur is not None:
        cur[1].setdefault(ident, name)


def ascii_name(s):
    s = s.replace('²', '2').replace('³', '3')
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode()
    return s


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('?', '\\?') + '\\0"'


strings, offsets, size = [], {}, 0


def intern(name):
    global size
    name = ascii_name(name)
    if name not in offsets:
        offsets[name] = size
        strings.append(name)
        size += len(name) + 1
    return offsets[name]


vrows, drows = [], []
for vid in sorted(vendors):
    name, devs = vendors[vid]
    vrows.append((vid, len(devs), intern(name), len(drows)))
    for did in sorted(devs):
        drows.append((did, intern(devs[did])))

with open(out, 'w') as f:
    f.write('/* Generated from pci.ids %s by tools/pciids2c.py. Do not edit.\n' % version)
    f.write(' * pci.ids: https://pci-ids.ucw.cz/, used under the 3-clause BSD license\n')
    f.write(' * (it is dual-licensed GPL-2.0-or-later / BSD-3-Clause). */\n')
    f.write('#include "info/pci_db.h"\n\n')
    f.write('static const char strings[] =\n')
    for s in strings:
        f.write('    %s\n' % c_string(s))
    f.write('    ;\n\n')
    f.write('static const struct pci_db_vendor vendors[] = {\n')
    for v in vrows:
        f.write('    { 0x%04X, %d, %d, %d },\n' % v)
    f.write('};\n\n')
    f.write('static const struct pci_db_device devices[] = {\n')
    for d in drows:
        f.write('    { 0x%04X, %d },\n' % d)
    f.write('};\n\n')
    f.write('const struct pci_db pci_ids = {\n')
    f.write('    strings, vendors, sizeof vendors / sizeof vendors[0],\n')
    f.write('    devices, sizeof devices / sizeof devices[0], "%s",\n' % version)
    f.write('};\n')
print('%s: %d vendors, %d devices, %d bytes of names (pci.ids %s)'
      % (out, len(vrows), len(drows), size, version))
