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
RHYTHM_SAMPLES = [r[0] for r in table(SETTINGS_H, 'NP2_RHYTHM_SAMPLES')]
FLOPPIES_AT_START = [r[0] for r in table(SETTINGS_H, 'NP2_FLOPPIES_AT_START')]
CPU_TYPES = [r[0] for r in table(SETTINGS_H, 'NP2_CPU_TYPES')]
GDC_TYPES = [r[0] for r in table(SETTINGS_H, 'NP2_GDC_TYPES')]
PCI_BUS = [r[0] for r in table(SETTINGS_H, 'NP2_PCI_BUS')]

names = [b[0] for b in BUTTONS]
assert len(names) == len(set(names)), 'two buttons share a name'
headers = [b[1] for b in BUTTONS]
assert len(headers) == len(set(headers)), 'two buttons share a header'
for h in headers:
    assert 1 <= len(h) <= 8 and h == h.strip(), 'bad header %r' % h

# ---- mnemonics: the one character each button writes into a movie -------
# Chimera allows one printable ASCII character, never '.' or '|': 92 of them
# for more buttons than that, so some must share (owner, 2026-10-31: as few
# as can, and only buttons that do the same thing). Each letter and digit
# key writes itself; a key's shifted character on a PC-98 keyboard, or a
# lower-case letter of its name, stands for the rest. TWINS lists the only
# buttons allowed to share, and the build stops on any other pair.
MNEMONICS = {
    # row 1: f.1-f.9 are Shift and 1-9 on a PC-98 keyboard; vf.N shares f.N
    'Key STOP': 'o', 'Key COPY': 'p',
    'Key f.1': '!', 'Key f.2': '"', 'Key f.3': '#', 'Key f.4': '$', 'Key f.5': '%',
    'Key f.6': '&', 'Key f.7': "'", 'Key f.8': '(', 'Key f.9': ')', 'Key f.10': '~',
    'Key vf.1': '!', 'Key vf.2': '"', 'Key vf.3': '#', 'Key vf.4': '$', 'Key vf.5': '%',
    # row 2
    'Key ESC': 'e', 'Key Minus': '-', 'Key Caret': '^', 'Key Yen': '\\', 'Key BS': 'b',
    'Key ROLL UP': 'u', 'Key ROLL DOWN': 'y', 'Key HOME CLR': 'h', 'Key HELP': '?',
    # row 3
    'Key TAB': 't', 'Key At': '@', 'Key Left Bracket': '[', 'Key RETURN': 'r',
    'Key INS': 'i', 'Key DEL': 'd',
    # row 4
    'Key CTRL': 'c', 'Key Semicolon': ';', 'Key Colon': ':', 'Key Right Bracket': ']',
    # row 5
    'Key SHIFT': 's', 'Key Comma': ',', 'Key Period': '`', 'Key Slash': '/',
    'Key Underscore': '_', 'Key Right SHIFT': 's',
    # row 6
    'Key CAPS': 'a', 'Key KANA': 'k', 'Key GRPH': 'g', 'Key NFER': 'n',
    'Key SPACE': 'w', 'Key XFER': 'x',
    # the arrows; the joystick's directions share them
    'Key Up': '{', 'Key Down': '}', 'Key Left': '<', 'Key Right': '>',
    'Joystick Up': '{', 'Joystick Down': '}', 'Joystick Left': '<', 'Joystick Right': '>',
    # the number pad: each key shares the main key it types
    'Key Pad Minus': '-', 'Key Pad Slash': '/', 'Key Pad Asterisk': '*',
    'Key Pad Plus': '+', 'Key Pad Equals': '=', 'Key Pad Comma': ',', 'Key Pad Period': '`',
    # the mouse buttons; the joystick's two buttons share them
    'Mouse Left Button': 'l', 'Mouse Right Button': 'm',
    'Joystick Button 1': 'l', 'Joystick Button 2': 'm',
    # the drives: the same action shares one character in every drive
    'Drive 1 Previous Floppy Disk': 'q', 'Drive 2 Previous Floppy Disk': 'q', 'CD Previous Disc': 'q',
    'Drive 1 Next Floppy Disk': 'j', 'Drive 2 Next Floppy Disk': 'j', 'CD Next Disc': 'j',
    'Drive 1 Swap Floppy Disk': 'v', 'Drive 2 Swap Floppy Disk': 'v', 'CD Swap Disc': 'v',
    'Drive 1 Eject Floppy Disk': 'z', 'Drive 2 Eject Floppy Disk': 'z', 'CD Eject Disc': 'z',
    'Reset': 'f',
}
for _d in '0123456789':
    MNEMONICS['Key %s' % _d] = MNEMONICS['Key Pad %s' % _d] = _d
