/* np2-firmware.h - the firmware files, by the names Chimera mounts them
 * under: their ids in waterbox.config, all in small letters
 * (tools/gen-config.py checks the two lists are the same).
 *
 * NP2kai and fmgen ask for these in either case (2608_bd.wav and
 * 2608_BD.WAV, FONT.ROM and font.rom). port/dosio.c puts the name asked for
 * in small letters and, when it is one of these, opens this mount. The
 * user's own file on disk may be spelled any way: Chimera's firmware
 * locator matches names without regard to case and mounts the file under
 * its id. */
#ifndef NP2_FIRMWARE_H
#define NP2_FIRMWARE_H

#define NP2_FIRMWARE_FILES(X) \
	X("font.rom") \
	X("font.bmp") \
	X("bios.rom") \
	X("sound.rom") \
	X("ide.rom") \
	X("2608_bd.wav") \
	X("2608_sd.wav") \
	X("2608_top.wav") \
	X("2608_hh.wav") \
	X("2608_tom.wav") \
	X("2608_rim.wav") \
	X("ym2608_adpcm_rom.bin")

#endif
