# Neko Project II Kai as a Chimera core: plan

Written 2026-10-23. This is the planning step (call it M0): the questions a
new core has to answer before any code, answered from the sources, and the
order of the work after that, with the reason for each step. Nothing has been
built. No core code exists yet, and this repository has no commits.

**A third-party core (user-decided, 2026-10-23).** This core is made and
published by its owner (the person who asked for it), not by the Chimera
project, and it is not expected to join Chimera's official cores. "The owner"
below means the owner of this core. Chimera's own guides are still followed
for how a core is built and tested, because a package has to meet the same
contract to load. What changes is publishing (section 15) and who decides:
anything about Chimera itself (its design, its roster, its repositories) is
not this project's to change.

Two words are used with a fixed meaning throughout:

- **READ** means the claim comes from a file that was opened and read for this
  plan. The file is named.
- **UNCHECKED** means the claim follows from what was read but was not built,
  run or measured. Every UNCHECKED claim has a step below that checks it.

## What was read

| Source | Version |
|---|---|
| NP2kai (upstream, `G:\GitHub\NP2kai`, https://github.com/AZO234/NP2kai) | branch `wx_alpha` (the default branch), commit `5939e0c6d598`, 2026-09-05, `git describe`: `rev.22-271-g5939e0c` |
| Chimera (`G:\GitHub\chimera-main`) | commit `7bc0d35`, 2026-10-22 |
| Chimera's guides | `AGENTS.md`, `docs/new-core.md`, `docs/porting-a-core.md`, `docs/gates.md`, `docs/waterbox-analysis.md`, `docs/multi-file.md`, `docs/save-data.md`, `extern/chimera-common-minibox/docs/MACHINE-SPEC.md` |
| The closest cores | `chimera-core-dosbox-x` (`AGENTS.md`, `docs/PLAN.md`, `waterbox/waterbox.config`, `file_slots.json`, `package-licenses.json`), `chimera-core-pcem` (`docs/PLAN.md`, the "why another PC core" reasoning), `chimera-core-x68k` (`docs/PLAN.md`, a Japanese computer) |
| libretro's page for this core | the saved copy in the project files (firmware list, core options, 56.4 fps, 44100 Hz) |

In NP2kai the files that matter most were `pccore.c` (the frame loop),
`timing.c`, `calendar.c`, `np2_tickcount.c`, `sdl/timemng.c`, `sdl/dosio.c`
(the file layer), `bios/bios.c`, `font/font.c`, `sdl/libretro/libretro.c`,
`sdl/libretro/libretro_params.h`, `sdl/Makefile.common`,
`sdl/Makefile.libretro`, `LICENSE` and everything under `LICENSES/`.

## The finding first

NP2kai looks like a good fit for Chimera's sandbox, and better than most PC
emulators, for five reasons (all READ):

1. **A frame already exists.** `pccore_exec()` (`pccore.c:1875`) runs the
   machine until its next vertical sync (`nevent_set(NEVENT_FLAMES,
   gdc.dispclock, screenvsync, ...)`) and returns. libretro's `retro_run()`
   calls it once per frame. Chimera's `FrameAdvance` can be the same single
   call.
2. **Time is already the machine's own.** Inside `pccore_exec()` time is
   counted in emulated CPU clocks by the event queue (`nevent.c`). The host's
   clock reaches the machine in a short, listable set of places (section 3),
   and each one has an obvious seam.
3. **It runs with no firmware at all.** When `bios.rom` is missing NP2kai
   copies its own compatible BIOS into memory (`bios/bios.c:449-480`,
   `BIOS_SIMULATE`). So a test machine can boot on a public CI runner with
   nothing copyrighted, from a boot disk the test script builds. That makes
   this core one of the "the machine is its own content" cores, like
   DOSBox-X (UNCHECKED until M1 boots such a disk).
4. **No JIT and no GPU.** The CPU cores (`i286c/`, `i386c/`) are
   interpreters. The only accelerated path is Intel HAXM (`i386hax/`), which
   is Windows-only and is left out. The picture is drawn in software into a
   memory buffer (`scrnmng`), which is "way 1" of porting-a-core.md: the
   picture is part of the machine's memory, byte for byte.
5. **The licence is permissive at the top** (MIT for NP2kai's own changes,
   BSD-3 for the NP2 base), with GPL code only in optional parts selected by
   build defines (section 2).

Two things to know before the port starts:

- **Chimera already has PC-98.** The DOSBox-X core offers `pc98`, `pc9801`
  and `pc9821` machine types, reads PC-98 `.hdi` and `.dcp` images, and takes
  the PC-98 font ROM, sound BIOS and rhythm samples as firmware (READ:
  `chimera-core-dosbox-x/waterbox/waterbox.config` and its `docs/PLAN.md`
  section 6 and 8). The owner has decided to build this core whatever the
  overlap (user-decided, 2026-10-23), so this does not stop the work. Step
  M0b still compares the two, for information: its table says what each core
  is good for and supplies the real-game tests.
- **The licence of the package as built.** This is the owner's to decide
  (section 2 and the open questions).

## Open questions for the owner

These are not decided here. Each one is the owner's to decide.

1. **Decided: build it.** The core is built whether or not it overlaps the
   DOSBox-X core's PC-98 mode (user-decided, 2026-10-23). The comparison
   (M0b) is kept for information only.
2. **When the repository is pushed and made public.** As a third-party
   core it lives in the owner's own account: the local clone
   `G:\GitHub\chimera-core-np2kai` already points at
   `https://github.com/ShinobiWannabe/chimera-core-np2kai.git`. When anything
   is committed, pushed or published is the owner's call, asked each time.
   Nothing in this plan does any of it.
3. **Decided: build A, fmgen with no GPL part** (user-decided, 2026-10-23).
   `USE_MAME_BSD`, no `SUPPORT_FPU_DOSBOX`/`DOSBOX2`, no
   `SUPPORT_TRIDENT_TGUI`, the FPU as SoftFloat 3. The package is MIT plus BSD
   plus fmgen's terms (section 2).
4. **Decided: one package, two machines** (user-decided, 2026-10-23).
   NP2kai builds two emulators from one tree: "np2kai" with the 80286/V30 CPU
   (`i286c/`) and "np21kai" with the IA-32 CPU (`i386c/`, PC-9821 machines,
   Windows 95). Both go into one package as two machines the user picks when
   making a project, the IA-32 machine first and the 286 machine added after
   (section 10). Ids decided (user-decided, 2026-10-23): core id `np2kai`,
   machines `PC9821` and `PC98` (section 10).
5. **Decided: the font file is required** (user-decided, 2026-10-23). The
   user supplies `FONT.ROM` or `font.bmp`; no free fallback font is shipped,
   so the package gains no font licence. NP2kai's own fallback, drawing a
   font from one installed on the host (`font/font.c`, `makepc98bmp`), is
   compiled out because the sandbox cannot allow it.
6. **How releases are built** (section 15): call Chimera's shared
   `publish-core.yml` from this repository's workflow, or carry a copy of its
   publishing script here. Deferred (user-decided, 2026-10-23): nothing is
   set up for publishing now; raise it again once the core works well
   (after M10).

Not applicable to a third-party core: a `core-request` issue, a row in
Chimera's `official-cores.json` and README, and the release-notification
credential the official cores share. If the owner later offers the core to
the Chimera project, those come back, and the Chimera project decides.

Nothing in this plan needs Chimera itself to learn anything new. If a step
turns out to need it (for example a frame rate that changes while the core
runs, section 4), the core has to work without it, or the change is proposed
to the Chimera project as an issue; a third-party core cannot change
Chimera.

---

## 1. Is it a new core at all?

**Decided (user-decided, 2026-10-23): yes, it is built regardless.**
`docs/new-core.md` section 1 asks this question first ("A machine that a core
already here can be taught is a machine added to that core"), and the
DOSBox-X core already runs PC-98 software. The owner has answered it: this
core is built whatever the overlap. What follows is kept as information about
how the two differ, not as a condition for the work.

**Where they overlap (READ, from the DOSBox-X core's own documents).**
DOSBox-X's PC-98 mode boots PC-98 MS-DOS programs from its built-in DOS,
reads `.hdi` hard disks and `.dcp` floppies (decoded by the core into flat
images), takes `FONT.ROM`, `SOUND.ROM` and the six `2608_*.wav` rhythm
samples, and was shown running "an eight-disk PC-98 game" to its publisher's
logo and "a PC-98 hard-disk game" to its attract screen. Its plan also
records the limit it hit: booting a PC-98 hard disk's own MS-DOS stopped in
`CONFIG.SYS`, and "booting the disk's own DOS is the open item, and it is
upstream's."

**Where NP2kai is different (READ from NP2kai's sources; how much it
matters is UNCHECKED).**

- It is a PC-98 machine emulator first: real PC-98 BIOS or its own
  compatible one, the GDC graphics chips, EGC, PEGC, the PC-9801-26K/-86/-118
  sound boards through two FM engines (its own `opngen` and cisc's `fmgen`),
  SASI/SCSI/IDE disks, and PC-9821 machines with an IA-32 CPU.
- Most PC-98 games are self-booting floppies, not programs run from DOS. It
  reads the floppy formats PC-98 dumps actually come in, including the
  copy-protection-carrying ones: `.d88`/`.d98`, `.fdi`, `.nfd`, `.dcp`/`.dcu`,
  `.xdf`/`.hdm` and more (`diskimage/fd/`, and `LR_VALIDFILEEXT` in
  `sdl/libretro/libretro_params.h`).
- It is the emulator PC-98 players generally use, so a movie made on it is
  more likely to be checkable against what other people see.

### Benefits of this core

What this core offers that the DOSBox-X core does not, or does not yet
(recorded at the owner's request, 2026-10-23). Each line says where it comes
from.

**Operating systems other than DOS.** The PC-98 ran far more than MS-DOS. In
general (from what is known about the PC-98, not checked here, and not yet
tried on this core):

- ROM BASIC: N88-BASIC(86), in the ROM of most models. On NP2kai it needs the
  user's real `bios.rom`, because NP2kai's own BIOS does not contain BASIC.
- CP/M-86 and Concurrent CP/M-86.
- NEC's PC-98 Windows: 1.0, 2.x, 3.0 and 3.1, Windows 95, Windows 98,
  Windows NT 3.5x and 4.0, and Windows 2000 (the last Windows for the PC-98;
  there is no PC-98 Windows Me or XP).
- OS/2 1.x (NEC editions).
- Unix-like systems: NEC's PC-UX, FreeBSD(98), Linux/98, and a NetBSD port.
- BTRON (the TRON project's 1B and B-right/V).

The 286 machine covers ROM BASIC, CP/M, MS-DOS and Windows up to 3.x. The
IA-32 (PC-9821) machine adds Windows 95 to 2000 and the Unix-like systems.

Evidence that NP2kai is used for these (READ): its README has notes on
Windows 3.1 in 256 colours with the PC-9821 drivers (`README.md:1039`), the
CD drive under MS-DOS 6.2 (`README.md:836`) and an IDE fix for Windows NT 4
and 2000 (`README.md:1382`).

The DOSBox-X side (READ): upstream DOSBox-X's changelog fixes "PC-98 mode
MS-DOS 7/Windows 9x boot disk's 80386 CPU check" (`CHANGELOG:2458`) and a
graphics operation "apparently used by Windows 3.1" (`CHANGELOG:7511`), so it
gets some way into PC-98 Windows. Its README describes its PC-98 mode as being
"so that you can play ... PC-98 games" and promises Windows 95/98/ME for the
IBM PC side only. The Chimera DOSBox-X core records that booting a PC-98 hard
disk's own MS-DOS stops in `CONFIG.SYS` and calls it an open item, and it
leaves its Windows mouse drivers out of PC-98 machines. Nothing found shows
PC-98 Windows 95 running on either.

How much of this the Chimera core can use is limited by speed in the sandbox
(measured in M3): MS-DOS and Windows 3.x should be comfortable; Windows 95
and later, and the Unix-like systems, may be slow.

**Disk image formats.** NP2kai picks a reader by the file's extension (READ:
`diskimage/fddfile.c` `fdd_set`, `fdd/sxsihdd.c`, `fdd/sxsicd.c`,
`diskimage/cddfile.c`). Upstream DOSBox-X was read in
`src/dos/dos_programs.cpp` and `include/bios_disk.h` (at the commit the
Chimera DOSBox-X core pins), and the Chimera DOSBox-X core in its
`waterbox/file_slots.json`.

Floppy disks:

| Format | NP2kai | Upstream DOSBox-X | Chimera DOSBox-X core |
|---|---|---|---|
| D88 (`.d88`) | yes | yes | yes |
| D88 under its other names (`.d98`, `.88d`, `.98d`) | yes | not by those names | no |
| FDI (`.fdi`) | yes | yes | yes |
| NFD (`.nfd`, both revisions) | yes | yes | yes |
| DCP and DCU (`.dcp`, `.dcu`) | yes | no | `.dcp` only (the core decodes it itself) |
| Virtual98 floppy (VFDD) | yes | a VFD reader exists (`imageDiskVFD`); whether it is the same format is UNCHECKED | no |
| BKDSK (BASIC disks, `.hdb` and similar) | yes | no | no |
| Raw images (`.xdf`, `.hdm`, `.dup`, `.2hd`, `.tfd`, `.hd4`, `.hd5`, `.hd9`, `.fdd`, `.h01`, `.ddb`, `.dd6`, `.flp`, `.fim`, `.bin`, `.img`, `.ima`) | yes, typed by size | raw images, typed by size | `.img`, `.ima`, `.xdf`, `.hdm` |

Hard disks:

| Format | NP2kai | Upstream DOSBox-X | Chimera DOSBox-X core |
|---|---|---|---|
| Anex86 (`.hdi`) | yes | yes | yes |
| T98-Next (`.nhd`) | yes | yes | no |
| T98 (`.thd`) | yes | no | no |
| Virtual98 SCSI (`.hdd`) | yes | no (`.hdd` there is a raw image) | as a raw image only |
| SL9821 (`.slh`) | yes | no | no |
| `.hdn`, `.hds` (SCSI images) | yes | no | no |
| VHD (`.vhd`) | yes | yes | no |
| QCOW2 (`.qcow2`) | no | yes | no |
| Raw (`.img`) | through the formats above | yes | yes |

CD-ROMs (PC-9821 machine only):

| Format | NP2kai | Upstream DOSBox-X | Chimera DOSBox-X core |
|---|---|---|---|
| `.iso`, `.cue` with `.bin` | yes | yes | yes |
| CloneCD (`.ccd`) | yes | yes | no |
| CD Manipulator (`.cdm`) | yes | no | no |
| Alcohol (`.mds`) | yes | `.mdf` only | no |
| Nero (`.nrg`) | yes | no | no |
| CHD (`.chd`) | no | yes | no |

So the formats this core adds over the Chimera DOSBox-X core are the D88
aliases, DCU, VFDD, BKDSK, the many raw floppy names, the T98, T98-Next,
Virtual98, SL9821 and SCSI hard disks, VHD, and the CloneCD, CD Manipulator,
Alcohol and Nero CD images. What it lacks that DOSBox-X has is CHD and QCOW2.
These come from which reader each program calls for an extension; whether
every reader works on real dumps is UNCHECKED and is tested in M10.

**M0b, a side-by-side run, for information.** Pick a short list of PC-98
games that people want to TAS (the owner names them), including one or two
CD games (for example Policenauts, which upstream DOSBox-X has a CD audio fix
for), run each on the DOSBox-X core's PC-98 machine and on NP2kai's own SDL or
libretro build, and record for each one: boots or not, reaches play or not,
sound right or not. Write the table into this file. It does not decide
whether the core is built.

The owner's test games (user-decided, 2026-10-23) are in
`G:\GitHub\pc98examplegames`, one folder per game:

| Game | Files | Kind |
|---|---|---|
| Brandish 2 Renewal - The Planet Buster | `.cue` with 12 `.bin` tracks | CD, data and audio tracks |
| Canaan - Yakusoku no Chi | `.cue` with one `.bin` | CD |
| Flame Zapper Kotsujin | `CO2-Pro (Flame Zapper Kotsujin).hdi` | hard disk |
| Policenauts | `.cue` with 6 `.bin` tracks | CD, data and audio tracks |
| Popful Mail (Falcom, 1992) | three `.fdi` floppies (program, data, opening) and a `.hdi` | floppies and hard disk |
| Rogue (ASCII, 1986) | `Rogue (1986)(Ascii).d88` | floppy |
| Rusty (C-Lab, 1993) | `Rusty (1993)(C-Lab).hdi` | hard disk |
| Windows 98 Second Edition (Japan) | `.cue` with one `.bin` | CD, an operating system |
| Wizardry II Diamond no Kishi (ASCII, 1986) | one `.fdi` | floppy |

What each one tests:

- The floppy and hard disk games (`.fdi`, `.hdi`, `.d88`) use formats both
  cores read, so that comparison is fair. Popful Mail also tests swapping
  floppies during play; Rogue tests the D88 reader.
- Brandish 2 Renewal and Policenauts test the CD drive with CD audio tracks.
  Their cue sheets name one `.bin` file per track, so the core has to mount
  several files for one CD (Chimera's multi-file support, `docs/multi-file.md`).
  UNCHECKED: that NP2kai's cue reader opens several `.bin` files, and that the
  DOSBox-X core's CD slot takes a multi-file cue.
- Canaan tests a single-file CD.
- Set aside for now (user-decided, 2026-10-23): no test uses it until the
  owner brings it back. When it does: Windows 98 SE needs the IA-32 machine (`PC9821`) and a hard disk to install
  onto, so the test makes an empty hard disk image first. It tests speed in
  the sandbox (risk 3) as much as correctness. UNCHECKED: that this disc is
  the PC-98 edition of Windows 98 (the name does not say), and how it boots
  (PC-98 Windows 98 is normally started from a boot floppy). It is not part
  of the DOSBox-X comparison: nothing found shows DOSBox-X running PC-98
  Windows 9x (section 1).

These files are the owner's content (1.6 GB): they are never committed,
uploaded, or copied into the repository or a package. It tells users which core suits which game, and
its games become the gate's real-game tests (M10).

---

## 2. Licence

**Why it matters.** `docs/new-core.md`: "A licence that forbids modified or
binary redistribution ends the request." Every component compiled into the
package is declared in `package-licenses.json` with its terms and source, and
the package's effective terms follow from the most restrictive part
included.

**What `LICENSES/` says (READ; the files are in Japanese and were read in
full).** The top-level `LICENSE` is MIT (Copyright 2017 AZO) for NP2kai's
work. `LICENSES/LICENSE.TXT` is BSD-3-Clause (NP2 developer team) for "source
code not covered by another licence document". The overview file in
`LICENSES/` (its Japanese name means "overall explanation") warns that some
files carry other licences, and compiling them in changes the licence of the
whole.

| Part | Files | Licence | Brought in by |
|---|---|---|---|
| NP2 base | most of the tree | BSD-3-Clause | always |
| NP2kai changes | | MIT | always |
| IA-32 CPU | `i386c/` | BSD-2-Clause (NONAKA Kimihiro) | the IA-32 build |
| IA-32 FPU (own) | `i386c/ia32/instructions/fpu/fp*.c` | BSD-2-Clause | the IA-32 build |
| SIMD instructions | `i386c/.../mmx`, `sse*` | BSD-2-Clause (SimK, Gocaine Project) | the IA-32 build |
| DOSBox FPU | `fpu/fpemul_dosbox*.c` | **GPL-2.0-or-later** | `SUPPORT_FPU_DOSBOX`, `SUPPORT_FPU_DOSBOX2` |
| SoftFloat 2 and 3 | `fpu/softfloat`, `fpu/softfloat3` | Berkeley SoftFloat terms (permissive) | `SUPPORT_FPU_SOFTFLOAT`, `SUPPORT_FPU_SOFTFLOAT3` |
| MAME sound (GPL) | `sound/mame` (OPL3, Y8950 and friends) | **GPL-2.0-or-later** | `USE_MAME` without `USE_MAME_BSD` |
| MAME sound (BSD) | `sound/mamebsd` (Aaron Giles' ymfm) | BSD-3-Clause | `USE_MAME_BSD` |
| fmgen | `sound/fmgen` | cisc's own terms: free to modify and distribute if the origin is named, it stays free software, changes are stated and the text is kept; commercial software needs the author's prior consent | `SUPPORT_FMGEN` |
| Cirrus GD54xx | `wab/cirrus_vga*` | MIT (from QEMU) | `SUPPORT_CL_GD5430` |
| Trident TGUI9680 | `wab/tgui9680*` | **GPL-2.0** | `SUPPORT_TRIDENT_TGUI` |
| GA-1280A | `wab/ga1280a*` | BSD (neetan team) | `SUPPORT_WAB_GA1280A` |
| LGY-98 network card | `network/lgy98*` | MIT (from QEMU) | `SUPPORT_LGY98` (left out anyway: no network) |
| LIO circles | `lio/gcircle.c` part | MIT (msonrm) | always |
| zlib | `zlib` | zlib | if used |
| c86ctl, SCCI | `win9x/ext/...` | BSD, MIT | Windows build only, not used |

**What the libretro build compiles (READ, `sdl/Makefile.common`).** It
defines `USE_MAME` and `USE_MAME_BSD` together. The MAME GPL licence note
says the GPL files are excluded when `USE_MAME_BSD` is defined, so that build
looks free of MAME's GPL code (UNCHECKED: confirm by looking at what the
object files contain). It compiles `fpemul_dosbox.c` and `fpemul_dosbox2.c`
as files, but defines only `SUPPORT_FPU_DOSBOX2_COMPATIBLE` and
`SUPPORT_FPU_SOFTFLOAT3` for the IA-32 build, so whether any GPL FPU code is
actually compiled in is UNCHECKED. It defines `SUPPORT_FMGEN`. It does not
define `SUPPORT_TRIDENT_TGUI`.

**GPL by itself is not the problem.** Chimera already publishes GPL cores
(DOSBox-X and PCem are GPL-2.0): the package is then GPL as a whole, and
`package-licenses.json` links the exact source of every part by commit, which
is what the GPL asks for.

**The conflict is GPL and fmgen in one binary.** fmgen's terms allow only
free distribution and require the author's consent for commercial software.
The GPL forbids adding restrictions beyond its own (GPL-2.0 section 6), and
"no commercial use without consent" is such a restriction. So a binary that
contains both GPL code and fmgen arguably cannot be distributed under the GPL
at all. This is a reading of the licence texts, not legal advice.

**So there are two consistent builds. The owner chose A (user-decided, 2026-10-23).**

- **A. fmgen, no GPL part** (chosen): `USE_MAME_BSD` (ymfm for the OPL
  chips, as the libretro build already does), no `SUPPORT_FPU_DOSBOX`/
  `DOSBOX2`, no `SUPPORT_TRIDENT_TGUI`, the FPU as SoftFloat 3. The package is
  MIT plus BSD plus fmgen's terms. It keeps fmgen's FM sound, and SoftFloat is
  also the better choice for determinism (section 3, item 9).
- **B. GPL parts allowed, no fmgen**: `SUPPORT_FMGEN` off, which leaves NP2's
  own `opngen` FM engine. The package is GPL-2.0-or-later (GPL-2.0 only if the
  Trident card is included, since its file is GPL-2.0).

Write `LICENSE` and `package-licenses.json` only after that answer, naming
exactly the components the build compiles, and check the built objects for
any GPL file that slipped in (M8).

---

## 3. One thread on one clock

**Why it matters.** Every Chimera core is one thread whose time is bought
with executed instructions, so the same movie gives the same machine on any
host, any day. `docs/new-core.md` section 6 lists the ways this has leaked
before: a clock, a thread, a host address, memory nobody wrote, whoever is
looking, order that is not order, and the picture feeding the machine.

**The frame (READ).** `pccore_exec()` sets a vsync event at
`gdc.dispclock` emulated clocks, then loops: interrupts, `CPU_EXEC()` for the
remaining clocks, `nevent_progress()`, until the vsync event clears
`pcstat.screendispflag`. Then it runs the once-a-frame devices
(`diskdrv_callback`, `calendar_inc`, `sound_sync`, ...). One call is one
frame of the machine. That is the frame Chimera gets.

**Every place the host reaches the machine, and what to do (READ, from a
search of the tree outside the Windows, wx, X11, Android and sample
directories).**

| # | Where | What it does | Plan |
|---|---|---|---|
| 1 | `timing.c` (`GETTICK()`, which libretro defines as `cpu_features_get_time_usec()/1000` in `sdl/libretro/compiler.h:25`) | A millisecond counter used by `timing_getcount()`, which the SDL front end uses for pacing, the disk motor sound (`fddmtr_callback`) and the SASI BIOS's "stop time" (`bios/sxsibios.c:281,318`) | Our own `compiler.h` defines `GETTICK()` as milliseconds of emulated time, computed from the CPU clock count. Nothing reads the host. |
| 2 | `fdd/fdd_mtr.c:164,176` | Motor seek timing on `GETTICK()` | Covered by item 1. |
| 3 | `keystat.c:667-714` | Software key repeat on `GETTICK()` | Force `np2cfg.keyrepeat_enable = 0`. A held key in a movie is held; the PC-98 keyboard does its own repeat. Item 1 covers it anyway. |
| 4 | `calendar.c:195` (`calendar_initialize`) and `sdl/timemng.c` (`gettimeofday` + `localtime`) | The calendar clock starts at the host's local time plus an offset, then advances once a second of frames (`calendar_inc`: 10 steps a frame, a second every 564 steps, so it assumes 56.4 frames a second) | Our `timemng_gettime()` returns the project's start time, a `datetime` setting (porting-a-core.md: "The machine's calendar clock is a setting the project pins"), plus emulated time since boot. Force `np2cfg.calendar = 0` (the virtual calendar) and `cal_vofs = 0`. |
| 5 | `io/np2sysp.c:448` | The "sync clock" switch copies host time into the high-resolution timer | Covered by item 4: it calls `timemng_gettime()`. |
| 6 | `fdd/sxsi.c:32` and `:499` | A `GetTickCount()` built on `gettimeofday`, and a date stamp in the names of disk state files | Not reached by our path (state files are NP2's own savestate, unused). Confirm, then stub `GetTickCount()` to the emulated counter. |
| 7 | `pccore.c` `SUPPORT_ASYNC_CPU` (`pccore_asynccpu`) | Speeds the emulated CPU up or down to keep up with the host | Build without `SUPPORT_ASYNC_CPU`, force `np2cfg.asynccpu = 0`. This one changes the machine itself, so it must not exist in either build. |
| 8 | `np2_tickcount.c`, `statsave.c:1971` | The performance counter, used by async CPU and HAXM | Build without `SUPPORT_NP2_TICKCOUNT` if it links without; otherwise point it at the emulated counter. HAXM is left out. |
| 9 | FPU choice (`np2cfg.fpu_type`: SoftFloat, DOSBox, DOSBox+INT64) | The DOSBox FPUs compute with the host's `double` and C maths library | Use SoftFloat 3 only, and do not offer the others. SoftFloat is integer code, so the native build (glibc) and the guest build (musl) cannot disagree. It also avoids the GPL (section 2). |
| 10 | `rand()` in `cbus/atapicmd.c:1298`, `cbus/mpu98ii.c:1254`, `sound/fmgen/fmgen_opm.cpp:354,385`, `wab/npdisp.cpp:244` | Random numbers from the C library, never seeded | Replace with our own copy of glibc's `rand()`, so the core gives the numbers NP2kai's Linux builds give. Details below. |
| 11 | `RDTSC` (`i386c/ia32/instructions/system_inst.c:1164`) | The time stamp counter | Only the HAXM branch reads the host. Force `np2cfg.consttsc` to the value that derives it from emulated clocks, and test it. |
| 12 | `cbus/board118.c`, `cbus/boardsb16.c` (`QueryPerformanceCounter`) | Game-port timing | The reads seen are inside `SUPPORT_IA32_HAXM`; otherwise it uses `CPU_MSR_TSC`. Confirm while building. |
| 13 | Threads: `np2_thread.c`, `sound/sndcsec.c` | A semaphore around sound, only with `SUPPORT_NP2_THREAD` | Build without `SUPPORT_NP2_THREAD`. libretro-common's thread code is not linked at all (we do not use the libretro port, section 12). |
| 14 | Network: `network/` | LGY-98 network card | Build without `SUPPORT_NET`/`SUPPORT_LGY98`. Chimera has no network code. |
| 15 | Host folders: `generic/hostdrv*.c` | Lets the guest see a folder on the host | Leave out (`SUPPORT_HOSTDRV*` off). The sandbox has only mounted files. |
| 16 | MIDI out: `sdl/cmmidi.c`, `sound/vermouth` | Host MIDI, or a GUS patch synthesiser reading a patch set from disk | Leave out at first. Vermouth could come back later as an in-core synthesiser if the owner wants MIDI music, with its patch files as firmware. |
| 17 | The embedded menu (`embed/menu`, F12) and the soft keyboard | On-screen menus drawn into the picture | Leave out. Every choice they offer becomes a setting or an input. |

**Random numbers: match NP2kai's Linux builds (user-decided, 2026-10-23).**

What NP2kai does (READ): it calls `rand()` and never `srand()` (no call in
the tree), so every run starts from the C library's default seed, 1. The
sequence is then whatever that C library's `rand()` makes: Microsoft's on
Windows, glibc's on Linux. The owner chose the Linux builds as the reference,
because the Windows builds are not consistent with each other.

Where the numbers reach the machine (READ):

- `cbus/atapicmd.c:1298`: while CD audio plays, the position the drive
  reports moves by `rand() & 7` frames. Part of the machine: a game that
  reads its music position can see it.
- `sound/fmgen/fmgen_opm.cpp:354,385`: the noise waveform of the OPM chip's
  LFO. Sound only.
- `cbus/mpu98ii.c:1254` is inside `MIDIIN_DEBUG`, not compiled.
  `wab/npdisp.cpp:244` picks a slot in a font cache of the host display
  driver; to confirm it never changes what the guest sees.

The plan:

1. One patch replaces those calls with `np2_rand()`, our own copy of glibc's
   `rand()` algorithm (its `TYPE_3` generator, the default): a table of 34
   numbers filled from the seed with `16807 * x mod 2147483647`, each next
   value the sum of the values 31 and 3 places back, the first 344 thrown
   away, the result shifted right by one. This was checked on 2026-10-23
   against the real glibc in a script: the first 100000 values agree exactly,
   starting 1804289383, 846930886, 1681692777.
2. Its state (34 numbers and a position, under 200 bytes) lives in the
   machine's memory, so savestates, rewinds and movies carry it. NP2kai's own
   savestates do not save it, so NP2kai drifts after a state load; this core
   does not. That is the one place the core is deliberately more consistent
   than upstream.
3. It is seeded with 1 once, at `Init`, and not again on a machine reset,
   because the real program does not reseed either: the sequence runs on from
   power-on through resets.
4. Both builds use our copy, so native (glibc) and sandbox (musl) cannot
   differ. No other code may call the C library's `rand()`; the gate checks
   that `core.wbx` has no reference to `rand` or `random` (`nm`).

What proves it (M2): a leg in the native harness compares `np2_rand()` with
the host glibc's own `rand()` for the first million values; negative control:
change the 344 to 343 and see it fail. A second leg plays a CD audio track
from a gate-built CD and checks the reported positions are the same in native
and sandbox.

**Floating point outside the CPU (UNCHECKED, a known risk).** fmgen builds
its tables with `sin`, `pow` and `log` (`fmgen.cpp`, `MakeTable`), and the
libretro volume code uses `pow`. glibc and musl may round some of these
differently in the last bit, which would make native and sandbox sound
different while the machine stays the same. The audio comparison in M3 will
show it. If it happens, the fix is to give both builds the same maths code
(for example compile a copy of musl's `libm` functions into the native
build), not to drop the comparison.

**What proves this section (M2).** The test porting-a-core.md describes:
stall the host for 300 ms in the middle of a run and show the machine did not
notice (same memory, same picture, same calendar), and show the test has
teeth by building once with item 1 or 4 pointed back at the host clock and
watching it fail.

---

## 4. The picture

**Why this way.** porting-a-core.md ranks three ways to get a picture; the
first, reading the machine's own memory, is the cheapest and is exact by
construction. NP2kai draws every frame in software (`scrndraw`, `vram/`,
`wab/` for the PC-9821 accelerator cards) into a buffer the platform layer
gives it (`sdl/scrnmng.c`). Our `scrnmng` gives it a buffer in guest memory
and `GetVideoBgra` converts that to BGRA.

**Decisions.**

- Ask NP2kai for 32 bits a pixel always (`SUPPORT_32BPP`), not the 16-bit
  RGB565 libretro uses by default, so no colour is lost and the conversion is
  a copy.
- Declare the largest picture the machine can draw as the buffer, and report
  the live size each frame through `GetVideoWidth`/`GetVideoHeight` (the
  DOSBox-X precedent). libretro declares 1640x1024 as its largest
  (`LR_SCREENWIDTH`, `LR_SCREENHEIGHT`); the real maximum with the Cirrus
  cards is to be read from `wab/` while building. The usual PC-98 picture is
  640x400.
- Leave out NP2kai's video filters (`SUPPORT_VIDEOFILTER`) at first. A
  setting that only changes the picture is allowed, but nothing needs it.

**The frame rate (UNCHECKED, needs care).** A PC-98 frame is one sync of its
display, and that rate depends on the display mode: libretro reports 56.4
frames a second (the 24 kHz, 400-line mode), but the 15 kHz modes and the
PC-9821's 31 kHz modes run at other rates, and `pccore_exec()` returns once
per sync whatever the rate is. The DOSBox-X core faced the same thing (its
refresh rate follows the DOS video mode). The step is: compute the exact
rate of each mode as a fraction from `gdc.c`'s arithmetic, declare the
24 kHz one in `waterbox.config` and return the same from
`GetVsyncNumerator`/`GetVsyncDenominator`, and read how the DOSBox-X core
reports a mode change. If Chimera cannot already express a changing rate,
the core has to live with a fixed declared rate, or propose the change to the
Chimera project; it is not this project's to make.

**Decided: a monitor setting (user-decided, 2026-10-23).** Chimera asks
`GetVsyncNumerator`/`GetVsyncDenominator` once, after `Init` (section 18), so
the core cannot report a new rate while it runs. A setting "Monitor" (24 kHz
or 31 kHz; 15 kHz if the gate shows it is needed) is part of the machine: it
picks the display timing the machine starts in, and the core reports that
mode's exact rate from `Init`. The default is 24 kHz (the PC-9801VX of
section 9). UNCHECKED, read in M4: whether NP2kai lets a setting pick the
starting mode, and what happens when software switches mode itself (some
PC-9821 software switches to 31 kHz 640x480). In that case the frames keep
coming once per sync, and only the movie time Chimera shows is off; the M4
step measures by how much and writes it here.

---

## 5. Sound

**Why this way.** porting-a-core.md: audio is pulled at the end of the
frame, the declared rate is the rate the core mixes at, and the reference
build must pull the same way or the two builds differ.

**Decisions.**

- Mix at 44100 Hz stereo (`np2cfg.samplingrate`, the libretro default) and
  declare `audio.rate` 44100. NP2kai's chips are mixed to that rate inside the
  emulator.
- Give Chimera the samples the machine actually produced in that frame. The
  libretro port pulls a fixed 782 samples a frame (`SNDSZ`, 44100/56.4) with
  `sdlaudio_callback(NULL, NULL, SNDSZ*4)`; a fixed count drifts against a
  machine whose frame rate changes. Take what `sound_sync()` made instead.
- Check it by counting, as porting-a-core.md says: over a long run the
  samples must be the rate divided by the frame rate.
- The six YM2608 rhythm samples (`2608_*.wav`) are firmware (section 7).
  Without them the rhythm channel is silent, as on DOSBox-X.
- The disk motor and seek sounds (`np2cfg.MOTOR`) are sound only; offer them
  as a setting, default off.
- Two of NP2kai's boards are fixed in waterbox/port/, each compiling the
  upstream file unchanged with the broken functions renamed (the owner chose
  this, 2026-10-24). PC-9801-14 (tms3631-pitch.c): upstream applies the 4 times
  oversampling twice, so every note played 16 times too high. Little
  Orchestra L (boardlol-fixed.c): upstream never resets the chip and hands it
  to the SSG mixer as an SSG, which crashed at the first sound; it is now set
  up like the other YM2203 boards, FM fixed left. The review behind this,
  with every board measured, is the Claude Doc "NP2kai core sound review".

---

## 6. Input

**Why this way.** Chimera sends levels, not events: each frame says what is
held. A movie column is a button, so the button list is the movie's format
and must be fixed and in a fixed order. A key binding names inputs exactly as
Chimera sends them (`docs/new-core.md` section 3).

**The keyboard.** The PC-98 keyboard sends its own key codes (0x00 to 0x7F)
to the machine's keyboard controller; NP2kai keeps them in `keystat.c` and
the platform layers translate host keys to them (`sdl/kbtrans.c`). Declare
one button per PC-98 key, named after the key on a PC-98 keyboard (STOP,
COPY, HELP, XFER, NFER, KANA, GRPH, CAPS, ROLL UP, ROLL DOWN, HOME/CLR, f1
to f10, vf1 to vf5, the keypad's `=` and `,`, and the rest). Each frame,
compare with the last frame's levels and call NP2kai's key down and key up
for what changed. The key list comes from `keystat.tbl` and is generated, not
typed by hand, the way the DOSBox-X core generates its list from the
emulator's enum (`gen-config.py`).

That is more than 64 buttons, so it uses the wide-input channel (a
`SetButton(index, state)` export) that Chimera grew for the DOSBox-X core
(READ: DOSBox-X's `waterbox.cpp`, "121 buttons ... through the wide-input
SetButton export"). Default key bindings map each PC-98 key to the host key
in the same place; keys a PC keyboard does not have (XFER, NFER, vf1 to vf5,
COPY, STOP, HELP) get bindings that are written down in `default_keybinds.json`
and in the README.

Give every button a mnemonic and a column header (`mnemonics`, `headers`),
because many PC-98 keys would otherwise share a first letter.

**The mouse.** The PC-98 bus mouse reports movement, not a position, and
software reads counters through the mouse port (`mousemng.c`,
`mouseif_sync()`). Declare its two buttons and two relative axes (Chimera's
`RMouse X`/`RMouse Y`, "its movement"). An absolute position, which the
DOSBox-X core needed for Windows, has no meaning on a bus mouse, so it is not
offered. Decided (user-decided, 2026-10-23): relative movement only;
pointing at a spot on the screen is not offered (section 18, #135 and #210).

**The order keys are listed in (user-decided, 2026-10-23).** Config >
Controls and TAStudio list the keyboard the way it sits on a PC-98: start at
the top-left key, go right across the whole row, then start the next row down
at its left end. A row runs across the main keys, then the cursor keys. The
number pad comes after the whole keyboard as one block, row by row (owner,
2026-10-24: all keypad keys together in Config > Controls). Then the mouse,
the joystick, and the disk buttons last (section 18, #310 and #337). The draft below
follows NP2kai's key codes (READ, `sdl/kbtrans.c`; PC-98 codes run in nearly
the same order), with the code in brackets:

| Row | Keys, left to right |
|---|---|
| 1 | STOP (60), COPY (61), f.1 to f.10 (62-6B), vf.1 to vf.5 (52-56) |
| 2 | ESC (00), 1 to 0 (01-0A), - (0B), ^ (0C), Yen (0D), BS (0E); ROLL UP (36), ROLL DOWN (37); HOME CLR (3E), HELP (3F) |
| 3 | TAB (0F), Q to P (10-19), @ (1A), [ (1B), RETURN (1C); INS (38), DEL (39) |
| 4 | CTRL (74), A to L (1D-25), ; (26), : (27), ] (28); Up (3A) |
| 5 | SHIFT (70), Z to M (29-2F), comma (30), . (31), / (32), _ (33), right SHIFT (7D); Left (3B), Right (3C) |
| 6 | CAPS (71), kana (72), GRPH (73), NFER (51), SPACE (34), XFER (35); Down (3D) |
| pad | - (40), / (41); 7, 8, 9, * (42-45); 4, 5, 6, + (46-49); 1, 2, 3, = (4A-4D); 0 (4E), comma (4F), . (50) |

UNCHECKED, checked in M6 against a photo of a real PC-9801 keyboard: where
the cursor keys and number pad line up with the main rows, where CAPS and
CTRL sit, and the vf keys (only on later keyboards; codes 52-56 are not in
`kbtrans.c`). Keys a PC has and a PC-98 lacks (the Windows and menu keys
NP2kai maps) are not listed: they are host keys, not PC-98 keys.

**The keyboard picture (owner's request, 2026-10-23).** Config > Controls can
show a PNG of the controller beside the bindings (`"picture"` in the input
declaration, Chimera issue 338); its licence goes in `package-licenses.json`
and the contract tests refuse an unlicensed one (READ, porting-a-core.md).
Two ways, in order of preference:

1. **Draw it ourselves.** A script (`tools/draw-keyboard.py`) draws the
   PC-98 keyboard from the table above: one box per key, in its row, with
   its name. It is this repository's own work under its own licence, so it
   needs no outside licence, and it always matches the bindings. This is
   the default.
2. **A free photo.** Wikimedia Commons has
   `File:Nec_pc9801-keyboard_browncable.jpg`, found by a web search on
   2026-10-23. Its licence, author and resolution are UNCHECKED: Commons
   could not be reached from this session. If the owner wants a photo, the
   owner downloads it and checks its licence page; a CC BY or CC BY-SA photo
   is fine if `package-licenses.json` names it with its author and licence.
   A photo of a keyboard at an angle is harder to read than a drawing.

**The joystick.** The PC-9801-26K and -86 sound boards have one or two
Atari-style ports read through the sound chip's SSG port (`joymng`). Declare
two pads of up, down, left, right and two buttons. libretro's joystick-to-key
and stick-to-mouse options (`joy2key`, `stick2mouse`) are front-end
conveniences and are dropped; Chimera's own bindings do that job.

**Disks.** Buttons to change floppies, following the DOSBox-X core's names
and meaning ("Previous Floppy Disk", "Next Floppy Disk", "Swap Floppy Disk"),
for each of the two drives, because PC-98 games often want disk 2 in drive 2
and then disk 3 in drive 1.

**Lag frames and the input-poll export.** A lag frame is one in which the
machine read no input. Mark a read when the guest reads the keyboard data
port, the mouse counters or the joystick register, and call the
`SetInputPollCallback` function there, so a Lua script's `event.oninputpoll`
runs at the right moment (porting-a-core.md, "Calls during a frame").
(UNCHECKED: the exact port numbers are read from `io/` and `cbus/` while
building.)

**What proves this section (M6).** A test program on a disk the test script
builds writes what it reads from the keyboard, mouse and joystick into text
VRAM; the test feeds a fixed pattern through `SetButton`/`SetAxis` in the
sandbox and through the native harness's input structure, and compares.
Negative control: drop one key from the translation and see the test fail.

---

## 7. Files: firmware, disks, settings

**Why this way.** The sandbox has a flat list of files mounted by name, no
folders, every mounted file read-only, and nothing a core writes to a
mounted file is saved with the machine (MACHINE-SPEC section 8). So anything
the machine changes has to live in the core's own memory, and be copied
there during `Init` before the seal so a savestate carries only what changed
(`docs/save-data.md`).

**Firmware.** NP2kai looks for these by name (READ: `bios/bios.c`,
`font/fontdata.c`, `sound/rhythmc.c`, `sound/soundrom.c`, `cbus/ideio.c`,
`cbus/sasiio.c`, `cbus/scsiio.c`, `io/pcidev.c`, and libretro's page):

| File | What it is | When it is needed |
|---|---|---|
| `bios.rom` | The PC-98 BIOS | **Always** (owner, 2026-10-24): required firmware, NP2kai's compatible BIOS is never used |
| `itf.rom` | The initial test firmware | **Not used** (owner, 2026-10-26): NP2kai is built with BIOS_SIMULATE, which always uses its own ITF and never opens the file |
| `FONT.ROM` or `font.bmp` | The character generator, including kanji | Always: required firmware, no fallback is shipped (open question 5) |
| `sound.rom` | The sound board's BIOS | **Always** (owner, 2026-10-24) |
| `2608_bd.wav`, `_sd`, `_top`, `_hh`, `_tom`, `_rim` | YM2608 rhythm samples | **Always** (owner, 2026-10-24), unless the setting Rhythm Samples is YM2608 ROM |
| `ym2608_adpcm_rom.bin` | The YM2608's own drum ROM, as MAME dumps it (8192 bytes, crc 23c9e0d8, in MAME's `ym2608.zip`) | When the setting Rhythm Samples is YM2608 ROM, in place of the six WAV files (waterbox/port/rhythm-rom.c) |
| `bios9821.rom`, `d8000.rom`, `ide.rom` | IDE BIOS of PC-9821 machines | The PC9821 machine **with a hard disk** in the project (owner asked for always; see below) |
| `sasi.rom`, `scsi.rom` | Hard-disk controller BIOS | When a SASI or SCSI disk is attached and the real BIOS is used |

Chimera does not allow optional firmware: each entry says when it is needed
with `requiredWhen` (porting-a-core.md, "Firmware").

**Decided (owner, 2026-10-24): none of them is optional.** The settings
"Use the Real BIOS", "Use PC-98 Sound BIOS", "Use PC-98 Rhythm Samples" and
"Use the Real IDE BIOS" are gone. `bios.rom`, `sound.rom` and the
six rhythm samples are required on both machines (`itf.rom` was too, until
2026-10-26: the build never reads it) (`requiredWhen` machine in
PC9821, PC98, since Chimera has no unconditional entry), and the driver
always sets `usebios`. `ide.rom` is required, and used, on the PC9821
machine whenever the project has a hard disk. Without a hard disk the real
IDE BIOS waits for ever: it polls the empty IDE unit for its ready bit
(`D800:09AA`, a delay on port 5Eh, still there after 25000 frames, 7
minutes), so then NP2kai's own IDE BIOS stands in. Fixing NP2kai's empty
unit so the real IDE BIOS gives up on it would let `ide.rom` be required
always; not done. Gate and frontend check take the firmware from
`NP2_FIRMWARE`; the frontend reference keeps only the memory below C0000h,
so no ROM is in it. Use the same setting
names and descriptions as the DOSBox-X core where the meaning is the same
(`pc98FontRom`, `pc98SoundBios`, `pc98RhythmSamples`), so a user meets the
same words in both cores. Dumps differ from machine to machine, so most
entries pin no `sha1`; give them `size` and `nameHints`.

NP2kai builds firmware paths as folder plus name (`getbiospath`,
`file_getcd`). Set the folder to empty, so `bios.rom` is opened as
`bios.rom`, which is exactly how the sandbox mounts firmware ("mounted in the
guest under its ID").

**The font, specially.** When no font file is found, `font_load()` calls
`makepc98bmp()`, which draws a font from a font installed on the host
(`fontmng`) and writes a temporary file. Both are impossible and
non-deterministic here, so that call is compiled out. What the machine shows
with no font firmware does not matter for users, because the font is
required firmware (decided, open question 5): one firmware entry that takes
either `FONT.ROM` or `font.bmp`, with no `requiredWhen` condition, so the
frontend asks for it before the machine starts. Which of the two names
NP2kai looks for first is read in `font_load()` when the entry is written.

**The owner's files (user-decided, 2026-10-23).** The owner keeps the
firmware and other files for this core (BIOS, font, sound ROM, rhythm
samples, disks) in `G:\sdlnp21kai (from linux)` on their own computer, the
folder of their Linux SDL np21kai build. Local tests that need real firmware
or games read it from there. It is outside `G:\GitHub`, so reading it needs
its own folder approval from the owner, asked when a step first needs it.
Nothing from it is committed, uploaded or copied into the repository or a
package: it is the user's content, not this project's (section 14).

**Floppies and hard disks: an in-memory file layer.** NP2kai's file layer
(`sdl/dosio.c`) opens disk images read-write and writes sectors back to the
host file (READ: `file_open` opens with `rb+`). PC-98 games save to floppy,
and many need a user disk. The plan, copied from the DOSBox-X core's hard
disk:

- At `Init`, before the seal, copy every disk image the project mounts into a
  buffer in guest memory.
- Our `dosio.c` serves `file_open`, `file_read`, `file_write`, `file_seek` and
  `file_getsize` for those names from the buffers. Every disk format NP2kai
  knows (`.d88`, `.fdi`, `.nfd`, `.hdi`, `.thd`, `.nhd`, ...) then works
  unchanged, protection data included, because NP2kai's own parsers still read
  the bytes.
- Writes land in the buffer, so they are part of every savestate and roll back
  with a rewind.
- The save-data export group (`GetSaveDataFileCount/Name/Size/Buffer`)
  returns the changed disks under the names they were mounted with, so an
  exported disk goes back into the project as a disk.
- Size the memory layout for the largest hard disk accepted. PC-98 hard
  disks are usually 20 to 500 MB; if larger ones are wanted later, the
  DOSBox-X core's sparse disk (`waterbox/sparse-disk.h`: the read-only image
  as the base, only changed sectors in memory) is the model.

**CD-ROMs** (PC-9821) are read-only, so the `.cue`/`.iso`/`.ccd`/`.mds`/
`.nrg` readers in `diskimage/cd/` can open the mounted files directly; the
sandbox allows several opens of a read-only file at once. The CD drive's
speed setting must not depend on the host.

**Settings file.** NP2kai normally reads and writes `np2.cfg` (`sdl/ini.c`).
Here nothing reads or writes it. The core fills `np2cfg` from Chimera's
settings before `pccore_init`. The PC-98's DIP switches and memory switches
(`np2cfg.dipsw`, `np2cfg.memsw`) are settings with a project default; what
the guest changes while it runs lives in the machine's memory like any
other state.

---

## 8. Savestates

**Why there is little to do.** In the sandbox a savestate is a snapshot of
the guest's whole memory, so it usually works the day everything else does
(porting-a-core.md, "Savestates"). NP2kai's own savestate code
(`statsave.c`, which writes a temporary file) is not used.

**What could break it.** Anything of the machine kept outside guest memory
(nothing expected, once section 7 is done), host pointers that differ
between builds (harmless inside the arena, but they make raw memory differ
between native and sandbox, so the gate compares the machine's memory
domains, not the whole heap), and caches kept across `Init`.

**What proves it (M7).** The two tests every core has: save and load before
every frame and land where a straight run does; save in one process, load in
another, and land in the same place.

---

## 9. Settings

**Why care.** "Settings that shape the machine are part of the reproduction
contract. There is no such thing as a cosmetic setting here." Each setting's
description says, in plain words, whether it is part of the machine ("a movie
needs the same value") or changes only the picture or sound.

**From libretro's core options (READ, `libretro.c` `update_variables` and
`libretro_core_options.h`), sorted:**

- **Part of the machine:** model (PC-9801VX, PC-286, PC-9801VM, and the
  PC-9821 models in the IA-32 build), base clock (2.4576 or 1.9968 MHz),
  clock multiplier, RAM size, sound board (14, 26K, 86, 86+26K, 118, SB16 and
  combinations), the 118 ROM, which FM engine (`usefmgen`), GDC type
  (`uPD72020`), the Cirrus card and its type, PEGC plane mode, PCI and its
  chipset, memory check speed, the CD ECC option, the calendar start time
  (new, a `datetime`), use of the real BIOS (new).
- **Sound only:** the master and per-chip volumes, the disk motor sound.
- **Picture only:** LCD mode, skip-line drawing (to confirm that `RASTER` is
  picture only; if it changes timing it is part of the machine).
- **Dropped, because they are the host's business or break determinism:**
  `asynccpu`, `delayms`, key repeat, joystick-to-key and stick-to-mouse, the
  menu buttons, the "use last hard disk" option, the mouse speed (Chimera's
  bindings set it).


**Defaults: NP2kai's Linux SDL build (user-decided, 2026-10-23).** Each
setting's default is the one the Linux SDL build starts with when it has no
config file, read from the `np2cfg` initialiser in `pccore.c` and the
defaults in `sdl/ini.c` (READ, NP2kai `5939e0c`), the same reference build
as for random numbers (section 3). The libretro core options differ and are
not used. Known so far:

| Setting | Default |
|---|---|
| Model | PC-9801VX (`"VX"`) |
| Base clock | 2.4576 MHz (`PCBASECLOCK25`) |
| Clock multiplier | 20 (49.152 MHz) (`PCBASEMULTIPLE`); a list of 1 and the even numbers to 70 (owner, 2026-10-24) |
| Extended RAM | 13 MB (`EXTMEM`, also the `ini.c` default) |
| Sound board | PC-9801-86 (`SOUND_SW` 0x04) |
| Sample rate | 44100 Hz (output only) |
| GDC | uPD7220 |
| Real BIOS | always (required `bios.rom`; `itf.rom` dropped 2026-10-26, never read) |
| CPU reported (IA-32) | Pentium III with an SSE build, Pentium without MMX/SSE (`i386c/ia32/cpu.h`); fixed by our build flags |
| FPU | SoftFloat (build A, section 2) |

UNCHECKED, read before the settings are written: the FM engine default
(`usefmgen`) in the SDL build, the WAB, PCI and PEGC defaults, and whether
the SDL build changes any of these at start-up. The IA-32 and 286 machines
both start from these values; the multiplier gives the 286 machine a speed no
real 286 had, which is upstream's default and is kept.
---

## 10. Which CPU: both, as two machines of one package

**Decided (user-decided, 2026-10-23):** one package that offers both CPUs as
two machines, chosen by the user when making a project.

**The two builds (READ, `sdl/Makefile.common`).** The "np2kai" build compiles
`i286c/` (V30 and 80286 machines). The "np21kai" build compiles `i386c/` and
adds `CPUCORE_IA32`, `SUPPORT_PC9821`, IDE, PCI, the PEGC and large memory.
Those defines change how the shared code is compiled too (`pccore.c`, `io/`,
`cbus/`, the memory code), and both builds define the same function names, so
the two cannot simply be compiled side by side.

**What Chimera already gives (READ).** A package may declare several
`machines`; the choice is an ordinary setting, recorded in the project, and
each machine has its own system id, controller and picture
(`source/engine/source/session.cpp`, `chooseMachine`; Genesis Plus GX uses it
for four Sega machines). But a package holds exactly one guest binary
(`source/engine/source/package.cpp`: `WBX_FILE = "core.wbx"`).

**How both fit in one `core.wbx` (UNCHECKED until built).** Build each
emulator as its own set of objects with its own defines. Link each set into
one relocatable object (`ld -r`), then hide every symbol in it except a few
entry functions (`objcopy --keep-global-symbol=...` on a prefixed name, such
as `np2_286_init`, `np2_286_frame`, `np2_ia32_init`, `np2_ia32_frame`), so the
two copies of `pccore_exec` and friends no longer clash. Calls to the C
library stay as undefined references and resolve to the one libc at the final
link. The adapter (`wbx-entry.c`) reads the machine setting at `Init` and
calls one half; the other half is never run. Chimera learns nothing new.

Costs and checks:

- The package carries both halves of code. The unused half's static data sits
  in guest memory but never changes, and a savestate stores only pages that
  changed since the seal, so it costs nothing per state (MACHINE-SPEC
  section 4).
- Our platform layer (`waterbox/port/`) is compiled into each half with that
  half's defines, so each half gets its own copy.
- Every gate leg runs on both machines, and a leg checks that each machine
  reports its own system id and that a movie from one refuses the other.
- The other way, teaching Chimera one binary per machine, would be a change to
  Chimera's design, which a third-party core cannot make; the linking route
  avoids it.

**Order.** The IA-32 machine first, through every step to M10, because it is
the larger catalogue (80386 and later PC-9801 software and the PC-9821) and
one machine halves the work until the core is proved. The 286 machine comes
next (M12), into the same package. The cost to watch is speed: the IA-32
interpreter is slower, and Windows 95 on an emulated Pentium inside the
sandbox may be too slow to be useful (measured in M3, the way the PCem plan
measured its machine before committing).

**Ids.** Core id `np2kai`, which matches this repository's name
(user-decided, 2026-10-23), and package `np2kai-<id>.chimeraCore`, where
<id> is the package's own 40-digit SHA1, as Chimera's other cores name
theirs (owner, 2026-10-24). The machines
(user-decided, 2026-10-23): `PC98`
("NEC PC-9801, 286 CPU") and `PC9821` ("NEC PC-9801 / PC-9821, IA-32 CPU"),
both named in `systemNames`. DOSBox-X's system id is `DOS`, so there is no
clash (READ).

---

## 11. Memory, and what tools can see

**The layout.** `waterbox.config` declares `{sbrk, sealed, invisible, plain,
mmap}` in MiB. Large allocations go to mmap under musl (porting-a-core.md).
Start from the DOSBox-X core's `{256, 16, 16, 64, 1024}` and adjust from
what M3 measures: the PC-98's own memory (up to the RAM setting's maximum),
the VRAM, the font memory, the disk buffers of section 7.

**Memory domains for RAM search and Lua.** Main memory (the first 1 MB plus
extended memory, `mem[]` and `CPU_EXTMEM`), text VRAM, graphics VRAM (the
four planes), the font RAM, and the disk buffers. For the IA-32 build, also
a bus of the physical address space through NP2kai's own memory read
functions, with `ReadBus` so a search is fast and `GetBusRanges` so a search
covers only memory that exists. List the main memory first: tools open on
the first one.

**Other optional exports**, each cheap and each listed in porting-a-core.md:
drives and their lights (`GetDriveCount/Name/Light` and the media exports,
since floppies swap), the CPU's registers, save-data export (section 7), and
`SetRenderingEnabled` for turbo, which skips only the BGRA conversion (the
drawing stays, because the machine's memory must not depend on it).

---

## 12. The repository

**Why copy rather than start empty.** "Nobody starts a core from an empty
directory." The DOSBox-X core is the closest in shape: a PC, disks, firmware,
a keyboard and a mouse, a C/C++ emulator built with meson for both builds,
wide input, an in-memory hard disk, save-data export, PC-98 firmware already
named. Copy its scripts and declarations and delete what does not apply.

**Not the libretro port.** `sdl/libretro/libretro.c` leans on libretro-common
(its file streams, threads, timers and `environ_cb`), the embedded menu and
libretro's fixed-size audio pull. It is the best map of what to call and in
what order, but our adapter is a new file: `pccore_init` and `pccore_reset`
at `Init` as `retro_run`'s first call does, `pccore_exec(TRUE)` per frame,
and our own versions of the few platform files (`compiler.h`, `dosio.c`,
`scrnmng.c`, `soundmng.c`, `timemng.c`, `mousemng.c`, `joymng.c`,
`kbtrans.c`, `fontmng.c`). The upstream platform files are not compiled; ours
live in `waterbox/port/`. Upstream's core sources (`pccore.c`, `cbus/`,
`io/`, `vram/`, `sound/`, `i386c/`, `diskimage/`, `fdd/`, `bios/`, ...) are
compiled as they are, with a short numbered patch series for the seams
section 3 lists that cannot be reached from the platform layer (the `rand()`
calls, the font fallback, anything that reads the host outside the platform
files).

**Patches.** porting-a-core.md's rule: a numbered series applied
all-or-nothing by `waterbox/apply-patches.sh`, each a build option or a hook,
never a deletion. (DOSBox-X carries full-file copies instead; that came from
its BizHawk history and does not suit a small set of seams.) Mind the two
traps that document names: regenerating one patch in the middle of the series,
and a submodule that is not really there.

**Layout.**

    extern/NP2kai/             upstream, a pinned submodule, never edited in place
    patches/                   0001-... numbered series
    meson.build                both builds (native and guest), one source list
    waterbox/
      port/                    our platform layer (compiler.h, dosio.c, scrnmng.c, ...)
      np2-driver.{h,c}         the machine: Init, the frame, inputs, the disk buffers
      wbx-entry.c              the guest exports (Init, FrameAdvance, GetVideoBgra, SetButton, ...)
      run-native.c             the reference harness, with a flag for every diagnostic
      run-wbx.c                runs core.wbx the way the engine does
      guest-syscalls.c         system calls the sandbox lacks, added as they appear
      setup-guest.sh, build-package.sh, apply-patches.sh
      run-gate.sh              the core gate
      gen-config.py            generates waterbox.config and default_keybinds.json
      waterbox.config, file_slots.json, default_keybinds.json, package-licenses.json
      tests/run-frontend.sh    the frontend gate
      tests/gen-testdisk.py    builds the boot disks the gate runs
    docs/PLAN.md, docs/BUILDING.md
    AGENTS.md, README.md, LICENSE
    .github/workflows/chimera.yml

**The pin.** Upstream's default branch is `wx_alpha` and it is active (the
latest commit read is from 2026-09-05). Pin one commit, write in this file
why that one, and move it only on purpose. `5939e0c6d598` is the candidate
because it is the one read for this plan.

**File slots (proposal).** Floppy disks (any number, in swap order; the first
two go in drives 1 and 2 at boot), hard disks (up to two, for SASI or IDE),
CD-ROMs (IA-32 build), save data (one, per `docs/save-data.md`), and at least
one of floppy or hard disk. The project is the game; the BIOS and the font are
firmware, because one serves every game.

---

## 13. The steps, in order

Each step says what is done, why it comes at this point, and what proves it.
"Proves" means a test in `waterbox/run-gate.sh` (the core gate) that fails
when the step is undone, and that has been seen to fail once on purpose
(`docs/gates.md`: "a leg that has never been seen to fail is a leg that has
not been tested").

**M0 - this plan.** Done: the questions that blocked M1 (1, 3 and 4) are
answered (2026-10-23). The licence decides which source files may be
compiled, which is why it had to come first. Still open, and not blocking:
(deferred) publishing. The ids are decided. The font is
decided: required.

**M0b - the DOSBox-X comparison (section 1), for information.** It does not
block anything and can run beside M1. Why early: it picks the real games the
later steps are tested on. Proof: a table in this file of named games, each
with what happened on both emulators.

**M1 - skeleton and a native headless build.** The submodule pinned, the
meson build of NP2kai's core sources with our platform layer, the defines of
the libretro IA-32 build minus everything section 3 leaves out, and
`run-native` booting with no disk and no firmware to NP2kai's own BIOS
screen, dumping a frame and the main memory. Then a boot disk built by
`tests/gen-testdisk.py` (a raw 1.25 MB PC-98 floppy whose boot sector
writes a known pattern to text VRAM) runs and the pattern is in memory.
Why now: everything after this needs a build you can debug with gdb and
printf, and the boot disk proves item 3 of the finding (no firmware needed)
early, which decides what CI can run.

**M2 - virtual time (section 3).** The `GETTICK`, `timemng`, async-CPU,
`rand()`, TSC and thread seams. Why before the guest build: a host clock
inside the machine makes every later comparison flaky, and it is easiest to
find in the native build. Proof: the 300 ms stall test, with the host-clock
build as the negative control; two native runs of the same input give
identical per-frame digests of memory, picture and sound.

**M3 - the guest build.** The same sources through miniBox's musl toolchain
into `core.wbx`, built with `-Dthread_local= -D_Thread_local= -D__thread=`,
passing `check-wbx.sh`; missing system calls added to `guest-syscalls.c` as
they show up; the memory layout sized. Then native against sandbox: the same
memory, picture, sound and emulated clock count frame by frame over a few
thousand frames. Also the first speed measurement (frames a second for a
286-class machine and for a Pentium-class PC-9821). Why now: this is where
"a week of link errors" lives, and the native build is ready to compare
against. Proof: native == sandbox on every digest; negative control: change
one byte of memory in one build only and see the leg fail.

**M4 - files (section 7).** Firmware mounted by name, the in-memory disk
layer, floppies and hard disks in guest memory, writes kept there, CD-ROMs
read directly, no `np2.cfg`. Why here: the machine needs disks before input
or games mean anything. Proof: the gate's disk writes a file on a
gate-built disk; the leg checks the disk changed, and the same in native and
sandbox; a differential check that a missing disk gives a different result,
so a silently broken mount cannot pass (the DOSBox-X core's `cd` leg found
exactly that kind of hollow pass).

**M5 - picture and sound declared (sections 4 and 5).** BGRA conversion,
live size, the frame-rate fraction, the sample counting. Proof: the samples
over a long run equal rate / frame rate within one frame; a picture digest
that changes when a display mode changes.

**M6 - input (section 6).** Keyboard, mouse, joysticks, disk buttons, lag
frames, the input-poll callback. Proof: the input test program of section 6,
native == sandbox, with a negative control.

**M7 - savestates (section 8).** Proof: rerecord (save and load around every
frame) and session (another process) both land where the straight run does.

**M8 - the package.** `waterbox.config` and `default_keybinds.json`
generated by `gen-config.py`, `file_slots.json`, `package-licenses.json`
after the owner's licence answer, `build-package.sh` writing
`np2kai-<id>.chimeraCore` into a Chimera checkout's `build/Cores`. Proof:
Chimera's contract tests (`InstalledCorePackagesTests` and
`MnemonicUniquenessTests`) pass against the package, and the frontend gate
(`tests/run-frontend.sh`) boots it through the real frontend, opens a
project in TAStudio and adopts the key bindings.

**M9 - the optional exports (section 11).** Memory domains and the bus,
drives, registers, save-data export, turbo. Proof: a Lua test in the frontend
gate reads a known address and gets what native wrote there; the save-data
export equals the disk buffer byte for byte.

**M10 - real games.** The games from M0b, from the user's own copies kept
outside the repository (`tests/roms-local`, or a path in an environment
variable), each named in the gate and SKIPped with the reason when absent.
At least one game from boot to play, native == sandbox, named here. A movie
recorded in the frontend replays in a new process on Linux and on Windows.
Why last but one: games find what synthetic tests miss, but only once the
machine is otherwise proved.

**M11 - CI and publishing (the owner approves each push and release).**
`chimera.yml` copied from the DOSBox-X core: check Chimera out at a pinned
commit, build miniBox and the guest toolchain, run the core gate (which needs
no game, thanks to the gate-built disks, if M1 confirms it), run Chimera's
contract tests against the package, and publish the package to this
repository's own GitHub releases (section 15). Then the first dated release
by hand, and the README says where to download it and where to put it.

**M12 - the 286 machine in the same package (section 10).** Build the 286
half, link both halves into one `core.wbx` with hidden symbols, add the second
entry to `machines`, and run every gate leg on both. Why last: the linking
trick is only worth doing once the first machine is proved, and it changes
the build scripts, not the machine. Proof: the whole gate green on both
machines, `nm core.wbx` shows no clashing global symbols, and a negative
control (call the wrong half for a machine) is seen to fail.

---

## 14. Test content

**Why it shapes CI.** A test that needs a BIOS or a commercial game cannot
run on a public runner. porting-a-core.md sorts cores three ways: the machine
is its own content, upstream ships a test suite, or nothing may be
distributed.

**This core should be the first kind.** NP2kai boots without `bios.rom`
(section 7). So the gate builds its own disks the way the DOSBox-X core
builds its `.com` programs and ISO: small hand-assembled x86 programs in a
PC-98 boot sector on a raw `.hdm` floppy (77 cylinders, 2 heads, 8 sectors of
1024 bytes), written by `tests/gen-testdisk.py`, whose bytes are a pure
function of the script. Programs: write a pattern to text VRAM; echo keyboard,
mouse and joystick to VRAM; write a sector to the disk; switch display modes;
play a tone on the FM chip. The same disk written as `.d88` and `.fdi` tests
those parsers. (UNCHECKED: whether NP2kai's own BIOS boots a disk's IPL and
answers the BIOS calls these programs use; M1 checks it. If it does not, the
programs avoid BIOS calls and talk to the hardware directly.)

**Upstream's own tests.** `tests/test_invariant_i286c.c` is one unit test of
the 286 core's memory allocation, using the `check` library. It does not test
the IA-32 build. Not a gate leg; note it.

**What may not be distributed** (the user's BIOS, fonts and games) stays
outside the repository, and its legs SKIP and say so. A SKIP is never read as
a pass. On the owner's computer those legs read the owner's folder
`G:\sdlnp21kai (from linux)` (section 7) for firmware and
`G:\GitHub\pc98examplegames` (section 1, M0b) for games, with the path given to the test
scripts as a setting, never written into a committed file.

**The required font and testing (user-decided, 2026-10-23).** Tests use the
owner's real `FONT.ROM` from `G:\sdlnp21kai (from linux)`, so the picture is
tested as it should look on a PC-98, not with a stand-in font. No placeholder
or free font is made or shipped. Because the font is required firmware
(open question 5), every leg that starts the machine through the frontend
needs it: those legs run on the owner's computer, and on a public runner
they SKIP and say so. Legs that drive the core directly and never draw text
(state, determinism, the stall test) may still run there without a font,
if NP2kai starts without one (UNCHECKED, M1). A free font such as Shinonome
(public domain) was looked at and turned down: it is not what a PC-98 looks
like, and software can read the font ROM, so a different font could change
how a game runs.

---

## 15. Publishing a third-party core

**How users get it (READ, Chimera's `docs/core-manager.md`).** Chimera
downloads nothing and keeps no list of cores. A user downloads a package from
a core's own releases page, or builds it, and puts the file in Chimera's
`Cores` folder; the Core Manager shows whatever is there. So a third-party
package loads exactly like an official one, as long as it passes the checks
Chimera makes when it scans the folder: it reads as a package, its guest ABI
is one that Chimera runs, and it stamps a version.

**What still applies, and why.**

- **The same package contract.** Run Chimera's contract tests
  (`InstalledCorePackagesTests`, `MnemonicUniquenessTests`) against the
  package in CI. They are what a user's Chimera effectively checks.
- **Versions from CI only.** `build-package.sh` stamps the commit and its
  date; a hand-built package says `+local` and is not published. A movie cites
  that version, so a published version must be reproducible from the
  repository.
- **One file per version, never replaced.** Publish `dev` (rolling) and dated
  `nightly-YYYY-MM-DD` releases the way the official cores do, with the asset
  named `np2kai-<version>.chimeraCore`. An old movie needs the old build, and
  Chimera cannot fetch it for anyone.
- **The licences travel with the package** (`package-licenses.json`), and the
  Core Manager shows them.
- **A core id that cannot be confused with an official core.** `np2kai` is not
  in Chimera's roster today (READ, `official-cores.json`). Check again before
  the first release.

**Two ways to build releases (open question 6, deferred until the core works well).**

- **Call Chimera's shared workflow**
  (`ToolAssisted-run/chimera/.github/workflows/publish-core.yml`), as the
  official cores do. Less to maintain, but this repository's releases then
  depend on another project's workflow, which can change under it. If chosen,
  pin it to a commit, not `@main`.
- **Carry a copy** of `tools/publish-core.sh` and the workflow in this
  repository (Chimera is MIT, so copying is allowed with its notice).
  Independent, at the cost of keeping it up to date by hand. Recommended for a
  third-party core.

**Chimera's version.** Pin the Chimera commit that CI tests against and say in
the README which Chimera versions the package was tested with. An official
core is tested against Chimera's `main` every day; a third-party core is not,
so the pin is what makes "it works" a checkable claim.

## 16. Risks, most serious first

1. **The licence of the package.** fmgen's terms, and making sure no GPL part
   is compiled in by accident. Answered by the owner and by checking the built
   objects (M8).
2. **Native and sandbox disagree because their C libraries differ** (maths
   in fmgen's tables, `rand()`). Seen in M3. Fixes in section 3.
3. **Speed of the IA-32 interpreter in the sandbox** for PC-9821 and Windows
   95 software. Measured in M3. If too slow, the core is useful for DOS-era
   PC-98 software only, which is most of the game catalogue.
4. **A frame rate that changes with the display mode** (section 4).
   Decided: a Monitor setting fixes the rate at start; software that
   switches mode itself makes the shown movie time slightly off (M4).
5. **No font without the user's font file** (section 7). Decided: the font
   file is required, so a user without a dump cannot use the core.
6. **Copy-protected floppy formats** must keep working through the in-memory
   layer. Because NP2kai's own parsers still read the bytes, they should;
   tested with the user's own disks in M10.
7. **Upstream changes quickly** on an alpha branch. Pinned; moved on purpose
   only.
8. **Code that reaches the host from places not found by this search**
   (the search skipped `windows/`, `wx/`, `x/`, `win9x/`, `jni/`,
   `sample/`). The native-against-sandbox comparison and the stall test are
   the safety net.

## 17. What in this document is not verified

Everything is read from sources; nothing is built. In particular: that
NP2kai's own BIOS boots a gate-built IPL; that the libretro defines compile
no GPL code; that the TSC, game-port and `GetTickCount` seams are reached
only as described; what the machine shows with no font; the exact frame
rates; the largest picture size; the speed in the sandbox; which PC-98 games
DOSBox-X runs. Each has a step above.

## 18. Chimera issues that could apply to this core

**Where this comes from.** The owner exported every issue of
`ToolAssisted-run/chimera` to `G:\GitHub\chimera-issues.json` on 2026-10-23
(366 entries: 359 issues and 7 pull requests, numbers 1 to 368). Each was
read by title, and the ones that could touch a PC-98 core were read in full.
The export holds each issue's first post, not its comments, so how a closed
issue was fixed is taken from Chimera's code at `7bc0d35` where it was
checked (READ), and is UNCHECKED otherwise. Issues about one console only
(PS2, PS3, Xbox, Vita and so on) and about the website are left out. The
file is not part of this repository and is not committed.

Each entry gives the issue, what happened, and what this core does about it.
"Open" means the issue was still open on 2026-10-23.

### Mouse

- **#61, #176, #210, #211 (DOSBox-X mouse position).** The absolute mouse
  position did nothing, then moved the wrong axis, then snapped back to the
  middle when the input returned to its neutral value, then jumped about
  during real-time play because the movement axes overrode the position.
  Fixed in the DOSBox-X core with a held "Mouse Set Position" button: held,
  the pointer is put at the position; not held, the position is ignored
  (READ, `chimera-core-dosbox-x/waterbox/gen-config.py`, 2026-10-08).
  **For this core:** the PC-98 bus mouse only reports movement, so the core
  offers relative movement only (section 6) and none of these can happen.
  If an absolute position is ever wanted, follow Chimera's rule (0..65535,
  neutral 32768, a fraction of the live picture, `docs/design-principles.md`)
  and the DOSBox-X "Set Position" button, and decide it with the owner.
- **#135 (move the mouse to a point).** Users asked for "move the cursor to
  x,y" instead of working out relative moves. On a PC-98 the cursor belongs
  to each game, so the core cannot know where it is. Not planned; a Lua
  helper reading the game's cursor from memory is the way, as #135's author
  did for DOSBox-X.
- **#318, #319 (TIC-80 mouse capture).** The pointer flickered after
  capture, and capture was not bound by default. **For this core:** bind
  mouse capture to the middle button by default, and check the pointer in
  the M6 input step.

### Inputs and the keyboard

- **#44 (DOSBox "last input" marker wrong).** Axes whose resting value was
  not zero made every frame look like it had input. **For this core:** every
  axis declares its real neutral value (0 for mouse movement), and the M6
  step checks that an empty frame reads as empty in TAStudio.
- **#225 (one-letter mnemonics).** A full keyboard runs out of letters.
  Fixed: a button keeps a one-character mnemonic for the movie text and can
  have a `headers` entry of up to eight characters for its column (READ,
  `docs/porting-a-core.md`). **For this core:** give every PC-98 key a
  header, including the keys a PC keyboard lacks: STOP, COPY, f.1 to f.10,
  vf.1 to vf.5, XFER, NFER, kana, GRPH, HOME CLR, HELP, ROLL UP, ROLL DOWN.
  Chimera's MnemonicUniquenessTests must pass.
- **#310, #337 (key order).** Computer keyboards were listed in an order
  nobody could search. Decided in Chimera: computers list keys row by row as
  they sit on the keyboard, with disk controls at the end. **For this core:**
  list the PC-98 keyboard row by row, then the number pad together, then
  the mouse, the joysticks, and the disk buttons last.
- **#336, #317 (default bindings).** Cores should ship keyboard and
  controller bindings, and each core's inputs should be checked. **For this
  core:** default bindings for the keyboard, and for the two joysticks on a
  game controller as well as keys.
- **#341, #113 (memory callbacks in Lua).** No Chimera core can stop part
  way through a frame, so Lua's `on_bus_read` and similar do not work. **For
  this core:** none are offered; the input-poll call (section 6) is the only
  call during a frame.

### Disks, discs and swapping

- **#47 (DOSBox CD swap not seen by Windows 98).** The image was changed in
  the drive layer, but the emulated drive never told the guest the disc had
  changed. **For this core:** a swap must go through NP2kai's own eject and
  insert path (the floppy controller's ready change, and the CD drive's
  media-changed state), not only replace the file. The M10 test swaps
  Popful Mail's floppies and a CD and checks the game sees the new disk.
- **#352 (X68000 core took two floppies at most).** A game on three disks
  could not be loaded. **For this core (user-decided, 2026-10-23):** the
  floppy slot has no upper limit (`max` -1 in `file_slots.json`), not just
  the two drives; the disk buttons choose which image is in which drive.
  Popful Mail's three disks test it.
- **#281, #286 (two discs).** Users want more than one disc in a project.
  **For this core:** the CD slot takes several images, swapped like floppies.
- **#351, #362 (open: X68000 hard disk crashes).** A hard disk image ended
  up being read as a floppy and the machine stopped with "Unable to identify
  image file format". **For this core:** each slot hands its files only to
  its own drive type, and the gate loads one image of every accepted format
  in its own slot (M10).
- **#350 (format list cut off in the wizard).** The last accepted format was
  hidden. NP2kai accepts many extensions (section 1), so the list is long.
  **For this core:** check in M9 that the wizard shows the whole list, and
  accept only formats that have been tested.
- **CloneCD discs (.ccd) — DECIDED: not accepted (owner, 2026-10-24).**
  The CD slot takes `.cue` and `.iso` only. A .ccd/.img converts to .cue
  and .bin without loss (the .img is raw 2352-byte sectors and the .ccd
  gives the track starts; only the .sub subchannel is dropped), and Chimera
  already adds a cue's .bin files to a project by itself. For a while the
  slot accepted `ccd` (Policenauts [CD] ran the same natively, in the
  sandbox and through chimera-run); the owner took it out.
- **A .cue with one .bin per track** (the usual Redump layout): NP2kai's
  cue reader uses only the first FILE line, so such a disc got a wrong
  track list ("CD cannot be used"). The driver now joins the track files
  into one read-only disc before opening it (`join_cue` in `np2-driver.c`,
  `chimera_vfs_join` in `port/dosio.c`; nothing is copied). Policenauts
  and Brandish 2 Renewal both run from their multi-bin cues.

### Firmware

- **#30, #60, #349, #359 (open), #364 (open), #368 (open).** Firmware was
  hard to check, listed twice for two core versions, not found in a folder or
  its subfolders, or reported missing on one wizard page and found on the
  next. **For this core:** NP2kai's firmware has generic names (`bios.rom`,
  `FONT.ROM`, `sound.rom`) and no fixed checksum (section 7), so it is found
  by name and size, which is the case these issues are about. Give each
  entry clear `nameHints` and `size`, state every condition with
  `requiredWhen` so both wizard pages agree, and test the wizard with the
  owner's files from `G:\sdlnp21kai (from linux)` before release.

### Clocks and random numbers

- **#139, #366 (open) (clock not a setting).** Games seeded their random
  numbers from the system clock, which was either missing from the settings
  (a desync) or fixed so every run looked the same. **For this core:** the
  calendar start time is a `datetime` setting (section 9), and NP2kai's
  `rand()` is our own seeded copy (section 3). A user can change the start
  time to change a game's luck, and a movie records it.
- **#358 (more setting types).** Fixed: settings can now be `uint`, `long`,
  `ulong` and `datetime` (READ, `WaterboxConfig.cs` line 557).

### Frame rate and timing

- **#31, #58, #346 (wrong frame rate).** A PAL game ran at 60 instead of 50,
  a PSP movie's time came out wrong, and the throttle ran slightly fast. A
  wrong rate makes movie time and sound pitch wrong. **For this core:** the
  frame rate matters (section 4). Chimera asks `GetVsyncNumerator` and
  `GetVsyncDenominator` once, after `Init` (READ, `WaterboxConfig.cs`
  `VideoConfig`, `session.cpp` line 2358), so a rate that changes with the
  display mode cannot be reported while the core runs. Decided (user,
  2026-10-23): a "Monitor" setting picks the mode at start, and the core
  reports that mode's rate (section 4).

### Memory and tools

- **#89 (RAM Search refused memory over 64 MiB).** Fixed: "there is no size
  RAM Search refuses" (READ, `RamSearch.cs`). The IA-32 machine's 13 MB is
  well within reach either way.
- **#288 (duplicate memory areas).** The same memory was listed twice.
  **For this core:** list each memory area once (section 11).
- **#216 (memory not exposed).** A core without memory areas cannot be used
  with RAM Search or Lua. **For this core:** memory areas are in the first
  version (section 11).

### The sandbox and the gate

- **#119 (exports past 32 were lost).** Fixed in miniBox. **For this core:**
  both machines share one set of exports (section 10 hides each build's own
  symbols), so the count does not double; M8 counts the exports anyway.
- **#120 (native and sandbox disagree on real games).** RPCS3's gate
  programs agreed while real games did not. **For this core:** M0b's games
  are run in both builds and their memory compared, not only the gate's own
  disk (section 14).
- **#245 (random crashes after a few hundred frames).** **For this core:** a
  long run of each test game (thousands of frames) is a gate leg.
- **#115 (core and frontend from different weeks).** A savestate written by
  one build failed in another with no reason given. **For this core:** put a
  format number in the savestate and refuse a different one with a clear
  message (section 8).
- **#26, #162, #291 (a saved project fails to reopen).** Settings were lost,
  files forgotten, or reopening failed. **For this core:** a gate leg makes
  a project, saves it, closes, reopens and replays it, and checks the same
  frames come out.
- **#260 (bad nightlies that load no game).** Each core should be tested on
  content before a release. **For this core:** the gate's own test disk does
  this on every build (section 14), and the owner's games on the owner's
  computer.

### Speed and settings text

- **#316, #23, #24 (speed, JIT by default).** Users want cores faster.
  NP2kai's CPUs are interpreters with no JIT, so there is nothing to switch
  on. Speed is measured in M3 (risk 3).
- **#194 (adjustable CPU speed in PCem).** **For this core:** the clock
  multiplier is already a setting (section 9).
- **#239, #241 (hard-to-read setting text).** Settings and file slots must
  be written in plain language. **For this core:** every description follows
  that rule (section 9), and the owner reviews them in M9.

### Picture

- **#209 (X68000 picture squashed).** The picture was drawn in the wrong
  shape. **For this core:** declare the PC-98 picture's shape (640x400 shown
  as 4:3) with the virtual width and height, and check it in M4 for 400-line
  and 480-line modes.

---

## Log

- 2026-10-23: plan written from NP2kai `5939e0c6d598` (`wx_alpha`) and
  Chimera `7bc0d35`. The core repository has no commits; this file is its
  first content. Nothing built, nothing pushed.
- 2026-10-23: the owner decided the core is built regardless of the overlap
  with the DOSBox-X core's PC-98 mode. M0b is kept, for information only.
- 2026-10-23: section 2 corrected: GPL alone is not a problem; the conflict is
  GPL code and fmgen in the same binary. Two consistent builds listed.
- 2026-10-23: the owner decided on one package with both CPUs as two machines.
  Section 10, open question 4 and M12 rewritten.
- 2026-10-23: the owner decided this is a third-party core, not expected to
  become official. Publishing rewritten (new section 15); roster, request
  issue and shared credential marked not applicable.
- 2026-10-23: publishing set-up deferred by the owner until the core is in
  good shape; question 6 to be raised again after M10.
- 2026-10-23: the owner chose build A: fmgen, no GPL part, SoftFloat 3 FPU.
- 2026-10-23: the owner chose to match NP2kai's Linux builds for random
  numbers: our own copy of glibc's `rand()`, seed 1, state in machine memory.
  The algorithm was checked against real glibc (100000 values).
- 2026-10-23: added "Benefits of this core" to section 1: operating systems
  beyond DOS, and a disk format comparison with DOSBox-X and its Chimera core.
- 2026-10-23: the owner chose core id `np2kai`, and made `FONT.ROM` or
  `font.bmp` required firmware with no free fallback font.
- 2026-10-23: the owner named `G:\sdlnp21kai (from linux)` as the folder for
  this core's firmware and files (sections 7 and 14). Tests use the owner's
  real `FONT.ROM`, no placeholder font; legs that need it SKIP on public CI.
- 2026-10-23: the owner accepted the machine ids `PC9821` and `PC98`.
- 2026-10-23: the owner's test games recorded (section 1, M0b): Flame Zapper
  Kotsujin, Popful Mail, Rogue, Rusty, Wizardry II, in `G:\GitHub\pc98examplegames`.
- 2026-10-23: the owner added CD games and Windows 98 SE to the test list:
  Brandish 2 Renewal, Canaan, Policenauts (section 1, M0b).
- 2026-10-23: the owner chose the Linux SDL build's defaults for settings
  (section 9), and set Windows 98 SE aside as a test for now.
- 2026-10-23: added section 18, the Chimera issues that could apply to this
  core, read from the owner's export of all 359 issues.
- 2026-10-23: the owner decided three items from section 18: a Monitor
  setting for the frame rate (section 4), relative mouse movement only
  (section 6), and no limit on the number of floppies.
- 2026-10-23: the owner decided the key order: row by row as on the PC-98
  keyboard, left to right from the top-left (section 6).
- 2026-10-23: keyboard picture planned (section 6): our own drawing by
  default, a Commons photo only if the owner checks its licence.
- 2026-10-24: the CD slot was given `.ccd` (CloneCD) for Policenauts [CD];
  the owner asked for the change to be evaluated (section 18, "Disks, discs
  and swapping").
- 2026-10-24: a .cue with one .bin per track is now read as one disc
  (driver `join_cue`). The owner decided against `.ccd`: it was taken out of
  the CD slot again; convert a .ccd to .cue and .bin instead.
- 2026-10-24: the owner asked for all keypad keys together: the number pad
  moved out of the keyboard rows into one block after them (np2-input.h).
  Not published yet, so no movie is affected. build-guest.sh and
  build-native.sh now rebuild a file when a header beside it changes.
- 2026-10-24: the owner made the real BIOS, ITF, sound BIOS, IDE BIOS and
  rhythm samples required firmware (their four settings removed). The real
  IDE BIOS hangs with no hard disk, so `ide.rom` is required with a hard
  disk on PC9821 (section 7, Firmware). Gate 26 of 26 with the owner's files.
- 2026-10-24: run-frontend.sh gained the lua:* legs (frontend-tools.lua on
  the counter disk, gen-testdisk.py --counter); 25 of 25 on the owner's PC.
  The owner checked RAM Search, RAM Watch and the Lua Console by hand.
- 2026-10-24: the CPU clock multiplier became a list (1, 2, 4 ... 70, default
  20), and the package is named np2kai-<its sha1>.chimeraCore; earlier
  np2kai packages are taken out of build/package and Chimera's Cores.
- 2026-10-24: sound review of all 26 boards against the chip formulas and
  ymfm; the PC-9801-14 pitch (16 times too high) and the Little Orchestra L
  crash were fixed in waterbox/port/ (section 5). run-native.c gained
  NP2_WAV=<file> to record the sound as a .wav.
- 2026-10-25: new setting Sound Generator (NP2 or fmgen, np2cfg.usefmgen),
  default NP2 (owner): fmgen's SSG sits about 6 dB higher next to the FM,
  and the owner's libTAS recording of Flame Zapper Kotsujin matches NP2's
  balance. The chips' registers, timers and interrupts are NP2kai's own
  code either way (opna.c never reads fmgen back): 2,400 frames of the game
  gave the same pictures, clocks and memory with both. The arrow keys became
  one block before the number pad (owner).
- 2026-10-25: firmware takes any dump (owner, option B). FONT.ROM is no
  longer pinned; no entry pins a hash, and the project records each file's
  own. New name hints find SNDBIOS.ROM, IDEBIOS.ROM and bios9821.rom; FONT.BMP's
  hint is font.bmp, so it never takes a FONT.ROM. The 36 known alternative
  dumps from the owner's folders are in docs/firmware-dumps.md.
