# Installing CD games

Many PC-98 CD games are not boot discs. They are DOS programs that have to be installed to a hard
disk from DOS, with a CD-ROM driver loaded for the install. This page gives tested steps for such
games in Chimera with the np2kai core.

What the core gives for this:

- **Reset** input: the PC-98's reset switch. The PC starts again and every disk keeps what was
  written to it, and the CD goes back in. Chimera's **Reboot Core** is different: it loads the
  project's files again from scratch, so everything written to the disks is lost.
- **Drive 1/2 Next Floppy Disk** and **Drive 1/2 Swap Floppy Disk** inputs. Next moves a drive's
  pointer to the next floppy in the project's list (nothing changes on screen); Swap puts the
  floppy the pointer is on into that drive. If that floppy is already in the other drive, the two
  drives trade disks.
- None of these inputs has a default key. Bind them in Chimera's controller settings (the np2kai
  controller) before you start.

Needs nightly-2026-10-29 or later (Reset, and Swap trading the drives).

## Typing on the PC-98 keyboard

On an English keyboard with the core's default bindings:

| To type | PC-98 key | Press on an English keyboard |
|---|---|---|
| `\` | ¥ (shows as ¥ on screen) | **`** (backtick, left of 1) |
| `:` | : | **'** (apostrophe) |
| `=` | SHIFT+- | **Shift+-** |
| `>` | SHIFT+. | **Shift+.** |

Upper or lower case does not matter. The CD driver name below is written `CD101` (no `_`), because
the PC-98 `_` key has no default binding; the name only has to be the same for NECCDD and MSCDEX.

---

# Puyo Puyo Tsuu (Puyo Puyo 2), with a boot disk and the disk swap

The game's installer writes a boot disk onto a blank floppy in drive 2. The floppy swap then moves
that disk to drive 1, and Reset starts the game from it. Neither the hard disk's nor the MS-DOS
floppy's CONFIG.SYS is changed: EMS memory and the CD driver are loaded from the prompt with NEC's
ADDDRV, only for the install. Tested 2026-10-29.

The installer asks for 560 KB of free memory and 2 MB of EMS memory. The boot disk it makes gives
the game that.

## Files

Machine **PC9821**, default settings.

| Slot | File | SHA-1 |
|---|---|---|
| Floppy disks (this order) | 1. `MS-DOS 6.2 (199x)(Microsoft)(Disk 1 of 1)(System)[m].fdi` | 9555bb6c3702b0fc6070567432784447cfff3408 |
| | 2. `Blank disk (MS-DOS format).hdm`, a formatted blank 2HD floppy (becomes the boot disk) | 1083a3bfc70ab1307adeb096c34f883594d4fc58 |
| Hard disks | `MS-DOS 6.20 (with extensions and update).hdi` | 0b36177cfb584d29588ff963ad47e2680ee304d8 |
| CD-ROM | `Puyo Puyo Tsuu (Japan).cue` (add only the .cue; its three .bin files come with it) | 0ed2b72c7eda806e17df8e22e6bd3686ee92df2b |

The CD's tracks: Track 1 edc42a08a2792807d6daae696b990ee3f6c3cb38, Track 2
1a52ffa8b8272ad95eec2b475c977151e98f57c6, Track 3 d99b57af923b60e5d55981a96abf8b0bf91171b9.

Firmware: the real BIOS, FONT.ROM, the sound BIOS, the IDE BIOS (a hard disk is in the project) and
the six `2608_*.wav` drum samples, as Chimera asks for them. See [firmware-dumps.md](firmware-dumps.md).

The MS-DOS floppy in drive 1 starts the PC. A PC-98 tries floppy drive 1, then drive 2, then the
hard disk, so with a formatted floppy in drive 2 the hard disk would not start anyway. Starting
from the floppy, **A:** is the MS-DOS floppy, **B:** the blank floppy and **C:** the hard disk.