for _c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
    MNEMONICS['Key %s' % _c] = _c

# The buttons that may share a character: a group's buttons do one thing.
TWINS = [
    ['Key f.%d' % n, 'Key vf.%d' % n] for n in range(1, 6)
] + [
    ['Key %s' % d, 'Key Pad %s' % d] for d in '0123456789'
] + [
    ['Key Minus', 'Key Pad Minus'], ['Key Slash', 'Key Pad Slash'],
    ['Key Comma', 'Key Pad Comma'], ['Key Period', 'Key Pad Period'],
    ['Key SHIFT', 'Key Right SHIFT'],
    ['Key Up', 'Joystick Up'], ['Key Down', 'Joystick Down'],
    ['Key Left', 'Joystick Left'], ['Key Right', 'Joystick Right'],
    ['Mouse Left Button', 'Joystick Button 1'], ['Mouse Right Button', 'Joystick Button 2'],
] + [
    ['Drive 1 %s Floppy Disk' % a, 'Drive 2 %s Floppy Disk' % a, 'CD %s Disc' % a]
    for a in ('Previous', 'Next', 'Swap', 'Eject')
]


def bare(name):
    """A control's name without its player: "P1 Joystick Up" -> "Joystick Up"."""
    head, _, rest = name.partition(' ')
    return rest if rest and head[:1] == 'P' and head[1:].isdigit() else name


def mnemonic(name, header):
    for key in (name, bare(name)):
        if key in MNEMONICS:
            return key, MNEMONICS[key]
    raise SystemExit('no mnemonic for the button %r (MNEMONICS in %s)' % (name, __file__))


mnemonics = dict(mnemonic(n, h) for n, h, _ in BUTTONS)
for k, v in mnemonics.items():
    assert len(v) == 1 and 32 < ord(v) < 127 and v not in '.|', (k, v)
sharing = {}
for k, v in mnemonics.items():
    sharing.setdefault(v, []).append(k)
for v, ks in sharing.items():
    if len(ks) > 1:
        assert any(set(ks) <= set(g) for g in TWINS), 'unrelated buttons share %r: %s' % (v, ks)
assert set(MNEMONICS) == set(mnemonics), set(MNEMONICS) ^ set(mnemonics)

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

