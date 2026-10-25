#!/usr/bin/env python3
"""Checks a named game run (a tests/games/*.json) by what is on the screen,
from the run's state at the "before" frame and at the end:

  text checks read text VRAM in the Main Memory dumps (at 0xA0000, 80 columns
  by 25 rows, two bytes a cell, the low byte the character of one-byte text):
    {"row": r, "startswith": s}       row r begins with s at the end
    {"find": c, "moved": [dr, dc]}    the one c on screen moved by dr, dc
  picture checks read the pictures (.ppm from run-native, .tga from
  chimera-run), as RGB:
    {"box": [x, y, w, h], "sha256": h}  that part of the end picture is this
    {"box": [x, y, w, h], "changed": true}  that part differs from before

usage: check-game.py <game.json> <before.mem> <after.mem> [<before.pic> <after.pic>]

Prints one line a check with positions only, never the game's own text or
pixels: the dumps hold copyrighted content and the font.
"""
import hashlib, json, sys

TVRAM, COLS, ROWS = 0xA0000, 80, 25


def screen(path):
    m = open(path, "rb").read()
    return [bytes(m[TVRAM + 2 * (r * COLS + c)] for c in range(COLS)) for r in range(ROWS)]


def picture(path):
    """(width, rows of RGB bytes, top first)"""
    d = open(path, "rb").read()
    if d[:2] == b"P6":
        fields, at = [], 2
        while len(fields) < 3:
            while d[at:at + 1].isspace(): at += 1
            end = at
            while not d[end:end + 1].isspace(): end += 1
            fields.append(int(d[at:end])); at = end
        w, h = fields[0], fields[1]; at += 1
        return w, [d[at + y * w * 3: at + (y + 1) * w * 3] for y in range(h)]
    # TGA, uncompressed true colour, 24 or 32 bits, BGR(A)
    w, h, bpp, desc = d[12] | d[13] << 8, d[14] | d[15] << 8, d[16] // 8, d[17]
    at = 18 + d[0]
    rows = []
    for y in range(h):
        raw = d[at + y * w * bpp: at + (y + 1) * w * bpp]
        rows.append(b"".join(bytes((raw[i + 2], raw[i + 1], raw[i])) for i in range(0, len(raw), bpp)))
    if not desc & 0x20: rows.reverse()  # bottom-up
    return w, rows


def box(pic, b):
    x, y, w, h = b
    return b"".join(row[x * 3:(x + w) * 3] for row in pic[1][y:y + h])


def find(s, ch):
    at = [(r, row.index(ch)) for r, row in enumerate(s) if ch in row]
    return at[0] if len(at) == 1 else None


conf = json.load(open(sys.argv[1]))
mem = sys.argv[2:4]
pics = sys.argv[4:6]
failed = 0
for c in conf["checks"]:
    if "box" in c:
        if len(pics) < 2:
            print("NOT %s (no pictures given)" % c["what"]); failed += 1; continue
        after = box(picture(pics[1]), c["box"])
        if "sha256" in c:
            good = hashlib.sha256(after).hexdigest() == c["sha256"]
        else:
            good = after != box(picture(pics[0]), c["box"])
        note = "box %s" % (c["box"],)
    elif "startswith" in c:
        good = screen(mem[1])[c["row"]].startswith(c["startswith"].encode())
        note = "row %d" % c["row"]
    else:
        ch = c["find"].encode()
        a, b = find(screen(mem[0]), ch), find(screen(mem[1]), ch)
        good = bool(a and b) and (b[0] - a[0], b[1] - a[1]) == tuple(c["moved"])
        note = "%s at %s, then %s" % (c["find"], a, b)
    print("%s %s (%s)" % ("ok" if good else "NOT", c["what"], note))
    failed += not good
sys.exit(1 if failed else 0)
