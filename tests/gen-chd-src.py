#!/usr/bin/env python3
"""Writes the CDs the gate's CHD leg turns into .chd files with chdman:

  mixed.bin + mixed.cue  a raw image of three tracks: 300 MODE1 sectors with
                         sync, header, EDC and ECC as on a real disc; an audio
                         track whose 2-second pregap is in the image (INDEX
                         00), 150 + 200 sectors; and an audio track of 101
                         sectors (not a multiple of 4, so the CHD pads it)
                         whose 2-second pregap is not (PREGAP).
  data.iso               200 sectors of 2048 bytes (a CHD keeps them cooked).

Every sector's bytes say where it is, and the audio is not symmetric in its
byte order, so a sector out of place or a sample with its bytes swapped
shows. The bytes depend only on this script.

usage: gen-chd-src.py <out folder>
"""
import os
import struct
import sys

# The CD-ROM EDC (a CRC-32 with the polynomial 0x8001801B, bits reversed)
# and the P and Q Reed-Solomon codes of ECMA-130, as cdrdao and ECM compute
# them.
EDC = []
for i in range(256):
    e = i
    for _ in range(8):
        e = (e >> 1) ^ (0xD8018001 if e & 1 else 0)
    EDC.append(e)
F = [((i << 1) ^ (0x11D if i & 0x80 else 0)) & 0xFF for i in range(256)]
B = [0] * 256
for i in range(256):
    B[i ^ F[i]] = i


def edc(data):
    e = 0
    for b in data:
        e = (e >> 8) ^ EDC[(e ^ b) & 0xFF]
    return e


def ecc_block(s, major_count, minor_count, major_mult, minor_inc, dest):
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = s[12 + index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = F[a]
        a = B[F[a] ^ b]
        s[dest + major] = a
        s[dest + major + major_count] = a ^ b


def bcd(v): return ((v // 10) << 4) | (v % 10)


def mode1(lba, user):
    s = bytearray(2352)
    s[0:12] = b'\x00' + b'\xff' * 10 + b'\x00'
    a = lba + 150
    s[12:16] = bytes([bcd(a // 4500), bcd(a // 75 % 60), bcd(a % 75), 1])
    s[16:2064] = user
    s[2064:2068] = struct.pack('<I', edc(s[0:2064]))
    ecc_block(s, 86, 24, 2, 86, 0x81C)
    ecc_block(s, 52, 43, 86, 88, 0x8C8)
    return bytes(s)


def user(tag, n):
    head = ('%s sector %d ' % (tag, n)).encode()
    return (head * (2048 // len(head) + 1))[:2048]


def audio(tag, n):
    # 588 stereo samples; left and right differ, and so do a sample's bytes
    out = bytearray()
    for i in range(588):
        out += struct.pack('<hh', (tag * 1000 + n * 7 + i) & 0x7FFF, -((n * 13 + i * 3) & 0x7FFF))
    return bytes(out)


def msf(p): return '%02d:%02d:%02d' % (p // 4500, p // 75 % 60, p % 75)


def main(out):
    os.makedirs(out, exist_ok=True)
    data, a1, a2 = 300, 350, 101
    with open(os.path.join(out, 'mixed.bin'), 'wb') as f:
        for n in range(data):
            f.write(mode1(n, user('MIXED', n)))
        for n in range(a1):
            f.write(audio(2, n))
        for n in range(a2):
            f.write(audio(3, n))
    with open(os.path.join(out, 'mixed.cue'), 'w', newline='\n') as f:
        f.write('FILE "mixed.bin" BINARY\n')
        f.write('  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n')
        f.write('  TRACK 02 AUDIO\n    INDEX 00 %s\n    INDEX 01 %s\n' % (msf(data), msf(data + 150)))
        f.write('  TRACK 03 AUDIO\n    PREGAP 00:02:00\n    INDEX 01 %s\n' % msf(data + a1))
    with open(os.path.join(out, 'data.iso'), 'wb') as f:
        for n in range(200):
            f.write(user('ISO', n))


if __name__ == '__main__':
    main(sys.argv[1])
