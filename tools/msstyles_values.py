#!/usr/bin/env python3
"""Like msstyles_dump.py, plus the value: int for 4-byte values, hex otherwise.
Usage: msstyles_values.py <file.msstyles> [class]"""
import struct
import sys

import msstyles_dump as m
import pefile

pe = pefile.PE(sys.argv[1])
names = m.classes(m.resource(pe, "CMAP", "CMAP"))
blob = m.resource(pe, "VARIANT", "NORMAL")
off = 0
while off + 32 <= len(blob):
    prop, typ, cls, part, state, short, _res, size = struct.unpack_from("<8i", blob, off)
    off += 32
    if short:
        val = str(size)
    else:
        data = blob[off:off + size]
        off += (size + 7) & ~7
        val = str(struct.unpack("<i", data)[0]) if size == 4 else data.hex()
    if len(sys.argv) < 3 or names[cls] == sys.argv[2]:
        print(f"{names[cls]}\t{part}\t{state}\t{prop}\t{typ}\t{val}")
