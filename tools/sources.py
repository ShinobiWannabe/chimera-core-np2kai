#!/usr/bin/env python3
"""Print the NP2kai source files this core compiles, one per line.

The list is taken from upstream's own sdl/Makefile.common (the libretro and
SDL builds share it), so it follows upstream when the pin moves. From it we
leave out what the licence build A forbids (docs/PLAN.md section 2) and what
section 3 says the sandbox cannot have: the platform layer (sdl/, replaced
by waterbox/port/), the on-screen menu, host folders, the network card,
threads, host tick counters, MIDI synthesis and the GPL parts.

Usage: sources.py <NP2kai tree> <np2|np21>
"""
import glob, os, re, sys

tree, flavour = sys.argv[1], sys.argv[2]
mk = open(os.path.join(tree, 'sdl', 'Makefile.common')).read().replace('\\\n', ' ')
mk = mk.replace('$(NP2_PATH)/', '')
mk = re.sub(r'\$\(wildcard\s+([^)]*)\)', r'\1', mk)

def var(name):
    out = []
    for m in re.finditer(r'^\s*' + name + r'\s*[:+]?=\s*(.*)$', mk, re.M):
        out += m.group(1).split()
    return out

items = var('SOURCES_C') + var('SOURCES_CXX')
items += var('NP2_SOURCES_C') if flavour == 'np2' else var('NP21_SOURCES_C') + var('NP21_SOURCES_CXX')

files = []
for it in items:
    it = it.replace('$(NP2_PATH)/', '')
    files += sorted(os.path.relpath(f, tree) for f in glob.glob(os.path.join(tree, it)))

DROP_PREFIX = (
    'sdl/',                    # the platform layer: ours is waterbox/port/
    'embed/',                  # the on-screen menu and its file selector
    'network/',                # the LGY-98 network card
    'sound/mame/',             # MAME's GPL OPL code; USE_MAME_BSD uses ymfm
    'sound/vermouth/',         # MIDI synthesis from patch files on the host
    'i386c/ia32/instructions/fpu/fpemul_dosbox',  # the GPL DOSBox FPUs
)
DROP_EXACT = {
    'generic/hostdrv.c', 'generic/hostdrvs.c', 'generic/hostdrv9x.c',
    'generic/hostdrvnt.c', 'generic/hostdrvwincompat.c',  # host folders
    'generic/softkbd.c',       # the on-screen keyboard
    'np2_thread.c', 'np2_tickcount.c',
    'debugsnapshot.c',         # writes snapshot files to the host
    'sound/sound.c',           # built through waterbox/port/sound-chimera.c
    'io/serial.c', 'io/mouseif.c',  # built through waterbox/port/keyboard-reads.c and mouseif-reads.c
    'bios/bios18.c',           # built through waterbox/port/bios18-reads.c
    'bios/bios.c',             # built through waterbox/port/bios-ide-units.c
    'sound/tms3631c.c',        # built through waterbox/port/tms3631-pitch.c
    'cbus/boardlol.c',         # built through waterbox/port/boardlol-fixed.c
    'sound/opna.c',            # built through waterbox/port/opna-fmgen-caps.c
    # NP21/W rev105/rev106 fixes, fixed copies in waterbox/port/
    'i386c/ia32/instructions/fpu/fpemul_softfloat3.cpp',  # fpu-softfloat3-np21w.cpp
    'sound/adpcmc.c',          # adpcmc-np21w.c
    'sound/adpcmg.c',          # adpcmg-np21w.c
    'vram/maketext.c',         # maketext-np21w.c
    'sound/cs4231c.c',         # cs4231-dma-np21w.c
    # the drums from the YM2608's ROM as well as the WAV files
    'sound/rhythmc.c',         # built through waterbox/port/rhythm-rom.c
    'sound/fmgen/fmgen_file.cpp',  # built through waterbox/port/fmgen-file-rom.cpp
}
# Upstream's CMake build has these and sdl/Makefile.common forgot them; the
# 118 board's ADPCM needs them. They are NP2kai's own code.
EXTRA = ['sound/ymzadpcmc.c', 'sound/ymzadpcmg.c']
seen, out = set(), []
for f in files:
    if f in seen or f in DROP_EXACT or f.startswith(DROP_PREFIX):
        continue
    if 'tgui' in os.path.basename(f).lower():
        continue               # the GPL Trident card
    seen.add(f)
    out.append(f)
for f in EXTRA:
    if f not in seen and os.path.exists(os.path.join(tree, f)):
        out.append(f)
print('\n'.join(out))
