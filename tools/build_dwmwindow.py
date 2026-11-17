#!/usr/bin/env python3
"""Build the DWMWindow (caption buttons, frame) part of the Win8 beta style.

Usage: build_dwmwindow.py <donor 8250 aero.msstyles> <target aero.msstyles> <out dir> [parts]
  parts: target parts to take from the donor, e.g. "0-10,44-46" (default: all);
  the others keep the target's own records (for bisecting load failures)
Writes:
  <out>/VARIANT_NORMAL.bin  the target's property blob with DWMWindow's records
                            replaced by the donor's, renumbered (docs/dwmwindow-mapping.md)
  <out>/STREAM_<id>.png     the donor's DWMWindow atlas, under the target's resource id
Apply both to a copy of the target msstyles with tools/apply_resources.ps1 (Windows).
"""
import os
import struct
import sys

import msstyles_dump as m
import pefile

CLASS = "DWMWindow"


def blob_records(blob):
    """(header tuple, raw bytes of the whole record) in file order."""
    off = 0
    while off + 32 <= len(blob):
        h = struct.unpack_from("<8i", blob, off)
        end = off + 32 + (0 if h[5] else (h[7] + 7) & ~7)
        yield h, blob[off:end]
        off = end
    if off != len(blob):
        raise SystemExit(f"record walk ended at {off} of {len(blob)}")


def with_class_part(raw, cls, part):
    return raw[:8] + struct.pack("<ii", cls, part) + raw[16:]


def donor_part(target_part):
    """Which donor part fills a target part (None: keep nothing)."""
    if target_part <= 10:
        return target_part
    if target_part <= 63:
        return target_part + 1  # the donor has no part 11
    if 64 <= target_part <= 83:
        return target_part - 64 + 12  # white glyphs: the donor's glyphs are white
    if 84 <= target_part <= 87:
        return target_part - 84 + 39  # small white close glyphs
    if 88 <= target_part <= 91:
        return target_part - 88 + 3  # dark-mode button backgrounds
    return None


def stream_ids(pe):
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if str(t.name) == "STREAM":
            return sorted(n.id for n in t.directory.entries)
    return []


def main():
    donor_pe, target_pe = pefile.PE(sys.argv[1]), pefile.PE(sys.argv[2])
    out = sys.argv[3]
    os.makedirs(out, exist_ok=True)
    dnames = m.classes(m.resource(donor_pe, "CMAP", "CMAP"))
    tnames = m.classes(m.resource(target_pe, "CMAP", "CMAP"))
    dcls, tcls = dnames.index(CLASS), tnames.index(CLASS)

    donor = [(h, raw) for h, raw in blob_records(m.resource(donor_pe, "VARIANT", "NORMAL")) if h[2] == dcls]
    by_part = {}
    for h, raw in donor:
        by_part.setdefault(h[3], []).append(raw)
    target_parts = sorted({h[3] for h, _ in blob_records(m.resource(target_pe, "VARIANT", "NORMAL")) if h[2] == tcls})

    only = None
    if len(sys.argv) > 4:
        only = set()
        for r in sys.argv[4].split(","):
            a, _, b = r.partition("-")
            only |= set(range(int(a), int(b or a) + 1))
    own = {}
    for h, raw in blob_records(m.resource(target_pe, "VARIANT", "NORMAL")):
        if h[2] == tcls:
            own.setdefault(h[3], []).append(raw)

    # Part 0 (class-wide values) and every target part, from the mapped donor part
    new = []
    for tp in target_parts:
        # Part 0 is one record, prop 213, whose 6th header field is the atlas's
        # STREAM id: keep the target's (the donor atlas goes in under that id)
        if tp == 0 or (only is not None and tp not in only):
            new += own[tp]
            continue
        dp = donor_part(tp)
        if dp is None or dp not in by_part:
            print(f"FAIL target part {tp}: no donor part {dp}")
            continue
        new += [with_class_part(raw, tcls, tp) for raw in by_part[dp]]

    # Replace the target's DWMWindow block (contiguous) with the new records
    out_blob, placed, seen = bytearray(), False, False
    for h, raw in blob_records(m.resource(target_pe, "VARIANT", "NORMAL")):
        if h[2] == tcls:
            if seen and not placed_run:
                raise SystemExit("FAIL DWMWindow records are not contiguous")
            if not placed:
                out_blob += b"".join(new)
                placed = True
            seen, placed_run = True, True
            continue
        placed_run = False
        out_blob += raw
    open(os.path.join(out, "VARIANT_NORMAL.bin"), "wb").write(out_blob)
    print(f"PASS VARIANT/NORMAL: {len(new)} DWMWindow records (target had "
          f"{sum(1 for h, _ in blob_records(m.resource(target_pe, 'VARIANT', 'NORMAL')) if h[2] == tcls)}), "
          f"{len(out_blob)} bytes")

    # The DWMWindow atlas is the first STREAM in both files (one atlas per DWM class, in class order)
    d_id, t_id = stream_ids(donor_pe)[0], stream_ids(target_pe)[0]
    open(os.path.join(out, f"STREAM_{t_id}.png"), "wb").write(m.resource(donor_pe, "STREAM", d_id))
    print(f"PASS atlas: donor STREAM {d_id} -> target STREAM {t_id}")


if __name__ == "__main__":
    placed_run = False
    main()
