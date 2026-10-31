#!/usr/bin/env python3
"""Writes one hard disk image in each of the formats the hdd and scsi slots
take besides .hdi/.nhd/.thd/.vhd: SL9821's .slh (IDE), Virtual98's .hdd and
RaSCSI's .hdn and .hds (SCSI). Each holds the sectors of a T98-Next .nhd
(as tests/gen-testdisk.py --hdd writes it), so the same boot sector boots
from every one.

usage: gen-hdformats.py <in.nhd> <out folder>
"""
import os
import struct
import sys

if len(sys.argv) != 3:
    raise SystemExit(__doc__)
nhd = open(sys.argv[1], 'rb').read()
assert nhd[:14] == b'T98HDDIMAGE.R0'
head, cyl, surf, sec, size = struct.unpack_from('<IIHHH', nhd, 0x110)
data = nhd[head:]
assert size == 512 and len(data) == cyl * surf * sec * size
out = sys.argv[2]


def write(name, b):
    with open(os.path.join(out, name), 'wb') as f:
        f.write(b)


# SL9821: "HDIM", then the geometry, a 512-byte header (fdd/sxsihdd.h SLHHDR)
slh = bytearray(512)
slh[0:4] = b'HDIM'
struct.pack_into('<QIIII', slh, 4, len(data), size, cyl, surf, sec)
write('disk.slh', bytes(slh) + data)

# Virtual98: "VHD1.00", geometry and the total sector count (VHDHDR, 220 bytes)
vhd = bytearray(220)
vhd[0:7] = b'VHD1.00'
struct.pack_into('<HHBBHI', vhd, 140, len(data) >> 20, size, sec, surf, cyl, cyl * surf * sec)
write('disk.hdd', bytes(vhd) + data)

# RaSCSI: no header; NP2kai works the geometry out from the size, so the
# image is padded to whole cylinders of 8 heads x 32 (.hds) or x 25 (.hdn)
# 512-byte sectors
for ext, spt in (('hds', 32), ('hdn', 25)):
    unit = 8 * spt * 512
    write('disk.' + ext, data + bytes(-len(data) % unit))
