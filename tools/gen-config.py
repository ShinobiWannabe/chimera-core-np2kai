#!/usr/bin/env python3
"""Writes waterbox/waterbox.config and waterbox/default_keybinds.json.

The buttons come from waterbox/np2-input.h and the options of the named
settings from waterbox/np2-settings.h, the same tables the driver is built
from, so the package and the core cannot disagree. Everything else a user
reads (settings, firmware, descriptions) is written here.

Run from anywhere: python3 tools/gen-config.py
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WB = os.path.join(ROOT, 'waterbox')


def read(name):
    with open(os.path.join(WB, name), encoding='utf-8') as f:
        return f.read()


def table(src, macro):
    """The X(...) rows of one #define NAME(X) table, as lists of their
    arguments (strings unquoted, everything else as written)."""
    m = re.search(r'#define %s\(X\)(.*?)(?:\n\n|\n#)' % macro, src, re.S)
    if not m:
        raise SystemExit('no table %s' % macro)
    body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S).replace('\\\n', ' ')
    rows = []
    for args in re.findall(r'X\((.*?)\)(?=\s*(?:X\(|$))', body, re.S):
        rows.append([a[1:-1] if a.startswith('"') else a.strip()
                     for a in re.findall(r'\s*("[^"]*"|[^,]+)\s*(?:,|$)', args)])
    return rows


INPUT_H = read('np2-input.h')
SETTINGS_H = read('np2-settings.h')
BUTTONS = table(INPUT_H, 'NP2_BUTTONS')          # [name, header, what]
SOUND_BOARDS = [r[0] for r in table(SETTINGS_H, 'NP2_SOUND_BOARDS')]
MONITORS = [r[0] for r in table(SETTINGS_H, 'NP2_MONITORS')]
BASE_CLOCKS = [r[0] for r in table(SETTINGS_H, 'NP2_BASE_CLOCKS')]
SOUND_GENERATORS = [r[0] for r in table(SETTINGS_H, 'NP2_SOUND_GENERATORS')]

names = [b[0] for b in BUTTONS]
assert len(names) == len(set(names)), 'two buttons share a name'
headers = [b[1] for b in BUTTONS]
assert len(headers) == len(set(headers)), 'two buttons share a header'
for h in headers:
    assert 1 <= len(h) <= 8 and h == h.strip(), 'bad header %r' % h

# ---- mnemonics: the one character each button writes into a movie -------
# A key whose cap is one character writes that character, except the two a
# movie cannot hold ('.' and '|'). The rest are spelled out here; a button
# with neither stops the build.
MNEMONICS = {
    'Key STOP': 'S', 'Key COPY': 'C', 'Key f.10': '0',
    'Key vf.1': '1', 'Key vf.2': '2', 'Key vf.3': '3', 'Key vf.4': '4', 'Key vf.5': '5',
    'Key ESC': 'e', 'Key Yen': '\\', 'Key BS': 'b',
    'Key ROLL UP': 'u', 'Key ROLL DOWN': 'd', 'Key HOME CLR': 'h', 'Key HELP': '?',
    'Key Pad Minus': '-', 'Key Pad Slash': '/', 'Key TAB': 't', 'Key RETURN': 'r',
    'Key INS': 'i', 'Key DEL': 'x', 'Key Pad Asterisk': '*', 'Key CTRL': 'c',
    'Key Up': '^', 'Key Pad Plus': '+', 'Key SHIFT': 's', 'Key Period': 'p',
    'Key Right SHIFT': 's', 'Key Left': '<', 'Key Right': '>', 'Key Pad Equals': '=',
    'Key CAPS': 'C', 'Key KANA': 'k', 'Key GRPH': 'g', 'Key NFER': 'n',
    'Key SPACE': '_', 'Key XFER': 'X', 'Key Down': 'v', 'Key Pad Comma': ',',
    'Key Pad Period': 'p',
    'Mouse Left Button': 'l', 'Mouse Right Button': 'r',
    'Joystick Up': 'U', 'Joystick Down': 'D', 'Joystick Left': 'L', 'Joystick Right': 'R',
    'Joystick Button 1': '1', 'Joystick Button 2': '2',
    'Drive 1 Previous Floppy Disk': '<', 'Drive 1 Next Floppy Disk': '>',
    'Drive 1 Swap Floppy Disk': 'v',
    'Drive 2 Previous Floppy Disk': '{', 'Drive 2 Next Floppy Disk': '}',
    'Drive 2 Swap Floppy Disk': 'w',
}


def bare(name):
    """A control's name without its player: "P1 Joystick Up" -> "Joystick Up"."""
    head, _, rest = name.partition(' ')
    return rest if rest and head[:1] == 'P' and head[1:].isdigit() else name


