# How to run hard disk games (.hdi and other hard disk images)

Many PC-98 games come as a hard disk image: the game (often with MS-DOS) already installed on a
disk, so the PC starts straight into it. Others are a plain MS-DOS hard disk that you install a game
onto. This page shows how to run both in Chimera with the np2kai core. Tested 2026-11-08 on main
165c475.

The core has two hard disk slots:

| Slot | Formats | Disks | What the PC gets |
|---|---|---|---|
| **Hard disks** | `.hdi` `.nhd` `.thd` `.vhd` `.slh` | up to 2 | The PC's own hard disk: IDE on the PC9821 machine, SASI on the PC98 machine. Use this one. |
| **SCSI hard disks** | `.hdd` `.hdn` `.hds` `.hdi` `.nhd` `.vhd` | up to 4 | A PC-9801-55 SCSI board with these disks on it (first = SCSI ID 0). Needed for Virtual98 `.hdd` and RaSCSI `.hdn`/`.hds` images. |

## What the core does with hard disks

- The PC-98 starts from floppy drive 1, then floppy drive 2, then the first hard disk. A hard disk
  only starts the PC when **no formatted floppy is in drive 1 or 2**.
- Started from the hard disk, **A:** is the hard disk, **B:** is floppy drive 1 and **C:** is floppy
  drive 2. (Started from a floppy it is the other way round: A: and B: are the floppies, C: the hard
  disk.)
- Everything the game writes to the hard disk (saves, installs, settings) is kept while the project
  runs. The image file on your PC is never changed. To keep the writes, use **Emulator > Export
  Save Data**: it saves the hard disk, with everything written to it, as a new image file. Give
  that file to a project to carry on from it (see *Saving* below).
- Chimera's **Reboot Core** loads the project's files again from scratch, so it loses everything
  written to the disks since the start. The core's **Reset** input is the PC-98's reset switch: the
  PC starts again and the disks keep what was written. Reset has no default key; bind it in
  **Config > Controllers** (np2kai controller) if you need it.
- With two disks in **Hard disks**, the first is the one the PC starts from (NEC's BIOS shows a menu
  to choose when both can start).

## Firmware

As for floppies: the BIOS, FONT.ROM, the sound BIOS and the six `2608_*.wav` drum samples. With a
disk in **Hard disks** on the PC9821 machine, Chimera also asks for the **PC-9821 IDE BIOS**
(`ide.rom`, 8 KiB). The PC98 machine and the SCSI slot do not need it. See
[firmware-dumps.md](firmware-dumps.md).

## Making the project

1. On the start screen choose **New Chimera Project**. Give it a title and choose the core
   **np2kai**, machine **PC9821** (the default).
2. On the files page, add the hard disk image to the **Hard disks** slot. Add no floppy unless the
   game needs one (a floppy in drive 1 or 2 would start the PC instead of the hard disk; see
   *Floppies at Start* below).
3. Settings can stay at their defaults.
4. Point Chimera at the firmware it asks for, including the IDE BIOS.
5. Create the project. TAStudio opens at frame 0, paused. Press **Play** (or the Pause hotkey) to
   run. The memory check and the hard disk start take about 20 to 30 seconds.

---

# Games that start by themselves

These images hold the game and what it needs to start. Make the project with the one hard disk
and press Play.

| Game | File | SHA-1 | What you see |
|---|---|---|---|
| Popful Mail | `Popful Mail (1992)(Falcom).hdi` | f7783c7b143811e960a663349b4dce54cfbd4187 | The game's menu after about 25 seconds. **Down**, **Down**, **Return** (はじめから冒険する) starts a new game. |
| Rusty | `Rusty (1993)(C-Lab).hdi` | c86d25e78ee9feff51b9e62f895bdaa405c08e08 | The title after about 25 seconds. **Return**, then **Z** through the story. Move with the keypad (**Keypad 4/6**). |
| Flame Zapper Kotsujin | `CO2-Pro (Flame Zapper Kotsujin).hdi` | 8e92e7b6ff87c1e45cb586149634a93740317a09 | The title (CODE NAME FLAME ZAPPER) after about 40 seconds. **Z** confirms (Return does nothing); the first item starts the game. |
| Totsugeki! Mix | `Totsugeki! Mix.hdi` | fa5007053067a2f90c079f960b555739a9c113b7 | The C-Lab logo, then the opening. **Return** and **Z** move it on. |

