#!/usr/bin/env python3
"""Draws the PC-98 keyboard picture the package shows beside its bindings
(Config > Controls): waterbox/pictures/keyboard.png.

The drawing is this project's own (user-decided, 2026-10-23), so it is
under the integration's MIT licence. Every key is labelled with the column
header it has in waterbox/np2-input.h, so the picture and the controller
name the same keys; a key of the table missing from the layout below, or a
key in the layout the table does not have, stops the script.

Run from anywhere: python3 tools/draw-keyboard.py   (needs Pillow)
"""
import os
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(ROOT, 'waterbox', 'np2-input.h'), encoding='utf-8').read()
keys = dict(re.findall(r'X\("(Key [^"]+)", "([^"]*)"', src))   # name -> header

# The layout, in key units: (name, x, y, width, height). Rows as on the
# PC-9801 keyboard: the function row, four rows of main keys with the space
# row under them, the editing keys beside them and the number pad on the
# right.
L = []
def row(y, x, names, w=1.0):
    for n in names:
        L.append(('Key ' + n, x, y, w, 1.0))
        x += w
    return x

x = row(0, 0, ['STOP', 'COPY'])
x = row(0, x + 0.5, ['f.%d' % i for i in range(1, 6)], 1.2)
x = row(0, x + 0.3, ['f.%d' % i for i in range(6, 11)], 1.2)
row(0, x + 0.5, ['vf.%d' % i for i in range(1, 6)], 1.0)

Y = 1.4
x = row(Y, 0, ['ESC', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'Minus', 'Caret', 'Yen'])
L.append(('Key BS', x, Y, 1.5, 1.0))
L.append(('Key TAB', 0, Y + 1, 1.5, 1.0))
x = row(Y + 1, 1.5, ['Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'At', 'Left Bracket'])
L.append(('Key RETURN', x + 0.25, Y + 1, 1.25, 2.0))
L.append(('Key CTRL', 0, Y + 2, 1.0, 1.0))
L.append(('Key CAPS', 0, Y + 4, 1.0, 1.0))
row(Y + 2, 1.75, ['A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'Semicolon', 'Colon', 'Right Bracket'])
L.append(('Key SHIFT', 0, Y + 3, 2.25, 1.0))
x = row(Y + 3, 2.25, ['Z', 'X', 'C', 'V', 'B', 'N', 'M', 'Comma', 'Period', 'Slash', 'Underscore'])
L.append(('Key Right SHIFT', x, Y + 3, 1.5, 1.0))
x = row(Y + 4, 1.0, ['KANA', 'GRPH', 'NFER'])
L.append(('Key SPACE', x, Y + 4, 6.0, 1.0))
L.append(('Key XFER', x + 6.0, Y + 4, 1.25, 1.0))

E = 16.0   # the editing keys
row(Y, E, ['ROLL UP', 'ROLL DOWN'], 1.25)
row(Y + 1, E, ['INS', 'DEL'], 1.25)
L.append(('Key Up', E + 0.75, Y + 2, 1.0, 1.0))
L.append(('Key Left', E, Y + 3, 1.25, 1.0))
L.append(('Key Right', E + 1.25, Y + 3, 1.25, 1.0))
L.append(('Key Down', E + 0.75, Y + 4, 1.0, 1.0))

P = 19.3   # the number pad
row(Y, P, ['HOME CLR', 'HELP', 'Pad Minus', 'Pad Slash'])
row(Y + 1, P, ['Pad 7', 'Pad 8', 'Pad 9', 'Pad Asterisk'])
row(Y + 2, P, ['Pad 4', 'Pad 5', 'Pad 6', 'Pad Plus'])
row(Y + 3, P, ['Pad 1', 'Pad 2', 'Pad 3', 'Pad Equals'])
row(Y + 4, P, ['Pad 0', 'Pad Comma', 'Pad Period'])

drawn = [n for n, *_ in L]
assert len(drawn) == len(set(drawn)), 'a key is drawn twice'
missing = sorted(set(keys) - set(drawn))
extra = sorted(set(drawn) - set(keys))
assert not missing and not extra, ('missing', missing, 'not in the table', extra)

U = 44         # pixels a key unit
PAD = 12
W = int((P + 4) * U) + 2 * PAD
H = int((Y + 5) * U) + 2 * PAD
img = Image.new('RGB', (W, H), (236, 233, 226))
d = ImageDraw.Draw(img)
font = ImageFont.load_default(size=12)
small = ImageFont.load_default(size=10)
for name, x, y, w, h in L:
    x0, y0 = PAD + x * U + 2, PAD + y * U + 2
    x1, y1 = PAD + (x + w) * U - 2, PAD + (y + h) * U - 2
    d.rounded_rectangle([x0 + 1, y0 + 2, x1 + 1, y1 + 2], 5, fill=(170, 166, 158))
    d.rounded_rectangle([x0, y0, x1, y1], 5, fill=(250, 249, 245), outline=(120, 116, 108))
    label = keys[name]
    f = font if d.textlength(label, font=font) <= (x1 - x0 - 6) else small
    tw = d.textlength(label, font=f)
    d.text(((x0 + x1 - tw) / 2, (y0 + y1) / 2 - 7), label, fill=(40, 40, 40), font=f)

out = os.path.join(ROOT, 'waterbox', 'pictures', 'keyboard.png')
os.makedirs(os.path.dirname(out), exist_ok=True)
img.save(out, optimize=True)
print('%s: %d keys, %dx%d' % (out, len(L), W, H))