def mnemonic(name, header):
    for key in (name, bare(name)):
        if key in MNEMONICS:
            return key, MNEMONICS[key]
    if len(header) == 1 and header not in '.|':
        return name, header
    if name.startswith('Key Pad ') and len(header) == 2:
        return name, header[1]
    if name.startswith('Key f.'):
        return name, header[1:]
    raise SystemExit('no mnemonic for the button %r (MNEMONICS in %s)' % (name, __file__))


mnemonics = dict(mnemonic(n, h) for n, h, _ in BUTTONS)
for k, v in mnemonics.items():
    assert len(v) == 1 and 32 < ord(v) < 127 and v not in '.|', (k, v)

AXES = [
    {"name": "Mouse X", "min": -180, "max": 180, "neutral": 0, "header": "MX"},
    {"name": "Mouse Y", "min": -180, "max": 180, "neutral": 0, "header": "MY"},
]
# the order is the driver's NP2_AXIS_*
assert re.search(r'#define NP2_AXIS_MOUSE_X 0', INPUT_H) and re.search(r'#define NP2_AXIS_MOUSE_Y 1', INPUT_H)
assert re.search(r'#define NP2_AXIS_COUNT %d' % len(AXES), INPUT_H)

# ---- settings ------------------------------------------------------------
MACHINE = 'It is part of the machine, so a movie needs the same value.'


def setting(name, display, description, type, default, **extra):
    s = {"name": name, "display": display, "description": description,
         "type": type, "default": default}
    s.update(extra)
    s["sync"] = True
    return s


# The two machines (docs/PLAN.md section 10): one core.wbx holds both
# builds, and the "machine" setting, read once in Init, picks the half.
MACHINES = [
    ("PC9821", "NEC PC-9801 / PC-9821, IA-32 CPU"),
    ("PC98", "NEC PC-9801, 286 CPU"),
]
IA32 = [MACHINES[0][0]]
# firmware every machine needs (Chimera has no unconditional entry)
ALWAYS = {"setting": "machine", "in": [m for m, _ in MACHINES]}
I286 = [MACHINES[1][0]]

# a list to pick from (owner, 2026-10-24): 1, then the even numbers to 70
CLOCK_MULTIPLIERS = [str(n) for n in [1] + list(range(2, 71, 2))]

