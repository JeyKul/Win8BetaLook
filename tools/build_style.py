#!/usr/bin/env python3
"""Build the Win8 beta style: the target msstyles with chosen classes taken from the donor.

Usage: build_style.py <donor aero.msstyles> <target aero.msstyles> <out dir> <class>...
  <class> is a class name both files have (taken over 1:1), or DWMWindow (renumbered,
  see docs/dwmwindow-mapping.md). A class name may be a regex: '^TaskBar' etc.
  'Donor=Target' takes the donor class's records into a different target class
  (e.g. 'TaskBarComposited::TaskBar=TaskBar': 8250's glass taskbar as the plain one).
Writes <out>/VARIANT_NORMAL.bin, <out>/STREAM_<id>.png (DWMWindow atlas) and
<out>/IMAGE_<id>.png for every donor image the taken classes use (new ids after the
target's highest). Apply in the VM with tools/apply_resources.ps1.

Record header: 8 x int32 (prop, type, class, part, state, ref, reserved, size). When
`ref` is non-zero it references a resource and no value follows: an IMAGE id for type
206, the class's atlas STREAM id for type 213. Other types (203, 209, 210) keep their
ids, which index string/data tables both files share.
"""
import os
import re
import struct
import sys

import msstyles_dump as m
import pefile

from build_dwmwindow import blob_records, donor_part


def res_ids(pe, rtype):
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if str(t.name) == rtype:
            return sorted(n.id for n in t.directory.entries)
    return []


def main():
    donor_pe, target_pe = pefile.PE(sys.argv[1]), pefile.PE(sys.argv[2])
    out, wanted = sys.argv[3], sys.argv[4:]
    os.makedirs(out, exist_ok=True)
    dnames = m.classes(m.resource(donor_pe, "CMAP", "CMAP"))
    tnames = m.classes(m.resource(target_pe, "CMAP", "CMAP"))
    dblob = list(blob_records(m.resource(donor_pe, "VARIANT", "NORMAL")))
    tblob = list(blob_records(m.resource(target_pe, "VARIANT", "NORMAL")))
    dused = {dnames[h[2]] for h, _ in dblob}
    tused = {tnames[h[2]] for h, _ in tblob}

    take, renamed = set(), {}
    for w in wanted:
        if "=" in w:
            d, t = w.split("=", 1)
            if d not in dused or t not in tused:
                print(f"FAIL {w}: {d} not in donor or {t} not in target")
                continue
            renamed[t] = d
            take.add(t)
            continue
        hits = {c for c in dused & tused if c == w or re.search(w, c)}
        if not hits:
            print(f"FAIL {w}: no class with that name in both files")
        take |= hits

    next_image = max(res_ids(target_pe, "IMAGE")) + 1
    image_map = {}  # donor image id -> new target id

    def image(donor_id):
        nonlocal next_image
        if donor_id not in image_map:
            image_map[donor_id] = next_image
            open(os.path.join(out, f"IMAGE_{next_image}.png"), "wb").write(
                m.resource(donor_pe, "IMAGE", donor_id))
            next_image += 1
        return image_map[donor_id]

    def convert(raw, tcls, part):
        h = struct.unpack_from("<8i", raw)
        raw = raw[:8] + struct.pack("<ii", tcls, part) + raw[16:]
        if h[5] and h[1] == 206:
            raw = raw[:20] + struct.pack("<i", image(h[5])) + raw[24:]
        return raw

    new_blocks = {}
    for c in sorted(take):
        dcls, tcls = dnames.index(renamed.get(c, c)), tnames.index(c)
        drecs = [raw for h, raw in dblob if h[2] == dcls]
        if c == "DWMWindow":
            by_part, own = {}, {}
            for raw in drecs:
                by_part.setdefault(struct.unpack_from("<i", raw, 12)[0], []).append(raw)
            for h, raw in tblob:
                if h[2] == tcls:
                    own.setdefault(h[3], []).append(raw)
            block = []
            for tp in sorted(own):
                dp = donor_part(tp)
                if tp == 0 or dp not in by_part:  # part 0 holds the atlas reference
                    block += own[tp]
                else:
                    block += [convert(raw, tcls, tp) for raw in by_part[dp]]
            d_atlas, t_atlas = res_ids(donor_pe, "STREAM")[0], res_ids(target_pe, "STREAM")[0]
            open(os.path.join(out, f"STREAM_{t_atlas}.png"), "wb").write(
                m.resource(donor_pe, "STREAM", d_atlas))
        else:
            block = [convert(raw, tcls, struct.unpack_from("<i", raw, 12)[0]) for raw in drecs]
        new_blocks[tcls] = block
        print(f"PASS {c}: {len(block)} records" + (f" from {renamed[c]}" if c in renamed else ""))

    # Each taken class's records go where the target's first record of it was
    out_blob, done = bytearray(), set()
    for h, raw in tblob:
        if h[2] in new_blocks:
            if h[2] not in done:
                out_blob += b"".join(new_blocks[h[2]])
                done.add(h[2])
            continue
        out_blob += raw
    open(os.path.join(out, "VARIANT_NORMAL.bin"), "wb").write(out_blob)
    print(f"PASS {len(take)} classes, {len(image_map)} images copied, VARIANT {len(out_blob)} bytes")


if __name__ == "__main__":
    main()