Files used: from the MS-DOS floppy `A:\DOS\SYS.EXE`, `A:\DOS\EMM386.EXE`, `A:\DOS\ADDDRV.EXE`
(found through the floppy's PATH, `A:\DOS`) and `A:\DOS\HIMEM.SYS` (its CONFIG.SYS loads it); from
the hard disk `C:\DOS\NECCDD.SYS` and `C:\DOS\MSCDEX.EXE`.

## 1. Prepare the blank floppy and load EMS and the CD driver

It starts in the FILMTN file manager. Press **Q** for the `A:\>` prompt, then type:

```
SYS B:
echo DEVICE=A:\DOS\EMM386.EXE>C:\PUYOCD.SYS
echo DEVICE=C:\DOS\NECCDD.SYS /D:CD101>>C:\PUYOCD.SYS
ADDDRV C:\PUYOCD.SYS
C:\DOS\MSCDEX /D:CD101
```

| Line | What it does |
|---|---|
| `SYS B:` | Copies the DOS system files (IO.SYS, MSDOS.SYS, COMMAND.COM) onto the blank floppy, so it can start the PC. The installer only accepts a floppy that already has them. Ends with システムが転送されました. |
| `echo DEVICE=A:\DOS\EMM386.EXE>C:\PUYOCD.SYS` | Starts a new list file C:\PUYOCD.SYS (on the hard disk, which has room) with one line: the EMM386 driver, for the EMS memory the installer needs. Without it the installer stops with 割り込み番号 67H. |
| `echo DEVICE=C:\DOS\NECCDD.SYS /D:CD101>>C:\PUYOCD.SYS` | Adds (`>>`) a second line: the CD-ROM drive driver from the hard disk, with the drive name CD101. |
| `ADDDRV C:\PUYOCD.SYS` | NEC's ADDDRV loads both drivers in that list now, without a reboot and without CONFIG.SYS. |
| `C:\DOS\MSCDEX /D:CD101` | Makes the CD101 drive a DOS drive. It ends with `Drive D: = Driver CD101 unit 0`. |

## 2. Install, making the boot disk on drive B

```
D:
INSTALL
```

| Line | What it does |
|---|---|
| `D:` | Go to the CD (drive D:). |
| `INSTALL` | Run the game's installer from the CD. |

1. **1** (NEC PC-9821) and RETURN.
2. The boot floppy notice (put a formatted system floppy in a drive; step 1 did that): **RETURN**.
3. メモリが足りません (7 KB short; the boot disk fixes that): **RETURN**.
4. インストール先を指定します:
   - **Right** twice: install drive **C** (the hard disk).
   - 起動ディスクの作成 is already 作成する (make) and 起動ディスクを作成するドライブ is already **B**.
     The directory stays \PUYO2.
   - **Down** four times to 変更完了, **RETURN**.
5. Settings: **Down** twice, **Right** (joystick 使用する), **Down**, **RETURN**.
6. **Left** (はじめる) and **RETURN**. Copying takes a few minutes; at
   ぷよぷよ通のインストールがおわりました press **RETURN**.

The boot disk in drive B now holds the DOS system files, HIMEM.SYS, EMM386.EXE, CHKINST.EXE,
CONFIG.SYS (HIMEM, EMM386), AUTOEXEC.BAT (`chkinst`, `PUYO.BAT`) and PUYO.BAT (`C:`, `CD \PUYO2`,
`PUYO2`). The game is in `C:\PUYO2` on the hard disk.

## 3. Swap the boot disk into drive 1 and start the game

The boot disk is in drive 2, and drive 1 still holds the MS-DOS floppy, which the PC would start
from.

1. **Drive 1 Next Floppy Disk** once. Drive 1's pointer moves from floppy 1 (MS-DOS) to floppy 2
   (the boot disk). Nothing changes on screen yet.
2. **Drive 1 Swap Floppy Disk**. The boot disk is in drive 2, so the two drives trade disks: drive 1
   now holds the boot disk and drive 2 the MS-DOS floppy. Both keep what was written to them.
3. **Reset**. The PC starts from the boot disk, and its AUTOEXEC.BAT starts the game by itself
   (the title comes after about 25 seconds).

The Reset is needed. Without it, DOS still has the CD driver loaded, which leaves 513 KB free, and
the game stops with コンベンショナルメモリが足りません (44 KB short). The game does not need the CD
driver to play.

## 4. Next time

Whenever the project starts again, the floppies go back in list order, so drive 1 holds the MS-DOS
floppy again and step 3 is needed once more. To skip that:

1. **Emulator > Export Save Data**. It saves the boot disk and the hard disk with everything written
   to them, as separate image files.
2. Make a new project with floppy disks **1.** the exported boot disk (it has to be first) and,
   optionally, **2.** the MS-DOS floppy; hard disk the exported HDI; CD-ROM the cue. In the New
   Project window, remove any files carried over from the last project first.

That project starts the game by itself every time, with nothing to press.

## Controls

RETURN or pad button 1 at the title screen; ひとりでぷよぷよ (one player) is the first mode. Move
with the **arrow keys**, keypad 4/6 or the pad; turn the pair with **Z** / **X** or pad button 1.

## Sound Blaster 16 (optional)

The game also plays through a Sound Blaster 16. Set the core's **Sound Board** to
**Sound Blaster 16** (or **PC9801-86 + Sound Blaster 16**). In the installer's settings screen
press **Right** once on サウンド first, then do the rest as above; on the SoundBlaster16の設定
screen that follows, the defaults (I/O port D2, DMA 3, drive A) match the core: **Down** three
times and **RETURN**. The boot disk's CONFIG.SYS then names Creative's driver
(`A:\SB16\DRV\CTSB16J.SYS`), which is not on the disk; the game does not need it.
