/* np2-settings.h - the settings whose values are names, and what each name
 * means to NP2kai.
 *
 * tools/gen-config.py reads these tables to write the options of the
 * settings in waterbox.config, so the package and the driver cannot
 * disagree. The first entry of each table is the default.
 */
#ifndef NP2_SETTINGS_H
#define NP2_SETTINGS_H

/* soundBoard: (option, SOUNDID). The names are the ones NP2kai's libretro
 * core options use (sdl/libretro/libretro.c), so a user of that core meets
 * the same words. The default is the PC-9801-86, the Linux SDL build's
 * (pccore.c, SOUND_SW 0x04). */
#define NP2_SOUND_BOARDS(X) \
	X("PC9801-86", 0x04) \
	X("None", 0x00) \
	X("PC9801-14", 0x01) \
	X("PC9801-26K", 0x02) \
	X("PC9801-26K + 86", 0x06) \
	X("PC9801-86 + 118(B460)", 0x68) \
	X("PC9801-86 + Mate-X PCM(B460)", 0x64) \
	X("PC9801-86 + Speak Board", 0x24) \
	X("PC9801-118", 0x08) \
	X("Mate-X PCM", 0x60) \
	X("Speak Board", 0x20) \
	X("Spark Board", 0x40) \
	X("Sound Orchestra", 0x32) \
	X("Sound Orchestra-V", 0x82) \
	X("Little Orchestra L", 0x22) \
	X("Multimedia Orchestra", 0x83) \
	X("Sound Blaster 16", 0x41) \
	X("PC9801-86 + Sound Blaster 16", 0x44) \
	X("Mate-X PCM + Sound Blaster 16", 0x43) \
	X("PC9801-118 + Sound Blaster 16", 0x45) \
	X("PC9801-86 + Mate-X PCM(B460) + Sound Blaster 16", 0x42) \
	X("PC9801-86 + 118(B460) + Sound Blaster 16", 0x46) \
	X("AMD-98", 0x80) \
	X("WaveStar", 0x70) \
	X("Otomi-chanx2", 0x30) \
	X("Otomi-chanx2 + 86", 0x50)

/* soundGenerator: (option, np2cfg.usefmgen). Which code makes the sound of
 * the FM chips: NP2's own generators, or fmgen (cisc's YM2608 emulator),
 * which NP2kai's builds start with. Only the sound differs: the chips'
 * registers, timers and interrupts are NP2kai's own code either way
 * (sound/opna.c never reads fmgen back). The default is NP2 (owner,
 * 2026-10-25): its SSG sits about 6 dB lower next to the FM than fmgen's,
 * which matches the owner's recording of Flame Zapper Kotsujin. */
#define NP2_SOUND_GENERATORS(X) \
	X("NP2", 0) \
	X("fmgen", 1)

/* rhythmSamples: (option, chimera_rhythm_rom). Where the YM2608's six drums
 * come from: the six 2608_*.WAV files PC-98 emulators share (the default,
 * as before), or the chip's own ROM as MAME dumps it, ym2608_adpcm_rom.bin,
 * decoded by port/rhythm-rom.c. Only the sound differs. */
#define NP2_RHYTHM_SAMPLES(X) \
	X("WAV files", 0) \
	X("YM2608 ROM", 1)

/* monitor: (option, GDC dot clock in Hz, dots a line, lines a frame).
 *
 * Chimera asks for the frame rate once, after Init, so the core reports
 * the rate of the display mode the Monitor setting names (docs/PLAN.md
 * section 4, user-decided). Each row is the mode as io/gdc.c times it
 * (gdc_updateclock): the clock of gdcclk[], and the line and frame lengths
 * its sync parameters give after gdcclk[]'s limits - 24 kHz from
 * defsyncm24, 31 kHz from bios18.c's gdcmastersync "31" and "31-480". A
 * frame is then (baseclock * lines) / (clock / dots) CPU base clocks,
 * truncated as gdc.c truncates it. */
#define NP2_MONITORS(X) \
	X("24khz", 21052600 / 8, 106, 440) \
	X("31khz", 25260000 / 8, 100, 449) \
	X("31khz480", 25260000 / 8, 100, 525)

/* floppiesAtStart: (option, how many drives get a disk at power-on). A
 * formatted floppy in drive 1 or 2 is booted before the hard disk, so a
 * project that boots its hard disk and uses floppies later starts with the
 * drives empty; Swap then puts the selected disk in. The default is both,
 * as before. */
#define NP2_FLOPPIES_AT_START(X) \
	X("Drives 1 and 2", 2) \
	X("Drive 1 only", 1) \
	X("None", 0)

/* baseClock: (option, Hz). pccore.c picks between these two. */
#define NP2_BASE_CLOCKS(X) \
	X("2.4576 MHz", 2457600) \
	X("1.9968 MHz", 1996800)

#endif
