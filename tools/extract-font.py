#!/usr/bin/env python3
"""Extract the IBM VGA 8x16 font (code page 437) from QEMU's SeaBIOS VGA BIOS.

usage: tools/extract-font.py [VGABIOS] > src/gfx/font8x16.c
"""
import sys

GLYPH_A = bytes([0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe,
                 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00])
GLYPH_SMILEY = bytes([0x00, 0x00, 0x7e, 0x81, 0xa5, 0x81, 0x81, 0xbd,
                      0x99, 0x81, 0x81, 0x7e, 0x00, 0x00, 0x00, 0x00])

src = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/qemu/vgabios-stdvga.bin'
data = open(src, 'rb').read()
pos = data.find(GLYPH_A)
start = pos - ord('A') * 16
font = data[start:start + 4096] if pos >= 0 and start >= 0 else b''
if len(font) != 4096 or font[16:32] != GLYPH_SMILEY or any(font[:16]):
    sys.exit('could not locate the 8x16 VGA font in ' + src)

print('/* IBM VGA 8x16 font (code page 437), extracted from the SeaBIOS VGA BIOS')
print(' * by tools/extract-font.py. Do not edit. */')
print('#include "gfx/font.h"')
print()
print('const uint8_t font8x16[256][16] = {')
for i in range(256):
    glyph = ', '.join('0x%02x' % b for b in font[i * 16:(i + 1) * 16])
    print('    { %s },  /* 0x%02x */' % (glyph, i))
print('};')
