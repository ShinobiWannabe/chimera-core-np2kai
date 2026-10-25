#!/usr/bin/env python3
"""Writes a 2HD floppy image (.hdm, raw, 77 cylinders x 2 heads x 8 sectors
of 1024 bytes) whose boot sector copies itself to the next sector with the
PC-98 disk BIOS (INT 1Bh, write data) and then stops. Free, machine-made
content for the gate: the bytes are a pure function of this file.

The save-data legs boot it, export the disk the machine wrote, and check
that sector 2 now holds sector 1 and nothing else changed. With --no-write
the INT 1Bh is two NOPs: the control disk, which writes nothing.

With --counter the boot sector is instead the counter the tools legs search
for (waterbox/tests/frontend-tools.lua): it counts frames and keys in its
own memory and shows both counts on the text screen.

usage: gen-testdisk.py [--no-write | --counter] <out.hdm>
"""
import sys

SECTOR = 1024
SIZE = 77 * 2 * 8 * SECTOR

# The boot sector, assembled (nasm -f bin) from:
#	bits 16
#	org 0
#	start:	mov ax, cs
#		mov ds, ax
#		mov es, ax
#		mov ah, 0x75		; write data: MFM, retry, seek
#		mov al, 0x90		; 1 MB floppy drive, unit 0
#		mov bx, 1024		; one sector
#		mov cx, 0x0300		; N = 3 (1024 bytes), cylinder 0
#		mov dx, 0x0002		; head 0, sector 2
#		xor bp, bp		; ES:BP = this sector, as the BIOS loaded it
#		int 0x1b		; (offset 0x15)
#		mov [result], ah	; the BIOS's answer, kept in memory for a look
#	hang:	jmp hang
#	result:	db 0xff
#		db "CHIMERA NP2KAI SAVE DATA TEST", 0
# The bytes are written out so that no assembler is needed to make the disk.
IPL = bytes.fromhex(
    "8cc88ed88ec0b475b090bb0004b90003ba020031edcd1b88261d00ebfeff"
    "4348494d455241204e50324b414920534156452044415441205445535400")
INT_1BH = 0x15

# The counter, assembled (nasm -f bin) from:
#	bits 16
#	org 0
#	start:	mov ax, cs
#		mov ds, ax
#		mov ax, 0xa000		; ES = the text screen
#		mov es, ax
#		cld
#	main:
#	w1:	in al, 0x60		; text GDC status: wait for the end of a retrace
#		test al, 0x20
#		jnz w1
#	w2:	in al, 0x60		; then for the next retrace
#		test al, 0x20
#		jz w2
#		inc word [frames]	; 1FC70h: one a frame
#		mov ah, 0x01		; key buffer sense
#		int 0x18
#		test bh, bh
#		jz show
#		xor ah, ah		; read the key, count it
#		int 0x18
#		inc byte [keys]		; 1FC72h
#		mov [lastkey], ax	; 1FC74h: character, key code
#	show:	xor di, di		; both counts in hex at the top left
#		mov ax, [frames]
#		call hex4
#		add di, 2
#		mov al, [keys]
#		call hex2
#		jmp main
#	hex4:	push ax
#		mov al, ah
#		call hex2
#		pop ax
#	hex2:	push ax
#		shr al, 4
#		call digit
#		pop ax
#	digit:	and al, 0x0f
#		add al, '0'
#		cmp al, '9'
#		jbe .d
#		add al, 7
#	.d:	xor ah, ah
#		stosw
#		mov byte [es:di+0x1ffe], 0xe1	; its attribute: white, shown
#		ret
#		align 16
#	frames:	dw 0
#	keys:	db 0, 0
#	lastkey: dw 0
#		db "CHIMERA NP2KAI COUNTER TEST", 0
COUNTER = bytes.fromhex(
    "8cc88ed8b800a08ec0fce460a82075fae460a82074faff067000b401cd18"
    "84ff740b30e4cd18fe067200a3740031ffa17000e80b0083c702a07200e8"
    "0900ebca5088e0e801005850c0e804e8010058240f04303c397602040730"
    "e4ab26c685fe1fe1c3909090909090909090909090900000000000004348"
    "494d455241204e50324b414920434f554e544552205445535400")

args = sys.argv[1:]
no_write = "--no-write" in args
counter = "--counter" in args
args = [a for a in args if a not in ("--no-write", "--counter")]
if len(args) != 1 or (no_write and counter):
    sys.exit(__doc__)
ipl = bytearray(COUNTER if counter else IPL)
assert counter or ipl[INT_1BH:INT_1BH + 2] == b"\xcd\x1b"
if no_write:
    ipl[INT_1BH:INT_1BH + 2] = b"\x90\x90"
image = bytearray(SIZE)
image[:len(ipl)] = ipl
open(args[0], "wb").write(image)