# a list to pick from (owner, 2026-10-31): every number from 1 to 70
CLOCK_MULTIPLIERS = [str(n) for n in range(1, 71)]

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
            "enum", "20", options=CLOCK_MULTIPLIERS, when=IA32),
    # the 286 machine is for old software, which can time itself by the
    # CPU: it starts at a real PC-9801VX's 10 MHz (2.4576 MHz x4), not at
    # NP2kai's 49 MHz (owner, 2026-10-31)
    setting("clockMultiplier", "CPU Clock Multiplier",
            "The CPU runs at the base clock times this number. The default "
            "of 4 gives 9.8304 MHz, the speed of a real PC-9801VX (the 8 MHz "
            "models are 4 with the 1.9968 MHz base clock, the 12 MHz ones 5). "
            "Old software that times itself by the CPU runs too fast when it "
            "is higher. " + MACHINE,
            "enum", "4", options=CLOCK_MULTIPLIERS, when=I286),
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
    setting("rhythmSamples", "Rhythm Samples",
            "Where the drums of the sound chip (YM2608) come from. WAV files, "
            "the default, are the six 2608_*.WAV recordings PC-98 emulators "
            "share. YM2608 ROM is the chip's own drum ROM, as MAME dumps it "
            "(ym2608_adpcm_rom.bin, in MAME's ym2608.zip), played as the chip "
            "plays them: the same drums, without the noise and ringing at "
            "the end of the recordings, and within about 1.5 dB of their "
            "loudness. Only the sound changes, never what the machine does, "
            "so a movie plays back the same with either.",
            "enum", RHYTHM_SAMPLES[0], options=RHYTHM_SAMPLES),
    setting("monitor", "Monitor",
            "The display mode the frame rate is counted in. One frame is one "
            "picture of the PC-98's display, and its rate depends on the "
            "mode: 24khz is the usual 640x400 mode at 56.42 frames a second, "
            "31khz is the PC-9821's 640x400 mode at 70.32, 31khz480 its "
            "640x480 mode at 60.14, and 15khz the 640x200 mode of the first "
            "PC-9801s, at 61.46. The machine starts in the 24khz mode, or in "
            "the 15khz one when 15khz is chosen (it sets DIP switch 1-1, which "
            "tells the machine a 15 kHz monitor is fitted), and software "
            "switches mode by itself. Choose the mode the game runs in, so "
            "the movie's times are right. It decides how a movie's frames "
            "are counted, so a movie needs the same value.",
            "enum", MONITORS[0], options=MONITORS),
    setting("floppiesAtStart", "Floppies at Start",
            "Which drives have a floppy in them when the machine is switched "
            "on. Drives 1 and 2, the default, get the first and second "
            "floppies of the project. The PC-98 boots a formatted floppy in "
            "either drive before the hard disk, so to boot the hard disk "
            "with floppies listed, choose Drive 1 only (when the first "
            "floppy does not boot) or None. An empty drive takes the floppy "
            "its selector is on (the first for drive 1, the second for "
            "drive 2) when its Swap input is pressed, and the Eject inputs "
            "empty a drive at any time.",
            "enum", FLOPPIES_AT_START[0], options=FLOPPIES_AT_START),
    setting("startTime", "Start Time",
            "The date and time the machine's calendar clock shows when it is "
            "switched on, exactly as written here. Chimera keeps every date "
            "in UTC, but a PC-98's clock has no time zone: it holds the local "
            "time it was set to, so write the time you want the machine to "
            "show, with no conversion. The clock then runs with the emulated "
            "time, not with the time on your computer. " + MACHINE,
            "datetime", 788918400),
    setting("cpuType", "CPU Type",
            "Which CPU the IA-32 machine reports to software that asks (the "
            "CPUID instruction): its maker, family and the features it has. "
            "The default, Intel Pentium III, is the CPU NP2kai starts as. "
            "Windows and some PC-9821 software check it, and a few programs "
            "refuse a CPU they do not know or crash on features a real "
            "PC-9821 never had; an older choice hides the newer features. "
            "Neko Processor II is NP2kai's own, with every feature the "
            "emulator has. " + MACHINE,
            "enum", CPU_TYPES[0], options=CPU_TYPES, when=IA32),
    setting("dipSwitches", "DIP Switches",
            "The three banks of DIP switches on the back of the machine, as "
            "three bytes in hexadecimal, bank 1 first (NP2kai writes them the "
            "same way in its np2.cfg). The default, 3E E3 7B, is NP2kai's. "
            "They tell the BIOS and software things such as the monitor, the "
            "number of floppy drives and the memory fitted. Switch 1-1 (the "
            "lowest bit of the first byte) is set by the Monitor setting, and "
            "switch 3-8 (the top bit of the third byte) starts the 286 "
            "machine's CPU as a V30. A value that is not three bytes is "
            "refused, and the default is used. " + MACHINE,
            "string", "3E E3 7B"),
    setting("memorySwitches", "Memory Switches",
            "The eight memory switches the PC-98 keeps in its battery-backed "
            "memory, as eight bytes in hexadecimal (NP2kai writes them the "
            "same way in its np2.cfg). The default, 48 05 04 08 01 00 00 6E, "
            "is NP2kai's. They hold settings MS-DOS's SWITCH command changes, "
            "such as the printer, the RS-232C line and the text screen. "
            "Software that changes them changes them in the machine, and "
            "they go back to this value when the machine is switched on or "
            "reset. A value that is not eight bytes is refused, and the "
            "default is used. " + MACHINE,
            "string", "48 05 04 08 01 00 00 6E"),
    setting("gdcType", "GDC Type",
            "Which graphics display controller chip the machine has. The "
            "uPD7220, the default, is the earlier chip; the uPD72020 of later "
            "machines draws some graphics modes differently, which a few "
            "games rely on. " + MACHINE,
            "enum", GDC_TYPES[0], options=GDC_TYPES),
    setting("pegcPlaneMode", "PEGC Plane Mode",
            "Whether the PC-9821's 256-colour graphics (the PEGC) also offer "
            "their plane mode, which a few programs use instead of the usual "
            "packed mode. Off by default, as in NP2kai. " + MACHINE,
            "bool", False, when=IA32),
    setting("pciBus", "PCI Bus",
            "Whether the IA-32 machine has a PCI bus, as the later PC-9821 "
            "models do, and whether its BIOS offers the BIOS32 service PCI "
            "software looks for. Off by default, as in NP2kai. Windows 95 "
            "and later use it to find PCI devices. " + MACHINE,
            "enum", PCI_BUS[0], options=PCI_BUS, when=IA32),
    setting("floppySeekSounds", "Floppy Seek Sounds",
            "Plays the sound of the floppy drive's head moving, as NP2kai "
            "can. It also makes a seek take as long as a real drive's head "
            "takes to move, which NP2kai ties to the sound, so it changes "
            "how long disk access takes. Off by default, as in NP2kai. " + MACHINE,
            "bool", False),
    setting("volumeMaster", "Master Volume",
            "Every sound of the machine together, in per cent. The default, 100, is NP2kai's (out of 100). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 100, min=0, max=100),
    setting("volumeFM", "FM Volume",
            "The FM music of the sound board's FM chip (and of OPL chips). The default, 64, is NP2kai's (out of 128). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 64, min=0, max=128),
    setting("volumeSSG", "SSG Volume",
            "The SSG of the FM chip, which most games use for sound effects. The default, 64, is NP2kai's (out of 128). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 64, min=0, max=128),
    setting("volumeADPCM", "ADPCM Volume",
            "The ADPCM sample channel of the PC-9801-86 and similar boards. The default, 64, is NP2kai's (out of 128). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 64, min=0, max=128),
    setting("volumePCM", "PCM Volume",
            "The PCM sample playback of the PC-9801-86 board. The default, 64, is NP2kai's (out of 128). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 64, min=0, max=128),
    setting("volumeRhythm", "Rhythm Volume",
            "The drums of the FM chip (YM2608 rhythm). The default, 64, is NP2kai's (out of 128). Only the sound changes, "
            "never what the machine does, so a movie plays back the same "
            "with any value.",
            "int", 64, min=0, max=128),
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
    {"id": "font.rom", "display": "PC-98 font ROM",
     "description": "The character ROM of an NEC PC-98 you own. It holds the "
                    "8x8 and 8x16 characters and the 16x16 kanji.",
     "size": 288768, "name": "font.rom", "nameHints": ["font"],
     "label": "NEC PC-98 FONT.ROM",
     "requiredWhen": {"setting": "fontFile", "is": "FONT.ROM"}},
    {"id": "font.bmp", "display": "PC-98 font picture",
     "description": "The PC-98 character set as a picture (FONT.BMP), the "
                    "form NP2 and other emulators write from a font ROM.",
     # not "font": that would take a FONT.ROM for the picture
     "size": 0, "name": "font.bmp", "nameHints": ["font.bmp"],
     "requiredWhen": {"setting": "fontFile", "is": "FONT.BMP"}},
    {"id": "bios.rom", "display": "PC-98 BIOS",
     "description": "The BIOS of a PC-98 you own (96 KiB, E8000h to FFFFFh).",
     "size": 98304, "name": "bios.rom", "nameHints": ["bios"],
     "requiredWhen": ALWAYS},
    # no itf.rom (owner, 2026-10-26): NP2kai is built with BIOS_SIMULATE,
    # which always puts its own ITF at F8000h and never opens the file
    # (bios/bios.c)
    {"id": "sound.rom", "display": "PC-98 sound BIOS",
     "description": "The BIOS (16 KiB) of a PC-9801-26K or PC-9801-86 sound "
                    "board you own.",
     "size": 16384, "name": "sound.rom", "nameHints": ["sound", "sndbios"],
     "requiredWhen": ALWAYS},
    {"id": "ide.rom", "display": "PC-9821 IDE BIOS",
     "description": "The IDE BIOS (8 KiB, D8000h) of a PC-9821 you own.",
     "size": 8192, "name": "ide.rom", "nameHints": ["ide", "bios9821"],
     # only the PC-9821 machine has the IDE interface it sits on, and the
     # real IDE BIOS waits for ever on a bus with no hard disk, so it is
     # needed (and used) when the project has one
     "requiredWhen": {"all": [{"setting": "machine", "is": IA32[0]}, {"slot": "hdd"}]}},
] + [
    {"id": "2608_%s.wav" % s.lower(), "display": "PC-98 rhythm sample (%s)" % what,
     "description": "One of the six drum samples inside the sound chip (YM2608) "
                    "of a PC-9801-86 sound board, as the WAV files that PC-98 "
                    "emulators share. Without them the board's drum channel is "
                    "silent.",
     "size": 0, "name": "2608_%s.wav" % s.lower(), "nameHints": ["2608_%s." % s.lower()],
     "label": "YM2608 %s" % what,
     "requiredWhen": {"setting": "rhythmSamples", "is": RHYTHM_SAMPLES[0]}}
    for s, what in RHYTHM
] + [
    {"id": "ym2608_adpcm_rom.bin", "display": "YM2608 rhythm ROM",
     "description": "The drum ROM inside the sound chip (YM2608) of a "
                    "PC-9801-86 sound board, as MAME dumps it (8 KiB, in "
                    "MAME's ym2608.zip). Used in place of the six 2608_*.WAV "
                    "files when Rhythm Samples is YM2608 ROM.",
     "size": 8192, "name": "ym2608_adpcm_rom.bin", "nameHints": ["ym2608_adpcm"],
     "label": "YM2608 rhythm ROM",
     "requiredWhen": {"setting": "rhythmSamples", "is": RHYTHM_SAMPLES[1]}},
]
# Every id in small letters, and the same list as the core's (np2-firmware.h):
# the core finds a file whatever the case NP2kai asks in (port/dosio.c).
FIRMWARE_FILES = [r[0] for r in table(read('np2-firmware.h'), 'NP2_FIRMWARE_FILES')]
assert [f["id"] for f in FIRMWARE] == FIRMWARE_FILES, 'waterbox/np2-firmware.h'
assert all(f["id"] == f["id"].lower() and f["name"] == f["id"] for f in FIRMWARE)
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

# The files that start this core when opened with no project. .ima is left
# out: the DOSBox-X and PCem cores claim it for IBM PC floppies (the floppy
# slot still takes it). So is .hdd, a raw hard disk to the DOSBox-X core (the
# SCSI slot still takes it).
EXTENSIONS = {e: MACHINES[0][0] for e in (
    ".d88", ".88d", ".d98", ".98d", ".fdi", ".hdm", ".xdf", ".dup", ".2hd",
    ".tfd", ".nfd", ".dcp", ".dcu", ".fdd", ".hd4", ".hd5", ".hd9", ".h01",
    ".hdb", ".ddb", ".dd6", ".dd9", ".flp", ".fim", ".dip",
    ".hdi", ".nhd", ".thd", ".vhd", ".slh", ".hdn", ".hds")}

CONFIG = {
    "coreName": "NP2kai",
    "systemId": MACHINES[0][0],
    "systemNames": {m: label for m, label in MACHINES},
    "author": "NP2 developer team, Neko Project II kai (AZO and contributors); Chimera port by ShinobiWannabe and contributors",
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
# KANA is Ctrl+ScrollLock (owner, 2026-10-31: a combination of its own). The
# frontend sends a Japanese keyboard's kana key as "HangulMode", a name
# nobody would look for, and Scroll Lock is bound to nothing else and is no
# Chimera hotkey, alone or with Ctrl. KANA locks (np2-driver.c), so a tap
# of the combination turns kana on and Ctrl is let go before typing.
# vf.3-vf.5, the underscore and keypad = and , have no PC key.
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
    'Key vf.1': 'Ctrl+F11', 'Key vf.2': 'Ctrl+F12', 'Key KANA': 'Ctrl+ScrollLock',
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
        "on the game pad; KANA on Ctrl+ScrollLock. vf.3-vf.5, the underscore,",
        "keypad = and , the drive controls and Reset ship unbound. Written by",
        "tools/gen-config.py.",
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
