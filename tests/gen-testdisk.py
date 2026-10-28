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

With --rhythm the boot sector plays the 86 board's six drums one by one,
then all together, then a short beat, half a second apart, and stops: sound
for the legs that check where the drums come from (setting rhythmSamples).

With --hdd the counter is instead the first sector of a small hard disk
(.nhd, 20 cylinders x 8 heads x 17 sectors of 512 bytes): the leg that
boots a hard disk with the real IDE BIOS looks for its frame count. With
--hdd --no-boot that sector is left empty, the control that boots nothing.

usage: gen-testdisk.py [--no-write | --counter | --rhythm | --hdd [--no-boot]] <out>
"""
import struct
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

# The drums, assembled (nasm -f bin) from:
#	bits 16
#	org 0
#	start:	mov ax, cs
#		mov ds, ax
#		cld
#		mov si, init
#		call regs
#		mov si, hits
#	next:	lodsb			; AL = key-on bits, 0 = done
#		test al, al
#		jz hang
#		mov ah, al
#		mov al, 0x10
#		call put
#		mov cx, 30		; half a second between hits
#	w1:	in al, 0x60		; text GDC status: end of retrace
#		test al, 0x20
#		jnz w1
#	w2:	in al, 0x60		; then the next retrace
#		test al, 0x20
#		jz w2
#		loop w1
#		jmp next
#	hang:	jmp hang
#	regs:	lodsw			; AL = register, AH = value, 0xff ends
#		cmp al, 0xff
#		je .r
#		call put
#		jmp regs
#	.r:	ret
#	put:	mov dx, 0x188		; the 86 board's OPNA: address
#		out dx, al
#		mov al, ah
#		mov dx, 0x18a		; data
#		out dx, al
#		ret
#	init:	db 0x11, 0x30		; rhythm total level: 11 dB down (fmgen clips at the top)
#		db 0x18, 0xdf, 0x19, 0xdf, 0x1a, 0xdf	; each drum: both sides, loudest
#		db 0x1b, 0xdf, 0x1c, 0xdf, 0x1d, 0xdf
#		db 0xff, 0xff
#	hits:	db 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3f, 0x01, 0x08, 0x01, 0x08, 0x02, 0x08, 0
#		db "CHIMERA NP2KAI RHYTHM TEST", 0
RHYTHM = bytes.fromhex(
    "8cc88ed8fcbe4500e82400be5500ac84c0741a88c4b010e82000b91e00e4"
    "60a82075fae460a82074fae2f2ebe1ebfead3cff7405e80300ebf6c3ba88"
    "01ee88e0ba8a01eec3113018df19df1adf1bdf1cdf1ddfffff0102040810"
    "203f010801080208004348494d455241204e50324b41492052485954484d"
    "205445535400")

args = sys.argv[1:]
no_write = "--no-write" in args
counter = "--counter" in args
rhythm = "--rhythm" in args
hdd = "--hdd" in args
no_boot = "--no-boot" in args
args = [a for a in args if a not in ("--no-write", "--counter", "--rhythm", "--hdd", "--no-boot")]
if len(args) != 1 or no_write + counter + rhythm + hdd > 1 or (no_boot and not hdd):
    sys.exit(__doc__)
if hdd:
    # T98-Next's .nhd: its signature, a comment, then the geometry
    cyl, heads, secs, size = 20, 8, 17, 512
    head = b"T98HDDIMAGE.R0\0\0" + bytes(0x100) + struct.pack("<IIHHH", 512, cyl, heads, secs, size)
    image = bytearray(cyl * heads * secs * size)
    if not no_boot:
        image[:len(COUNTER)] = COUNTER
    open(args[0], "wb").write(head + bytes(512 - len(head)) + image)
    sys.exit(0)
ipl = bytearray(COUNTER if counter else RHYTHM if rhythm else IPL)
assert counter or rhythm or ipl[INT_1BH:INT_1BH + 2] == b"\xcd\x1b"
if no_write:
    ipl[INT_1BH:INT_1BH + 2] = b"\x90\x90"
image = bytearray(SIZE)
image[:len(ipl)] = ipl
open(args[0], "wb").write(image)