SETTINGS = [
    setting("machine", "Machine",
            "Which CPU the PC-98 has. PC9821 is NP2kai's IA-32 build (np21kai), "
            "which runs everything from the PC-9801 to the Windows-era "
            "PC-9821, with hard disks on IDE and a CD-ROM drive. PC98 is its "
            "286 build (np2kai), the older PC-9801 with a 286, hard disks on "
            "SASI and no CD-ROM drive; some old software only runs right on "
            "it. " + MACHINE,
            "enum", MACHINES[0][0], options=[m for m, _ in MACHINES],
            optionLabels={m: label for m, label in MACHINES}),
    setting("model", "Model",
            "Which PC-98 the machine is. VX is a PC-9801VX, the model NP2kai "
            "starts as. VM is the older PC-9801VM. EPSON is an Epson PC-286 "
            "compatible, which some software checks for. The CPU is the "
            "one the Machine setting chose. " + MACHINE,
            "enum", "VX", options=["VX", "VM", "EPSON"]),
    setting("baseClock", "Base Clock",
            "The clock the CPU speed is a multiple of. PC-98 machines run "
            "either from 2.4576 MHz (the 5 and 10 MHz family) or from "
            "1.9968 MHz (the 8 MHz family). Some old software times itself "
            "by it. " + MACHINE,
            "enum", BASE_CLOCKS[0], options=BASE_CLOCKS),
    setting("clockMultiplier", "CPU Clock Multiplier",
            "The CPU runs at the base clock times this number. The default "
            "of 20 gives 49.152 MHz. A higher number makes the CPU faster "
            "and the emulator slower. " + MACHINE,
            "enum", "20", options=CLOCK_MULTIPLIERS),
    setting("extraMemoryMB", "Extended Memory (MB)",
            "Memory above the first megabyte, in MB. The default is 13. "
            "Some programs refuse to start with too little, and a few old "
            "ones with too much. " + MACHINE,
            "int", 13, min=0, max=230, when=IA32),
    # the 286 addresses 16 MB, and NP2kai's 286 build stops at 13 (pccore.c)
    setting("extraMemoryMB", "Extended Memory (MB)",
            "Memory above the first megabyte, in MB, up to the 13 a 286 "
            "PC-9801 can have. The default is 13. Some programs refuse to "
            "start with too little, and a few old ones with too much. " + MACHINE,
            "int", 13, min=0, max=13, when=I286),
    setting("soundBoard", "Sound Board",
            "Which sound board is fitted. The PC-9801-86 is the most common "
            "and the default; it has FM, SSG, rhythm and PCM sound. Games "
            "usually say which boards they support. " + MACHINE,
            "enum", SOUND_BOARDS[0], options=SOUND_BOARDS),
    setting("soundGenerator", "Sound Generator",
            "Which code makes the sound of the FM sound chips. NP2 is NP2kai's "
            "own; fmgen is the YM2608 emulator NP2kai's builds start with. "
            "They differ in the balance: fmgen's SSG (most games' sound "
            "effects) is about 6 dB louder next to the FM music. Only the "
            "sound changes, never what the machine does, so a movie plays "
            "back the same with either.",
            "enum", SOUND_GENERATORS[0], options=SOUND_GENERATORS),
    setting("monitor", "Monitor",
            "The display mode the frame rate is counted in. One frame is one "
            "picture of the PC-98's display, and its rate depends on the "
            "mode: 24khz is the usual 640x400 mode at 56.42 frames a second, "
            "31khz is the PC-9821's 640x400 mode at 70.32, and 31khz480 its "
            "640x480 mode at 60.14. The machine always starts in the 24khz "
            "mode, and software switches mode by itself. Choose the mode the "
            "game runs in, so the movie's times are right. It decides how a "
            "movie's frames are counted, so a movie needs the same value.",
            "enum", MONITORS[0], options=MONITORS),
    setting("startTime", "Start Time",
            "The date and time on the machine's calendar clock when it is "
            "switched on, in UTC. The clock then runs with the emulated time, "
            "not with the time on your computer. " + MACHINE,
            "datetime", 788918400),
    setting("fontFile", "Font File",
            "Which file holds the PC-98's characters: FONT.ROM, a dump of the "
            "character ROM, or FONT.BMP, the picture form some emulators "
            "use. One of them is needed: without it the machine shows no "
            "text. " + MACHINE,
            "enum", "FONT.ROM", options=["FONT.ROM", "FONT.BMP"]),
]

# ---- firmware ------------------------------------------------------------
# No entry pins a hash (owner, 2026-10-25): PC-98 dumps differ from machine to
# machine, and any dump of the right size is taken and recorded by its own
# hash in the project, so a movie still names the files it ran on. The name
# hints let the wizard find the names other emulators use (SNDBIOS.ROM,
# IDEBIOS.ROM, bios9821.rom, FONT.DAT); the dumps known to work are listed in
# docs/firmware-dumps.md.
RHYTHM = [("BD", "bass drum"), ("SD", "snare drum"), ("TOP", "top cymbal"),
          ("HH", "hi-hat"), ("TOM", "tom-tom"), ("RIM", "rim shot")]
