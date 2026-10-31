#!/usr/bin/env python3
"""Writes one floppy image of every layout the floppy slot takes beyond the
first ones (.fdd, the BKDSK names, .flp, .fim, .dip, .ima), into a folder,
with formats.txt listing them for tests/floppy-formats.c. Free,
machine-made content for the gate: the bytes are a pure function of this
file and gen-testdisk.py.

Every sector says where it is: its first bytes are its own C, H, R and N,
then "NP2FMT" and the image's extension, and the rest is a pattern of its
place on the disk. The first sector (C 0, H 0, R 1) is instead the counter
boot sector from gen-testdisk.py (on .hdb's 128-byte first track, R 1 and
R 2 between them), so a layout the PC-98 can start from counts frames as
the counter disk does.

Each line of formats.txt is: file, disk type (2HD or 2DD), cylinders,
sectors per track, N, and the N of cylinder 0 head 0 when it differs
(BKDSK's .hdb: N-88 BASIC's first track is 26 FM sectors of 128 bytes).
Then come the controls: files of the same layouts one byte short, or
with a broken header, that NP2kai must refuse.

usage: gen-formats.py <folder>
"""
import ast
import os
import struct
import sys

# the counter boot sector, as gen-testdisk.py writes it (that file runs as a
# script, so its COUNTER is read rather than imported)
_src = ast.parse(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen-testdisk.py")).read())
COUNTER = next(eval(compile(ast.Expression(n.value), "gen-testdisk.py", "eval"))
               for n in _src.body if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == "COUNTER")
args = sys.argv[1:]

# extension: (disk type, cylinders, sectors, N, N of track 0, header bytes)
LAYOUTS = {
    "flp": ("2HD", 77, 8, 3, None, 0),      # 1.25 MB, as .hdm
    "dip": ("2HD", 77, 8, 3, None, 256),    # 1.25 MB after a 256-byte header
    "fim": ("2HD", 77, 8, 3, None, 256),
    "hd4": ("2HD", 80, 18, 2, None, 0),     # 1.44 MB
    "ima": ("2HD", 80, 18, 2, None, 0),
    "hd5": ("2HD", 80, 15, 2, None, 0),     # 1.21 MB 2HC
    "hd9": ("2HD", 77, 9, 3, None, 0),      # 9 sectors of 1024
    "h01": ("2HD", 77, 26, 1, None, 0),     # 26 sectors of 256
    "hdb": ("2HD", 77, 26, 1, 0, 0),        # N-88 BASIC 2HD
    "dd6": ("2DD", 80, 8, 2, None, 0),      # 640 KB
    "dd9": ("2DD", 80, 9, 2, None, 0),      # 720 KB
    "ddb": ("2DD", 80, 16, 1, None, 0),     # N-88 BASIC 2DD, dd6's size
    "fdd": ("2HD", 77, 8, 3, None, None),   # Virtual98, its own header
}


def sector(ext, c, h, r, n):
    size = 128 << n
    if (c, h) == (0, 0) and (r - 1) * size < len(COUNTER):
        part = COUNTER[(r - 1) * size:r * size]
        return part + bytes(size - len(part))
    head = bytes((c, h, r, n)) + b"NP2FMT" + ext.encode()
    body = bytes(((c * 2 + h) * 31 + r * 7 + i) & 0xff for i in range(size - len(head)))
    return head + body


def track(ext, layout, c, h):
    _, _, secs, n, n0, _ = layout
    if (c, h) == (0, 0) and n0 is not None:
        n = n0
    return [(r, n, sector(ext, c, h, r, n)) for r in range(1, secs + 1)]


def raw(ext, layout):
    data = bytearray(layout[5])
    for c in range(layout[1]):
        for h in range(2):
            for _, _, s in track(ext, layout, c, h):
                data += s
    return bytes(data)


def vfdd(ext, layout):
    """Virtual98's .fdd: a 220-byte header, then 160 x 26 sector IDs of 12
    bytes (C 0xff for none), 32 bytes for special reads, then the data.
    Cylinder 1 head 0 sector 2 has no data in the file: its ID's D byte
    fills it, as Virtual98 writes a sector of one byte."""
    head = b"VFD1.00\0" + b"NP2KAI FORMAT TEST".ljust(128, b"\0") + struct.pack("<hh", 0, 0) + bytes(80)
    ids = bytearray(b"\xff" * (12 * 160 * 26))
    data = bytearray()
    base = len(head) + len(ids) + 32
    for c in range(layout[1]):
        for h in range(2):
            t = c * 2 + h
            for i, (r, n, s) in enumerate(track(ext, layout, c, h)):
                if (c, h, r) == (1, 0, 2):
                    s, ptr = bytes([0xe5]) * len(s), 0xffffffff
                else:
                    ptr = base + len(data)
                    data += s
                struct.pack_into("<BBBBBBBBI", ids, (t * 26 + i) * 12, c, h, r, n, 0xe5, 0, 0, 1, ptr)
    return head + bytes(ids) + bytes(32) + bytes(data)


def main():
    if len(args) != 1:
        sys.exit(__doc__)
    out = args[0]
    os.makedirs(out, exist_ok=True)
    lines = []
    for ext, layout in LAYOUTS.items():
        name = "test." + ext
        image = vfdd(ext, layout) if layout[5] is None else raw(ext, layout)
        open(os.path.join(out, name), "wb").write(image)
        n0 = "" if layout[4] is None else " %d" % layout[4]
        lines.append("%s %s %d %d %d%s" % (name, layout[0], layout[1], layout[2], layout[3], n0))
        # the control: one byte short, or a header that is not Virtual98's
        bad = image[:-1] if layout[5] is not None else b"VFD9.99\0" + image[8:]
        open(os.path.join(out, "short." + ext), "wb").write(bad)
        lines.append("short.%s refuse" % ext)
    open(os.path.join(out, "formats.txt"), "w").write("\n".join(lines) + "\n")


main()
