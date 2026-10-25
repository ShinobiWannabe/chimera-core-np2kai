#!/usr/bin/env python3
"""Writes a named game's key presses (a tests/games/*.json "press" script,
the one the native harness takes as NP2_PRESS) as a Chimera input movie:
a LogKey naming only the keys pressed, then one entry a frame, each key
held for 8 frames unless the script says otherwise, as press-script.h does.

usage: game-movie.py <game.json> <out.txt>
"""
import json, sys

conf = json.load(open(sys.argv[1]))
held = []	# (key, first frame, frames)
for p in conf["press"].split(","):
    f = p.split(":")
    held.append((f[1], int(f[0]), int(f[2]) if len(f) > 2 else 8))
keys = list(dict.fromkeys(k for k, _, _ in held))
lines = ["[Input]", "LogKey:#" + "".join(k + "|" for k in keys)]
for n in range(conf["frames"]):
    down = {k for k, at, ln in held if at <= n < at + ln}
    lines.append("|" + "".join("P" if k in down else "." for k in keys) + "|")
lines.append("[/Input]")
open(sys.argv[2], "w", newline="\n").write("\n".join(lines) + "\n")