FIRMWARE = [
    {"id": "FONT.ROM", "display": "PC-98 font ROM",
     "description": "The character ROM of an NEC PC-98 you own. It holds the "
                    "8x8 and 8x16 characters and the 16x16 kanji.",
     "size": 288768, "name": "FONT.ROM", "nameHints": ["font"],
     "label": "NEC PC-98 FONT.ROM",
     "requiredWhen": {"setting": "fontFile", "is": "FONT.ROM"}},
    {"id": "FONT.BMP", "display": "PC-98 font picture",
     "description": "The PC-98 character set as a picture (FONT.BMP), the "
                    "form NP2 and other emulators write from a font ROM.",
     # not "font": that would take a FONT.ROM for the picture
     "size": 0, "name": "FONT.BMP", "nameHints": ["font.bmp"],
     "requiredWhen": {"setting": "fontFile", "is": "FONT.BMP"}},
    {"id": "bios.rom", "display": "PC-98 BIOS",
     "description": "The BIOS of a PC-98 you own (96 KiB, E8000h to FFFFFh).",
     "size": 98304, "name": "BIOS.ROM", "nameHints": ["bios"],
     "requiredWhen": ALWAYS},
    {"id": "itf.rom", "display": "PC-98 initial test firmware",
     "description": "The start-up test program (ITF) of the same PC-98 as the "
                    "BIOS (32 KiB, F8000h).",
     "size": 32768, "name": "ITF.ROM", "nameHints": ["itf"],
     "requiredWhen": ALWAYS},
    {"id": "sound.rom", "display": "PC-98 sound BIOS",
     "description": "The BIOS (16 KiB) of a PC-9801-26K or PC-9801-86 sound "
                    "board you own.",
     "size": 16384, "name": "SOUND.ROM", "nameHints": ["sound", "sndbios"],
     "requiredWhen": ALWAYS},
    {"id": "ide.rom", "display": "PC-9821 IDE BIOS",
     "description": "The IDE BIOS (8 KiB, D8000h) of a PC-9821 you own.",
     "size": 8192, "name": "IDE.ROM", "nameHints": ["ide", "bios9821"],
     # only the PC-9821 machine has the IDE interface it sits on, and the
     # real IDE BIOS waits for ever on a bus with no hard disk, so it is
     # needed (and used) when the project has one
     "requiredWhen": {"all": [{"setting": "machine", "is": IA32[0]}, {"slot": "hdd"}]}},
] + [
    # fmgen opens these, in capitals (sound/fmgen/fmgen_opna.cpp)
    {"id": "2608_%s.WAV" % s, "display": "PC-98 rhythm sample (%s)" % what,
     "description": "One of the six drum samples inside the sound chip (YM2608) "
                    "of a PC-9801-86 sound board, as the WAV files that PC-98 "
                    "emulators share. Without them the board's drum channel is "
                    "silent.",
     "size": 0, "name": "2608_%s.WAV" % s, "nameHints": ["2608_%s." % s.lower()],
     "label": "YM2608 %s" % what,
     "requiredWhen": ALWAYS}
    for s, what in RHYTHM
]
setting_names = {s["name"]: s for s in SETTINGS}
for f in FIRMWARE:
    for rw in f.get("requiredWhen", {}).get("all", [f["requiredWhen"]] if "requiredWhen" in f else []):
        if "slot" in rw:
            continue
        s = setting_names[rw["setting"]]
        for v in rw.get("in", [rw.get("is")]):
            assert v == s.get("default") or v in s.get("options", [True, False]), f["id"]
# every machine is left with one declaration of each setting
for m, _ in MACHINES:
    mine = [d["name"] for d in SETTINGS if m in d.get("when", [m])]
    assert len(mine) == len(set(mine)), m

# ---- the frame rate, as the driver works it out (np2-driver.c frame_rate) --
def rate(monitor, base=2457600):
    from math import gcd
    for row in table(SETTINGS_H, 'NP2_MONITORS'):
        if row[0] == monitor:
            clock = eval(row[1].replace('/', '//'))
            hclock = clock // int(row[2])
            den = base * int(row[3]) // hclock
            g = gcd(base, den)
            return base // g, den // g
    raise SystemExit(monitor)


VSYNC = rate(MONITORS[0])