---

# An MS-DOS hard disk

| Slot | File | SHA-1 |
|---|---|---|
| Hard disks | `MS-DOS 6.20 (with extensions and update).hdi` | 0b36177cfb584d29588ff963ad47e2680ee304d8 |

It starts MS-DOS 6.20 and then the MS-DOS Shell (about 35 seconds). To leave the Shell for the
`A:\>` prompt, press **GRPH+f.4** (**Alt+F4** on an English keyboard). This disk is the starting
point for games that install to a hard disk; [CDInstalls.md](CDInstalls.md) installs Puyo Puyo
Tsuu onto it from CD.

## Typing on the PC-98 keyboard

On an English keyboard with the core's default bindings:

| To type | PC-98 key | Press on an English keyboard |
|---|---|---|
| `\` | ¥ (shows as ¥ on screen) | **`** (backtick, left of 1) |
| `:` | : | **'** (apostrophe) |
| `=` | SHIFT+- | **Shift+-** |
| `>` | SHIFT+. | **Shift+.** |

Upper or lower case does not matter. The PC-98 `_` key has no default binding, so the CD driver
name below is written `CD101`; it only has to be the same for NECCDD and MSCDEX.

## Brandish 2 Renewal (installed from CD onto the hard disk)

| Slot | File | SHA-1 |
|---|---|---|
| Hard disks | `MS-DOS 6.20 (with extensions and update).hdi` (a copy of the disk above) | 0b36177cfb584d29588ff963ad47e2680ee304d8 |
| CD-ROM | `Brandish 2 Renewal - The Planet Buster (Japan).cue` (add only the .cue; its 12 .bin files come with it) | |

No floppy. Firmware as above, with the IDE BIOS.

### 1. Load the CD driver at every start

Leave the Shell with **Alt+F4** and type these lines, each followed by **Return**:

```
echo DEVICE=A:\DOS\NECCDD.SYS /D:CD101>>A:\CONFIG.SYS
echo LASTDRIVE=Z>>A:\CONFIG.SYS
echo @ECHO OFF>A:\AUTOEXEC.BAT
echo PATH A:\DOS;A:\>>A:\AUTOEXEC.BAT
echo MSCDEX /D:CD101 /L:Q>>A:\AUTOEXEC.BAT
```

| Line | What it does |
|---|---|
| `echo DEVICE=...>>A:\CONFIG.SYS` | Adds (`>>`) the CD-ROM drive driver to CONFIG.SYS, with the drive name CD101. |
| `echo LASTDRIVE=Z>>A:\CONFIG.SYS` | Lets DOS have drive letters up to Z. Without it MSCDEX stops with "Not enough drive letters available". |
| `echo @ECHO OFF>A:\AUTOEXEC.BAT` | Starts a new AUTOEXEC.BAT (the single `>` replaces the old one, so the Shell no longer starts). |
| `echo PATH A:\DOS;A:\>>A:\AUTOEXEC.BAT` | Lets DOS find its commands. |
| `echo MSCDEX /D:CD101 /L:Q>>A:\AUTOEXEC.BAT` | Makes the CD drive **Q:** at every start. |

Then press **Reset** (bind it first). The start messages now end with
`Drive Q: = Driver CD101 unit 0`.

### 2. Install

```
Q:
INSTALL
```

In the Brandish2 hard disk install utility:

1. The opening screen: **Return**.
2. インストールするドライブ番号を選択して下さい (the drive to install to): **A** (IDEハードディスク) is
   already chosen: **Return**.
