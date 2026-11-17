#!/usr/bin/env python3
"""Crop every DWMWindow part (prop 8002 = atlas rect l,t,r,b) out of the first STREAM
atlas and lay them out with their part/state numbers.
Usage: dwm_atlas_sheet.py <file.msstyles> <values.tsv> <out.png>"""
import io
import struct
import sys

import pefile
from PIL import Image, ImageDraw

pe = pefile.PE(sys.argv[1])
atlases = []
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if str(t.name) == "STREAM":
        for n in sorted(t.directory.entries, key=lambda e: e.id):
            d = n.directory.entries[0].data.struct
            atlases.append(Image.open(io.BytesIO(pe.get_data(d.OffsetToData, d.Size))).convert("RGBA"))
atlas = atlases[0]
rects = []
for line in open(sys.argv[2]):
    c, part, state, prop, typ, val = line.rstrip("\n").split("\t")
    if prop == "8002":
        l, t, r, b = struct.unpack("<4i", bytes.fromhex(val))
        rects.append((int(part), int(state), l, t, r, b))
print("atlas", atlas.size, "max r/b", max(x[4] for x in rects), max(x[5] for x in rects))
cell = 120
cols = 10
sheet = Image.new("RGBA", (cols * cell, ((len(rects) + cols - 1) // cols) * cell), (60, 60, 60, 255))
dr = ImageDraw.Draw(sheet)
for i, (p, s, l, t, r, b) in enumerate(rects):
    x, y = (i % cols) * cell, (i // cols) * cell
    crop = atlas.crop((l, t, r, b))
    crop.thumbnail((cell - 4, cell - 16))
    sheet.alpha_composite(crop, (x + 2, y + 14))
    dr.text((x + 2, y + 1), f"{p}/{s} {r-l}x{b-t}", fill=(255, 255, 0, 255))
sheet.save(sys.argv[3])
