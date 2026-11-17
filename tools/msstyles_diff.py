#!/usr/bin/env python3
"""Compare two msstyles_dump.py tables (donor vs target).

Usage: msstyles_diff.py <donor.tsv> <target.tsv> [class regex]
Prints, per class: classes only in one side, then (part, state) pairs only in
the donor (to map or drop) and only in the target (need an asset), and how many
shared pairs carry different property sets.
"""
import re
import sys
from collections import defaultdict


def load(path):
    t = defaultdict(lambda: defaultdict(set))  # class -> (part, state) -> {prop}
    for line in open(path, encoding="utf-8"):
        c, part, state, prop, _typ, _size = line.rstrip("\n").split("\t")
        t[c][(int(part), int(state))].add(int(prop))
    return t


def main():
    donor, target = load(sys.argv[1]), load(sys.argv[2])
    pat = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
    keep = lambda c: pat is None or pat.search(c)
    only_d = sorted(c for c in donor if c not in target and keep(c))
    only_t = sorted(c for c in target if c not in donor and keep(c))
    print(f"classes only in donor ({len(only_d)}): {' '.join(only_d)}")
    print(f"classes only in target ({len(only_t)}): {' '.join(only_t)}")
    for c in sorted(set(donor) & set(target)):
        if not keep(c):
            continue
        d, t = donor[c], target[c]
        miss_t = sorted(set(d) - set(t))
        miss_d = sorted(set(t) - set(d))
        changed = sum(1 for k in set(d) & set(t) if d[k] != t[k])
        if miss_t or miss_d or changed:
            fmt = lambda ps: " ".join(f"{p}/{s}" for p, s in ps)
            print(f"{c}: donor-only [{fmt(miss_t)}] target-only [{fmt(miss_d)}] props-differ {changed}")


if __name__ == "__main__":
    main()