3. ディレクトリ名を入力 (the folder): `A:\BR2_98` is filled in: **Return**.
4. It copies the files. At インストールが終了しました press **Return**. You are back at DOS.

Only 7 files (766 KB) are copied; the rest of the game is read from the CD, so keep the CD in the
project.

### 3. Play

```
A:
CD \BR2_98
BR2
```

The Brandish 2 menu appears. It is used with the mouse: click **BR2スタート** to start.

### Next time

Export the hard disk (**Emulator > Export Save Data**) and make a new project with the exported
`.hdi` in **Hard disks** and the same .cue in **CD-ROM**. Only step 3 is needed then. To start the
game by itself, type these two lines once before you export:

```
echo CD \BR2_98>>A:\AUTOEXEC.BAT
echo BR2>>A:\AUTOEXEC.BAT
```

---

# Saving

Games on a hard disk save to it, and so does anything you install. These writes last until the
project starts again from frame 0 (opening it again, Reboot Core, or a change in **File > Project
Settings**). To keep them:

1. **Emulator > Export Save Data**. The core hands out every disk that was written to, the whole
   image with the writes in it. When only the hard disk was written to, Chimera offers to save that
   one file, named after the project, the date and time and the disk (it keeps the `.hdi` ending);
   when floppies were written to as well, it saves them all in one `.zip`. The save dialog opens
   in Chimera's Save data folder (**Config > Paths**), in a folder for the core.
2. Make a new project (or change this one in **File > Project Settings**) with the exported image
   (unzipped first, if it came in a zip) in **Hard disks** instead of the original. In the New
   Project window, remove any files carried over from the last project first.

Tested: a file written on the MS-DOS hard disk was still there after starting a new project from
the exported image.

# Hard disk and floppies together

Some games want a floppy as well (a user disk, or an install from floppies onto the hard disk). A
formatted floppy in drive 1 or 2 starts the PC instead of the hard disk, so:

- To start from the **hard disk**, set **Floppies at Start** to **None** and put the floppy in later
  with **Drive 1 Swap Floppy Disk** (or Drive 2). See
  [How-to-run-fdi-files.md](How-to-run-fdi-files.md) for the floppy inputs.
- To start from an **MS-DOS floppy** (for an install onto a blank hard disk), put that floppy first
  in **Floppy disks**. The hard disk is then **C:**.

# SCSI hard disks

Use the **SCSI hard disks** slot for Virtual98 `.hdd` and RaSCSI `.hdn`/`.hds` images, which are
SCSI disks. The PC gets a PC-9801-55 SCSI board with the core's own SCSI BIOS, so no extra firmware
is needed. The PC starts from the first SCSI disk when no floppy is in a drive and nothing is in
**Hard disks**. `.hdi`, `.nhd` and `.vhd` can go in either slot; use **Hard disks** unless the game
or disk was made for SCSI. (Tested: the MS-DOS 6.20 and Popful Mail images above also start from the
SCSI slot.)

## Troubleshooting

| What you see | What to do |
|---|---|
| A floppy game, "No system files", or a black screen instead of the hard disk | A formatted floppy is in drive 1 or 2 and the PC started from it. Remove the floppies, or set **Floppies at Start** to **None**. |
| `How many files(0-15)?` (N-88 BASIC) | The PC found nothing it could start from: the hard disk image is blank, unformatted, or holds no start-up code the IDE BIOS accepts. Start from an MS-DOS floppy instead (put it first in **Floppy disks**), or check the image. |
| Chimera asks for the PC-9821 IDE BIOS | It is needed whenever **Hard disks** has a disk on the PC9821 machine. See [firmware-dumps.md](firmware-dumps.md). |
| The game's save or install is gone | The project started again from frame 0. Export the hard disk before closing (see *Saving*) and use the exported image. |
| `Not enough drive letters available` from MSCDEX | Add `LASTDRIVE=Z` to CONFIG.SYS (see Brandish 2, step 1). |
