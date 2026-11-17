#!/usr/bin/env python3
"""Dump an msstyles file's class/part/state/property table, one line per property.

Usage: msstyles_dump.py <file.msstyles> [VARIANT name, default NORMAL] > table.tsv
Output columns: class  part  state  property-id  type-id  size
Classes come from CMAP (UTF-16, index = class id); records from VARIANT/<name>:
8 x int32 header (name, type, class, part, state, short-flag, reserved, size;
for prop 213 the short-flag field holds the atlas STREAM id),
then the value (inline in the header's size field when short-flag is set,
otherwise `size` bytes padded to 8).
"""
import struct
import sys

import pefile


def resource(pe, rtype, rname):
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if str(t.name) != rtype:
            continue
        for n in t.directory.entries:
            if str(n.name) == rname or n.id == rname:
                d = n.directory.entries[0].data.struct
                return pe.get_data(d.OffsetToData, d.Size)
    raise SystemExit(f"no resource {rtype}/{rname}")


def classes(cmap):
    """UTF-16 strings, each with its terminator padded to 8 bytes."""
    names, off = [], 0
    while off < len(cmap):
        end = off
        while end + 1 < len(cmap) and cmap[end:end + 2] != b"\0\0":
            end += 2
        names.append(cmap[off:end].decode("utf-16-le"))
        off = (end + 2 + 7) & ~7
    return names


def records(blob):
    off = 0
    while off + 32 <= len(blob):
        name, typ, cls, part, state, short, _res, size = struct.unpack_from("<8i", blob, off)
        off += 32
        if not short:
            off += (size + 7) & ~7
        yield cls, part, state, name, typ, size if not short else 0


def main():
    path = sys.argv[1]
    variant = sys.argv[2] if len(sys.argv) > 2 else "NORMAL"
    pe = pefile.PE(path, fast_load=False)
    names = classes(resource(pe, "CMAP", "CMAP"))
    for cls, part, state, prop, typ, size in records(resource(pe, "VARIANT", variant)):
        cname = names[cls] if 0 <= cls < len(names) else f"#{cls}"
        print(f"{cname}\t{part}\t{state}\t{prop}\t{typ}\t{size}")


if __name__ == "__main__":
    main()