EXTENSIONS = {e: MACHINES[0][0] for e in (
    ".d88", ".88d", ".d98", ".98d", ".fdi", ".hdm", ".xdf", ".dup", ".2hd",
    ".tfd", ".nfd", ".dcp", ".dcu", ".hdi", ".nhd", ".thd", ".vhd")}

CONFIG = {
    "coreName": "NP2kai",
    "systemId": MACHINES[0][0],
    "systemNames": {m: label for m, label in MACHINES},
    "author": "NP2 developer team, Neko Project II kai (AZO and contributors); chimera port",
    "url": "https://github.com/ShinobiWannabe/chimera-core-np2kai",
    "romFile": "rom",
    "deterministic": True,
    "memoryLayoutMiB": [256, 16, 16, 16, 1024],
    "video": {
        "_comment": "the BUFFER capacity (np2-driver.h); the live size comes from GetVideoWidth/Height every frame. The rate here is the default Monitor's; the core reports the chosen one from GetVsyncNumerator/Denominator.",
        "width": 1600, "height": 1200,
        "virtualWidth": 640, "virtualHeight": 480,
        "vsyncNumerator": VSYNC[0], "vsyncDenominator": VSYNC[1],
        "getBgra": "GetVideoBgra",
    },
    "audio": {"samplesPerFrame": 22050, "channels": 2, "rate": 44100, "get": "GetAudio"},
    "input": {
        "name": "PC-98 Controller",
        "_comment": "index order is the wire format (waterbox/np2-input.h): the PC-98 keyboard row by row as it sits on the keyboard, then the number pad together, then the mouse, the joystick and the floppy drives. Wider than 64, so everything rides the SetButton channel.",
        "buttons": names,
        "mnemonics": mnemonics,
        "headers": {n: h for n, h, _ in BUTTONS if len(h) > 1},
        "axes": AXES,
        "picture": "pictures/keyboard.png",
        "_axes_note": "The mouse's movement this frame, in the bus mouse's counts. NP2kai's mouse is relative only (user-decided, 2026-10-23).",
    },
    # a disk opened with no project goes to the IA-32 machine, which runs
    # nearly everything the 286 one does
    "extensions": EXTENSIONS,
    "machineSetting": "machine",
    "machines": [
        {"id": MACHINES[0][0], "label": MACHINES[0][1], "when": IA32, "extensions": EXTENSIONS},
        {"id": MACHINES[1][0], "label": MACHINES[1][1], "when": I286},
    ],
    "settings": SETTINGS,
    "firmware": FIRMWARE,
}

with open(os.path.join(WB, 'waterbox.config'), 'w', encoding='utf-8') as f:
    json.dump(CONFIG, f, indent=2, ensure_ascii=False)
    f.write('\n')
print('waterbox.config written: %d buttons, %d settings, %d firmware, vsync %d/%d'
      % (len(names), len(SETTINGS), len(FIRMWARE), VSYNC[0], VSYNC[1]))

