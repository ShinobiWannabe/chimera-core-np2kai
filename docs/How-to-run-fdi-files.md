# How to run floppy disk games (.fdi and other floppy images)

Most PC-98 floppy games are boot disks: put disk 1 in drive 1 and the game starts by itself. Some
need MS-DOS to install them first. This page shows how to run them in Chimera with the np2kai
core. Tested 2026-11-08 on main b67f492.

The core takes these floppy image types: `.fdi` `.hdm` `.d88` `.88d` `.d98` `.98d` `.xdf` `.dup`
`.2hd` `.tfd` `.nfd` `.dcp` `.dcu` `.fdd` `.hd4` `.hd5` `.hd9` `.h01` `.hdb` `.ddb` `.dd6` `.dd9`
`.flp` `.fim` `.dip` `.ima`. Everything below works the same for all of them.

## What the core does with floppies

- A project can hold any number of floppies. Their **order in the project's list** is the order the
  core uses: floppy 1 goes in drive 1 and floppy 2 in drive 2 when the PC starts. The others wait
  in the list until you swap them in.
- The **Floppies at Start** setting changes that: *Drives 1 and 2* (the default), *Drive 1 only*,
  or *None*.
- The PC-98 starts from floppy drive 1, then floppy drive 2, then the hard disk. Started from a
  floppy, **A:** is drive 1, **B:** is drive 2 and **C:** is the hard disk (if there is one).
- Inputs for changing disks while the game runs (np2kai controller):

| Input | What it does |
|---|---|
| **Drive 1/2 Next Floppy Disk** | Moves that drive's pointer to the next floppy in the list. Nothing changes on screen yet. |
| **Drive 1/2 Previous Floppy Disk** | Moves the pointer back one. |
| **Drive 1/2 Swap Floppy Disk** | Puts the floppy the pointer is on into that drive. If that floppy is in the other drive, the two drives trade disks. |
| **Drive 1/2 Eject Floppy Disk** | Takes the disk out of that drive. |
| **Reset** | The PC-98's reset switch: the PC starts again and the disks keep what was written to them. |

None of these has a default key. Bind the ones you need in **Config > Controllers** (np2kai
controller) before you start, and tick them in the TAStudio columns there if you want to see them in
TAStudio. A drive's pointer starts on the floppy that is in it at start, so with three floppies,
drive 1's pointer is on floppy 1: **Drive 1 Next**, **Drive 1 Next**, **Drive 1 Swap** puts
floppy 3 in drive 1.

Chimera's own **Reboot Core** is not the same as Reset: it loads the project's files again, so
anything the game saved to a disk is lost unless you exported it first (**Emulator > Export Save
Data**).

## Making the project

1. On the start screen choose **New Chimera Project**. Give it a title and choose the core
   **np2kai**, machine **PC9821** (the default; use **PC98** for games that need a 286-era PC).
2. On the files page, add the game's floppies to the **Floppy disks** slot. Put them in the order
   the game wants them, with disk 1 first. Use the ▲ and ▼ buttons to change the order. Add no
   hard disk or CD unless the game needs one.
3. Settings can stay at their defaults for most games.
4. Point Chimera at the firmware it asks for: the BIOS, FONT.ROM, the sound BIOS and the six
   `2608_*.wav` drum samples (the IDE BIOS only when a hard disk is in the project). See
   [firmware-dumps.md](firmware-dumps.md).
5. Create the project. TAStudio opens at frame 0, paused. Press **Play** (or the Pause hotkey) to
   run.

To change the floppies or their order later, use **File > Project Settings**. The game starts again
from frame 0.

---

# Games that start by themselves

## Rogue and Wizardry II (one disk)

| Slot | File | SHA-1 |
|---|---|---|
| Floppy disks | `Rogue (1986)(Ascii).d88` | 1c8ce5122cdee2fc95c97096695d614662b7c681 |

| Slot | File | SHA-1 |
|---|---|---|
| Floppy disks | `Wizardry II Diamond no Kishi (1986)(Ascii).fdi` | 32aef31169e6d0388592424223711c2f4a0d246d |

Make the project with the one floppy and press Play. The game boots from drive 1 and shows its
title screen after a few seconds. Nothing else is needed.

## Popful Mail (three disks)

| Slot | File | SHA-1 |
|---|---|---|
| Floppy disks (this order) | 1. `Popful Mail (1992)(Falcom)(Disk 1 of 3)(Program).fdi` | 5a96938908e787fd5c03bb45be58b5f2a9e81c1a |
| | 2. `Popful Mail (1992)(Falcom)(Disk 2 of 3)(Data).fdi` | d481aa4a317bdd4c7f7340232d5cd6c1d5bb1799 |
| | 3. `Popful Mail (1992)(Falcom)(Disk 3 of 3)(Opening).fdi` | c68ab1f57a6462c2d5f890a1d19beab35a17c804 |

With this order the Program disk is in drive 1 and the Data disk in drive 2 at start, which is how
the game wants them. The Opening disk waits in the list. Bind **Drive 1 Next Floppy Disk** and
**Drive 1 Swap Floppy Disk** first if you want to watch the opening.

1. Press Play. The game boots from drive 1 to its menu.
2. To play: press **Down**, **Down**, **Return** (はじめから冒険する, start the adventure from the
   beginning). The game runs from drives 1 and 2 and needs no swaps.
3. To watch the opening instead: press **Return** on the first item (オープニングから始める, start
   from the opening). The game asks for the Opening disk in drive A. Press **Drive 1 Next**,
   **Drive 1 Next**, **Drive 1 Swap** (the Opening disk, floppy 3, goes in drive 1), then
   **Return**. The opening plays.

When a game asks for another disk, it nearly always means the drive it names: A is drive 1, B is
drive 2. Move that drive's pointer with Next or Previous and press Swap.

---

# Games that need MS-DOS to install

Some PC-98 games are not boot disks but install from floppy to a hard disk with MS-DOS: you start
MS-DOS, run the game's installer, and afterwards start the game from the hard disk. Many later
Koei games work this way. These need a hard disk image with MS-DOS on it in the project, as well as
the game's floppies. [CDInstalls.md](CDInstalls.md) shows the same kind of install for a CD game.

## Troubleshooting

| What you see | What to do |
|---|---|
| A black screen or nothing after Play | The game disk may not be a boot disk. Check whether the game needs MS-DOS to install it. |
| The game wants a disk in a drive and nothing happens after Swap | Check the drive: A is drive 1, B is drive 2. Use Previous to step the pointer back if you went past the disk. |
| The wrong disk is in a drive at start | Change the order in **File > Project Settings** (▲ ▼), or set Floppies at Start. |
