/* rhythm-rom.c - the YM2608's six drums, from the chip's own ROM or from the
 * six 2608_*.WAV files.
 *
 * The YM2608 holds its drums in an 8 KiB ROM inside the chip, as 4-bit
 * ADPCM (the YM2610's ADPCM-A). MAME dumps it as ym2608_adpcm_rom.bin; the
 * WAV files PC-98 emulators share are recordings of the same drums. With
 * the setting rhythmSamples on "YM2608 ROM" the drums are decoded from the
 * ROM here, as ymfm decodes them (ymfm_adpcm.cpp, adpcm_a_channel::clock;
 * the drum addresses from ymfm_opn.cpp, ym2608::reset), into WAV files in
 * memory that both sound generators read in place of the mounted files:
 * NP2's through sound/rhythmc.c below, fmgen's through fmgen-file-rom.cpp.
 * The chip plays the bass drum, snare, top cymbal and hi-hat at its output
 * rate over 3 (7987200 / 144 / 3, 18489 Hz) and the tom-tom and rim shot at
 * half that (9244 Hz). The 12-bit result is put at the top of 16 bits, so
 * the drums come out as loud as the WAV files, which are near full scale.
 *
 * sound/rhythmc.c is compiled here, unchanged, with the file it loads each
 * drum from sent through rhythm_regfile.
 *
 * Only the sound changes, never what the machine does.
 */
#include <compiler.h>
#include <stdlib.h>
#include <string.h>
#include <dosio.h>
#include <sound/pcmmix.h>
#include "chimera-port.h"

#define pcmmix_regfile rhythm_regfile
static BRESULT rhythm_regfile(PMIXDAT *dat, const OEMCHAR *fname, UINT rate);
#include "../../extern/NP2kai/sound/rhythmc.c"
#undef pcmmix_regfile

#define ROM_NAME "ym2608_adpcm_rom.bin"
#define ROM_SIZE 8192
#define WAV_HEADER 44

int chimera_rhythm_rom;

static const struct {
	const char *name;
	UINT start, end;	/* bytes in the ROM, end included */
	UINT rate;
} s_drum[6] = {
	{ "2608_bd.wav",  0x0000, 0x01bf, 18489 },
	{ "2608_sd.wav",  0x01c0, 0x043f, 18489 },
	{ "2608_top.wav", 0x0440, 0x1b7f, 18489 },
	{ "2608_hh.wav",  0x1b80, 0x1cff, 18489 },
	{ "2608_tom.wav", 0x1d00, 0x1f7f, 9244 },
	{ "2608_rim.wav", 0x1f80, 0x1fff, 9244 },
};

static UINT8 *s_image[6];
static UINT s_imagesize[6];
static int s_decoded;	/* 1 done, -1 no ROM */

static void put16(UINT8 *p, UINT v) { p[0] = (UINT8)v; p[1] = (UINT8)(v >> 8); }
static void put32(UINT8 *p, UINT v) { put16(p, v & 0xffff); put16(p + 2, v >> 16); }

static UINT8 *wav_image(UINT samples, UINT rate)
{
	UINT8 *p = (UINT8 *)calloc(1, WAV_HEADER + samples * 2);
	if (!p) return NULL;
	memcpy(p, "RIFF", 4); put32(p + 4, 36 + samples * 2);
	memcpy(p + 8, "WAVEfmt ", 8); put32(p + 16, 16);
	put16(p + 20, 1); put16(p + 22, 1);	/* PCM, mono */
	put32(p + 24, rate); put32(p + 28, rate * 2);
	put16(p + 32, 2); put16(p + 34, 16);
	memcpy(p + 36, "data", 4); put32(p + 40, samples * 2);
	return p;
}

/* ymfm's ADPCM-A decoder: a 12-bit accumulator that wraps, high nibble
 * first, from the start of the sample each key-on */
static void decode(const UINT8 *rom)
{
	static const UINT16 steps[49] = {
		 16,  17,  19,  21,  23,  25,  28,  31,  34,  37,  41,  45,  50,
		 55,  60,  66,  73,  80,  88,  97, 107, 118, 130, 143, 157, 173,
		190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598,
		658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
	};
	static const SINT8 stepinc[8] = { -1, -1, -1, -1, 2, 5, 7, 9 };
	int i;
	for (i = 0; i < 6; i++) {
		UINT samples = (s_drum[i].end + 1 - s_drum[i].start) * 2, a, n = 0;
		UINT8 *img = wav_image(samples, s_drum[i].rate);
		int acc = 0, step = 0;
		if (!img) continue;
		for (a = s_drum[i].start; a <= s_drum[i].end; a++) {
			int k;
			for (k = 0; k < 2; k++) {
				int d = k ? (rom[a] & 15) : (rom[a] >> 4);
				int delta = (2 * (d & 7) + 1) * steps[step] / 8;
				SINT16 v;
				acc = (acc + ((d & 8) ? -delta : delta)) & 0xfff;
				step += stepinc[d & 7];
				step = step < 0 ? 0 : step > 48 ? 48 : step;
				v = (SINT16)(acc << 4);	/* sign-extended 12 bits, at the top */
				put16(img + WAV_HEADER + n++ * 2, (UINT16)v);
			}
		}
		s_image[i] = img;
		s_imagesize[i] = WAV_HEADER + samples * 2;
	}
}

int chimera_rhythm_image(const char *name, const uint8_t **data, uint32_t *size)
{
	const char *fw = chimera_firmware_name(name);
	int i;
	if (!chimera_rhythm_rom || !fw) return 0;
	if (!s_decoded) {
		UINT8 rom[ROM_SIZE];
		FILEH f = file_open_rb(ROM_NAME);
		s_decoded = -1;
		if (f != FILEH_INVALID) {
			if (file_getsize(f) == ROM_SIZE && file_read(f, rom, ROM_SIZE) == ROM_SIZE) {
				decode(rom);
				s_decoded = 1;
			}
			file_close(f);
		}
	}
	if (s_decoded < 0) return 0;	/* no ROM: no drums, as with no WAV files */
	for (i = 0; i < 6; i++) {
		if (!strcmp(fw, s_drum[i].name)) {
			*data = s_image[i];
			*size = s_imagesize[i];
			return s_image[i] != NULL;
		}
	}
	return 0;
}

static BRESULT rhythm_regfile(PMIXDAT *dat, const OEMCHAR *fname, UINT rate)
{
	const uint8_t *img;
	uint32_t size;

	if (!chimera_rhythm_rom)
		return pcmmix_regfile(dat, fname, rate);
	if (!chimera_rhythm_image(file_getname(fname), &img, &size))
		return FAILURE;
	return pcmmix_regist(dat, (void *)img, size, rate);
}