# ---- default_keybinds.json -----------------------------------------------
# The keyboard as NP2kai's own SDL2 build maps a PC keyboard (sdl/kbtrans.c,
# the SDL_SCANCODE table): Pause is STOP, PrintScreen COPY, the backtick Yen,
# Equals the caret, Page Up and Page Down ROLL UP and ROLL DOWN, End HELP.
# Except the modifiers: a default setup sends either Shift as Shift, either
# Ctrl as Ctrl and either Alt as Alt (chimera docs/new-core.md), so the
# right-hand Ctrl and Alt the SDL build gives GRPH and XFER could never
# reach them. GRPH is Alt instead, and NFER and XFER are the Japanese
# keyboard's own keys (muhenkan and henkan). STOP is Shift+Pause and vf.1
# and vf.2 Ctrl+F11 and Ctrl+F12, clear of Chimera's Pause and Screenshot
# hotkeys.
# KANA ships unbound too: the frontend sends the kana key as "HangulMode",
# a name nobody would look for. vf.3-vf.5, the underscore and keypad = and ,
# have no PC key.
HOST = {
    'Key STOP': 'Shift+Pause', 'Key COPY': 'PrintScreen',
    'Key ESC': 'Escape', 'Key Minus': 'Minus', 'Key Caret': 'Equals', 'Key Yen': 'Backtick',
    'Key BS': 'Backspace', 'Key ROLL UP': 'PageUp', 'Key ROLL DOWN': 'PageDown',
    'Key HOME CLR': 'Home', 'Key HELP': 'End', 'Key Pad Minus': 'KeypadSubtract',
    'Key Pad Slash': 'KeypadDivide', 'Key TAB': 'Tab', 'Key At': 'LeftBracket',
    'Key Left Bracket': 'RightBracket', 'Key RETURN': 'Enter, KeypadEnter', 'Key INS': 'Insert',
    'Key DEL': 'Delete', 'Key Pad Asterisk': 'KeypadMultiply', 'Key CTRL': 'Ctrl, LeftCtrl',
    'Key Semicolon': 'Semicolon', 'Key Colon': 'Apostrophe', 'Key Right Bracket': 'Backslash',
    'Key Up': 'Up', 'Key Pad Plus': 'KeypadAdd', 'Key SHIFT': 'Shift, LeftShift',
    'Key Comma': 'Comma', 'Key Period': 'Period', 'Key Slash': 'Slash',
    'Key Right SHIFT': 'RightShift', 'Key Left': 'Left', 'Key Right': 'Right',
    'Key CAPS': 'CapsLock', 'Key GRPH': 'Alt, LeftAlt',
    'Key NFER': 'ImeNonConvert',
    'Key SPACE': 'Space', 'Key XFER': 'ImeConvert', 'Key Down': 'Down',
    'Key vf.1': 'Ctrl+F11', 'Key vf.2': 'Ctrl+F12',
    'Key Pad Period': 'KeypadDecimal',
    'Mouse Left Button': 'WMouse L', 'Mouse Right Button': 'WMouse R',
    # the arrows are the keyboard's, so the stick is on the game pad only
    'P1 Joystick Up': 'J1 POV0U, X1 DpadUp, X1 LStickUp',
    'P1 Joystick Down': 'J1 POV0D, X1 DpadDown, X1 LStickDown',
    'P1 Joystick Left': 'J1 POV0L, X1 DpadLeft, X1 LStickLeft',
    'P1 Joystick Right': 'J1 POV0R, X1 DpadRight, X1 LStickRight',
    'P1 Joystick Button 1': 'J1 B1, X1 A', 'P1 Joystick Button 2': 'J1 B2, X1 B',
}
for i in range(1, 11):
    HOST['Key f.%d' % i] = 'F%d' % i
for d in '1234567890':
    HOST['Key %s' % d] = 'Number%s' % d
    HOST['Key Pad %s' % d] = 'Keypad%s' % d
for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
    HOST['Key %s' % c] = c
unknown = set(HOST) - set(names)
assert not unknown, unknown
binds = {n: HOST.get(n, '') for n in names}
used = [k.strip() for v in binds.values() for k in v.split(',') if k.strip()]
assert len(used) == len(set(used)), 'a host key is bound twice'

KEYBINDS = {
    "_comment": [
        "Default bindings for the PC-98: the keyboard as NP2kai's SDL2 build maps",
        "a PC keyboard (sdl/kbtrans.c), except GRPH on Alt and NFER and XFER on a",
        "Japanese keyboard's own keys; the mouse on the host mouse, the joystick",
        "on the game pad. KANA, vf.3-vf.5, the underscore, keypad = and , and the",
        "floppy controls ship unbound. Written by tools/gen-config.py.",
    ],
    "AllTrollers": {"PC-98 Controller": binds},
    "AllTrollersAutoFire": {"PC-98 Controller": {}},
    "AllTrollersAnalog": {"PC-98 Controller": {
        "Mouse X": {"Value": "RMouse X", "Mult": 1.0, "Deadzone": 0.0},
        "Mouse Y": {"Value": "RMouse Y", "Mult": 1.0, "Deadzone": 0.0},
    }},
}
with open(os.path.join(WB, 'default_keybinds.json'), 'w', encoding='utf-8') as f:
    json.dump(KEYBINDS, f, indent=2)
    f.write('\n')
print('default_keybinds.json written: %d of %d buttons bound'
      % (sum(1 for v in binds.values() if v), len(names)))
