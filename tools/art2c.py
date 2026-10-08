#!/usr/bin/env python3
"""Convert Unicode braille art into C arrays of dot patterns for the About tab.

usage: tools/art2c.py [assets/ascii.txt]   (writes src/ui/about_art.h and .c)
"""
import sys

src = sys.argv[1] if len(sys.argv) > 1 else 'assets/ascii.txt'
lines = open(src, encoding='utf-8').read().split('\n')
while lines and not lines[-1].strip():
    lines.pop()
cols = max(len(l) for l in lines)

rows = []
for n, line in enumerate(lines, 1):
    row = []
    for ch in line.ljust(cols, ' '):
        if ch == ' ':
            row.append(0)
        elif 0x2800 <= ord(ch) <= 0x28FF:
            row.append(ord(ch) - 0x2800)
        else:
            sys.exit('%s:%d: not a braille character: %r' % (src, n, ch))
    rows.append(row)

with open('src/ui/about_art.h', 'w') as h:
    h.write('/* Generated from %s by tools/art2c.py. Do not edit. */\n' % src)
    h.write('#pragma once\n#include <stdint.h>\n\n')
    h.write('#define ABOUT_ART_ROWS %d\n#define ABOUT_ART_COLS %d\n\n' % (len(rows), cols))
    h.write('// Braille dot patterns (codepoint - U+2800), one per text cell.\n')
    h.write('extern const uint8_t about_art[ABOUT_ART_ROWS][ABOUT_ART_COLS];\n')

with open('src/ui/about_art.c', 'w') as c:
    c.write('/* Generated from %s by tools/art2c.py. Do not edit. */\n' % src)
    c.write('#include "ui/about_art.h"\n\n')
    c.write('const uint8_t about_art[ABOUT_ART_ROWS][ABOUT_ART_COLS] = {\n')
    for row in rows:
        c.write('    { %s },\n' % ', '.join('0x%02x' % v for v in row))
    c.write('};\n')
print('about art: %d x %d' % (cols, len(rows)))
