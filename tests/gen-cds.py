#!/usr/bin/env python3
"""Writes two small ISO 9660 CDs, one.iso and two.iso (volume labels DISCONE
and DISCTWO, each holding one text file), for the gate's CD swap leg. The
bytes depend only on this script: no dates, no tools outside it.

usage: gen-cds.py <out folder>
"""
import os
import struct
import sys


def both16(v): return struct.pack('<H', v) + struct.pack('>H', v)
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)


SECTORS = 32
DATE7 = bytes([95, 1, 1, 0, 0, 0, 0])          # 1995-01-01 00:00:00
DATE17 = b'1995010100000000' + b'\x00'


def record(extent, size, flags, name):
    r = struct.pack('<BB', 0, 0) + both32(extent) + both32(size) + DATE7 + \
        bytes([flags, 0, 0]) + both16(1) + bytes([len(name)]) + name
    if len(r) % 2: r += b'\x00'
    return bytes([len(r)]) + r[1:]


def iso(label, fname, text):
    s = 2048
    root, ptl, ptm, data = 20, 18, 19, 21     # sectors
    rootdir = record(root, s, 2, b'\x00') + record(root, s, 2, b'\x01') + \
        record(data, len(text), 0, fname)
    pvd = bytearray(s)
    pvd[0:8] = b'\x01CD001\x01\x00'
    pvd[8:40] = b' ' * 32
    pvd[40:72] = label.ljust(32).encode()
    pvd[80:88] = both32(SECTORS)
    pvd[120:124] = both16(1); pvd[124:128] = both16(1); pvd[128:132] = both16(s)
    pvd[132:140] = both32(10)
    pvd[140:144] = struct.pack('<I', ptl); pvd[148:152] = struct.pack('>I', ptm)
    pvd[156:190] = record(root, s, 2, b'\x00')
    for o, n in ((190, 128), (318, 128), (446, 128), (574, 128), (702, 37), (739, 37), (776, 37)):
        pvd[o:o + n] = b' ' * n
    for o in (813, 830, 847, 864):
        pvd[o:o + 17] = DATE17 if o == 813 or o == 830 else b'0' * 16 + b'\x00'
    pvd[881] = 1
    term = bytearray(s); term[0:7] = b'\xffCD001\x01'
    pt = lambda big: bytes([1, 0]) + (struct.pack('>I', root) if big else struct.pack('<I', root)) + \
        (struct.pack('>H', 1) if big else struct.pack('<H', 1)) + b'\x00\x00'
    img = bytearray(SECTORS * s)
    img[16 * s:17 * s] = pvd
    img[17 * s:18 * s] = term
    img[ptl * s:ptl * s + 10] = pt(False)
    img[ptm * s:ptm * s + 10] = pt(True)
    img[root * s:root * s + len(rootdir)] = rootdir
    img[data * s:data * s + len(text)] = text
    assert len(img) == SECTORS * s
    return bytes(img)


if len(sys.argv) != 2:
    raise SystemExit(__doc__)
for n, label in (('one', 'DISCONE'), ('two', 'DISCTWO')):
    with open(os.path.join(sys.argv[1], n + '.iso'), 'wb') as f:
        f.write(iso(label, (n.upper() + '.TXT;1').encode(), ('THIS IS %s\r\n' % label).encode()))
