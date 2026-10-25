#!/usr/bin/env python3
import hashlib, sys

ROM = 'baserom.gba'
out_c = 'src/story_assets.c'
out_h = 'include/story_assets.h'

rom = open(ROM,'rb').read()
h = hashlib.sha256(rom).hexdigest()
print('sha256:', h)

# Extract palette at 0x0844E800
pal_off = 0x0844E800 - 0x08000000
pal = rom[pal_off:pal_off+512]
tiles_off = 0x0844F26C - 0x08000000  # rough guess
# Just extract palette for now
with open(out_c,'w') as f:
    f.write('// AUTOGENERADO\n')
    f.write('#include "story_assets.h"\n\n')
    f.write('const unsigned char g_story_pal[512] = {\n')
    for i,b in enumerate(pal):
        if i%16==0: f.write('    ')
        f.write(f'0x{b:02X},')
        if i%16==15: f.write('\n')
        else: f.write(' ')
    f.write('};\n')

with open(out_h,'w') as f:
    f.write('#ifndef STORY_ASSETS_H\n#define STORY_ASSETS_H\n\n')
    f.write('extern const unsigned char g_story_pal[512];\n\n')
    f.write('#endif\n')

print('done')
