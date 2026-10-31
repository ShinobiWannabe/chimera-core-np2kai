/* chd-cd.c - MAME's compressed CD images (.chd) for NP2kai's CD-ROM drive.
 *
 * NP2kai reads CD images through its own drivers (diskimage/cd), and none
 * of them reads a CHD. Rather than add one to NP2kai, a CHD is shown to
 * NP2kai as the image it was made from: a cue sheet of one FILE and a raw
 * image of 2352-byte sectors, track after track. The sheet is a small file
 * made once (chimera_vfs_text); the image is a file of the file layer whose
 * bytes are made when NP2kai reads them (chimera_vfs_virtual): the sectors'
 * hunk is read from the CHD and decoded with libchdr (MAME's CHD reader as a
 * C library, BSD-3-Clause, extern/libchdr). Nothing is decoded ahead, and
 * one decoded hunk (8 sectors) is kept for the next read.
 *
 * What a CHD keeps differently from a raw image:
 * - each sector is a 2448-byte frame, the 2352 bytes of the sector and then
 *   96 of subcode, which the image leaves out;
 * - each track's frames are padded to a multiple of 4 (CD_TRACK_PADDING);
 *   the padding is left out;
 * - audio samples are big-endian, and a raw image's are little-endian;
 * - a track's pregap is in its frames when its PGTYPE starts with V, and is
 *   not stored at all otherwise (the sheet then says PREGAP);
 * - a data track may be kept "cooked", the 2048 (MODE1) or 2336 (MODE2)
 *   bytes of user data alone; the sector around them is made as a drive
 *   reads it: sync, header with the sector's address, and for MODE1 the
 *   EDC and ECC.
 * Any other kind of track (MODE2_FORM1, MODE2_FORM2, MODE2_FORM_MIX), a
 * CHD that needs a parent, or a CHD that is not a CD is refused: the CD
 * does not open, as a broken image does not.
 *
 * Both builds use this file. libchdr does its own file reading through the
 * FILE the mount gives (chd_open_file); it seeks before every hunk it
 * reads, so a savestate taken with the file anywhere reads the same after.
 */
#include <compiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dosio.h>
#include <libchdr/chd.h>
#include <libchdr/cdrom.h>
#include "chimera-port.h"
#include "chd-cd.h"

#define CHD_MAX_TRACKS 99
#define SECTOR 2352

typedef struct {
	int type;	/* CD_TRACK_* */
	UINT32 frames;	/* frames stored in the CHD (with a V pregap) */
	UINT32 chdframe;	/* its first frame in the CHD */
	UINT64 imagesector;	/* its first sector in the image */
	UINT32 pregap;	/* pregap frames, in the CHD when pgstored */
	int pgstored;
} CHDTRACK;

typedef struct {
	chd_file *chd;
	FILE *file;
	UINT32 framesperhunk;
	UINT32 hunkbytes;
	UINT8 *hunk;	/* the last hunk decoded, and its number (-1: none) */
	INT64 hunknum;
	int ntracks;
	CHDTRACK trk[CHD_MAX_TRACKS];
	UINT64 sectors;
} CHDCD;

/* ------------------------------------------------------------ sectors */

static UINT32 s_edc[256];

static void edc_init(void)
{
	UINT32 i, j, e;
	if (s_edc[1]) return;
	for (i = 0; i < 256; i++) {
		e = i;
		for (j = 0; j < 8; j++)
			e = (e >> 1) ^ ((e & 1) ? 0xd8018001u : 0);
		s_edc[i] = e;
	}
}

static UINT8 bcd(UINT32 v) { return (UINT8)(((v / 10) << 4) | (v % 10)); }

/* sync and header of the sector at address lba, as a drive reads it */
static void sector_head(UINT8 *s, UINT64 lba, UINT8 mode)
{
	UINT32 a = (UINT32)lba + 150;
	s[0] = 0;
	memset(s + 1, 0xff, 10);
	s[11] = 0;
	s[12] = bcd(a / 4500);
	s[13] = bcd(a / 75 % 60);
	s[14] = bcd(a % 75);
	s[15] = mode;
}

/* the 2352 bytes of a MODE1 sector around its 2048 of user data, already
 * at s + 16: header, EDC over bytes 0-2063, zeros, then P and Q ECC */
static void mode1_sector(UINT8 *s, UINT64 lba)
{
	UINT32 e = 0, i;
	sector_head(s, lba, 1);
	for (i = 0; i < 2064; i++)
		e = (e >> 8) ^ s_edc[(e ^ s[i]) & 0xff];
	s[2064] = (UINT8)e;
	s[2065] = (UINT8)(e >> 8);
	s[2066] = (UINT8)(e >> 16);
	s[2067] = (UINT8)(e >> 24);
	memset(s + 2068, 0, 8);
	ecc_generate(s);
}

/* frame `frame` of the CHD, its 2448 bytes; NULL when it cannot be read */
static const UINT8 *chd_frame(CHDCD *c, UINT32 frame)
{
	UINT32 h = frame / c->framesperhunk;
	if (c->hunknum != (INT64)h) {
		c->hunknum = -1;
		if (chd_read(c->chd, h, c->hunk) != CHDERR_NONE) return NULL;
		c->hunknum = h;
	}
	return c->hunk + (size_t)(frame % c->framesperhunk) * CD_FRAME_SIZE;
}

/* image sector `sector` (of track t) into s, all 2352 bytes */
static void make_sector(CHDCD *c, int t, UINT64 sector, UINT8 *s)
{
	const CHDTRACK *k = &c->trk[t];
	UINT32 i = (UINT32)(sector - k->imagesector);
	const UINT8 *f = chd_frame(c, k->chdframe + i);
	/* the sector's address on the disc: the sectors before it, and the
	 * pregaps before it that the image does not hold */
	UINT64 lba = sector;
	int j;
	for (j = 0; j <= t; j++)
		if (!c->trk[j].pgstored && j > 0) lba += c->trk[j].pregap;
	if (!f) { memset(s, 0, SECTOR); return; }
	switch (k->type) {
	case CD_TRACK_AUDIO:
		for (j = 0; j < SECTOR; j += 2) {
			s[j] = f[j + 1];
			s[j + 1] = f[j];
		}
		break;
	case CD_TRACK_MODE1:
		memcpy(s + 16, f, 2048);
		mode1_sector(s, lba);
		break;
	case CD_TRACK_MODE2:
		sector_head(s, lba, 2);
		memcpy(s + 16, f, 2336);
		break;
	default:	/* MODE1_RAW, MODE2_RAW */
		memcpy(s, f, SECTOR);
		break;
	}
}

/* chimera_vfs_virtual's make(): the image's bytes [off, off+len) */
static void chd_image_read(void *ctx, uint64_t off, uint8_t *dst, unsigned len)
{
	CHDCD *c = (CHDCD *)ctx;
	UINT8 s[SECTOR];
	int t = 0;
	while (len) {
		UINT64 sector = off / SECTOR;
		unsigned at = (unsigned)(off % SECTOR), n = SECTOR - at;
		if (n > len) n = len;
		while (t + 1 < c->ntracks && c->trk[t + 1].imagesector <= sector) t++;
		while (t > 0 && c->trk[t].imagesector > sector) t--;
		if (sector < c->sectors) {
			make_sector(c, t, sector, s);
			memcpy(dst, s + at, n);
		}
		else memset(dst, 0, n);
		dst += n; off += n; len -= n;
	}
}

/* ------------------------------------------------------------ tracks */

static int track_type(const char *name)
{
	static const struct { const char *name; int type; } types[] = {
		{ "MODE1", CD_TRACK_MODE1 }, { "MODE1/2048", CD_TRACK_MODE1 },
		{ "MODE1_RAW", CD_TRACK_MODE1_RAW }, { "MODE1/2352", CD_TRACK_MODE1_RAW },
		{ "MODE2", CD_TRACK_MODE2 }, { "MODE2/2336", CD_TRACK_MODE2 },
		{ "MODE2_RAW", CD_TRACK_MODE2_RAW }, { "MODE2/2352", CD_TRACK_MODE2_RAW },
		{ "AUDIO", CD_TRACK_AUDIO },
	};
	size_t i;
	for (i = 0; i < sizeof types / sizeof types[0]; i++)
		if (!strcmp(name, types[i].name)) return types[i].type;
	return -1;
}

/* the tracks from the CHD's metadata (CHT2, or the older CHTR with no
 * pregaps); 0 when one cannot be used */
static int read_tracks(CHDCD *c)
{
	char meta[256], type[32], sub[32], pgtype[32], pgsub[32];
	UINT32 chdframe = 0;
	UINT64 imagesector = 0;
	int i;
	for (i = 0; i < CHD_MAX_TRACKS; i++) {
		int num = 0, frames = 0, pregap = 0, postgap = 0, kind;
		CHDTRACK *k = &c->trk[i];
		pgtype[0] = '\0';
		if (chd_get_metadata(c->chd, CDROM_TRACK_METADATA2_TAG, i, meta, sizeof meta - 1, NULL, NULL, NULL) == CHDERR_NONE) {
			meta[sizeof meta - 1] = '\0';
			if (sscanf(meta, CDROM_TRACK_METADATA2_FORMAT, &num, type, sub, &frames, &pregap, pgtype, pgsub, &postgap) != 8)
				return 0;
		}
		else if (chd_get_metadata(c->chd, CDROM_TRACK_METADATA_TAG, i, meta, sizeof meta - 1, NULL, NULL, NULL) == CHDERR_NONE) {
			meta[sizeof meta - 1] = '\0';
			if (sscanf(meta, CDROM_TRACK_METADATA_FORMAT, &num, type, sub, &frames) != 4)
				return 0;
		}
		else break;
		kind = track_type(type);
		if (num != i + 1 || kind < 0 || frames <= 0 || pregap < 0 || (pgtype[0] == 'V' && pregap > frames))
			return 0;
		k->type = kind;
		k->frames = (UINT32)frames;
		k->chdframe = chdframe;
		k->imagesector = imagesector;
		k->pregap = (UINT32)pregap;
		k->pgstored = pgtype[0] == 'V';
		chdframe += (UINT32)((frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING * CD_TRACK_PADDING);
		imagesector += (UINT64)frames;
	}
	c->ntracks = i;
	c->sectors = imagesector;
	return i > 0;
}

/* the cue sheet NP2kai reads (diskimage/cd/cdd_cue.c: TRACK, PREGAP, INDEX) */
static void write_sheet(const CHDCD *c, const char *bin, char *sheet, size_t size)
{
	size_t used = (size_t)snprintf(sheet, size, "FILE \"%s\" BINARY\n", file_getname(bin));
	int i;
	for (i = 0; i < c->ntracks && used < size - 96; i++) {
		const CHDTRACK *k = &c->trk[i];
		UINT64 p = k->imagesector;
		const char *mode = k->type == CD_TRACK_AUDIO ? "AUDIO" :
			(k->type == CD_TRACK_MODE1 || k->type == CD_TRACK_MODE1_RAW) ? "MODE1/2352" : "MODE2/2352";
		used += (size_t)snprintf(sheet + used, size - used, "TRACK %02d %s\n", i + 1, mode);
		if (k->pregap && k->pgstored) {
			used += (size_t)snprintf(sheet + used, size - used, "INDEX 00 %02u:%02u:%02u\n",
				(unsigned)(p / 4500), (unsigned)(p / 75 % 60), (unsigned)(p % 75));
			p += k->pregap;
		}
		else if (k->pregap)
			used += (size_t)snprintf(sheet + used, size - used, "PREGAP %02u:%02u:%02u\n",
				(unsigned)(k->pregap / 4500), (unsigned)(k->pregap / 75 % 60), (unsigned)(k->pregap % 75));
		used += (size_t)snprintf(sheet + used, size - used, "INDEX 01 %02u:%02u:%02u\n",
			(unsigned)(p / 4500), (unsigned)(p / 75 % 60), (unsigned)(p % 75));
	}
}

/* ------------------------------------------------------------ the CD */

int chimera_chd_cue(const char *path, char *out, size_t outsize)
{
	static char sheet[8192];
	char bin[MAX_PATH], cue[MAX_PATH];
	const chd_header *h;
	CHDCD *c;
	chd_error err;
	if (milstr_cmp(file_getext(path), "chd")) return 0;
	c = (CHDCD *)calloc(1, sizeof *c);
	if (!c) return 0;
	c->hunknum = -1;
	c->file = chimera_vfs_mount_open(path);
	if (!c->file) goto fail;
	err = chd_open_file(c->file, CHD_OPEN_READ, NULL, &c->chd);
	if (err != CHDERR_NONE) {
		fprintf(stderr, "np2kai: %s is not a CHD libchdr can open: %s\n", file_getname(path), chd_error_string(err));
		goto fail;
	}
	h = chd_get_header(c->chd);
	if (!h || h->unitbytes != CD_FRAME_SIZE || h->hunkbytes % CD_FRAME_SIZE || !read_tracks(c)) {
		fprintf(stderr, "np2kai: %s is not a CD image this core reads (MODE1, MODE2, AUDIO tracks)\n", file_getname(path));
		goto fail;
	}
	c->hunkbytes = h->hunkbytes;
	c->framesperhunk = h->hunkbytes / CD_FRAME_SIZE;
	c->hunk = (UINT8 *)malloc(c->hunkbytes);
	if (!c->hunk) goto fail;
	edc_init();
	milstr_ncpy(bin, path, sizeof bin);
	milstr_ncat(bin, ".bin", sizeof bin);
	milstr_ncpy(cue, path, sizeof cue);
	milstr_ncat(cue, ".cue", sizeof cue);
	write_sheet(c, bin, sheet, sizeof sheet);
	if (!chimera_vfs_virtual(bin, c->sectors * SECTOR, chd_image_read, c)) goto fail;
	if (!chimera_vfs_text(cue, sheet)) goto fail;	/* the image stays, unused */
	milstr_ncpy(out, cue, outsize);
	return 1;

fail:
	if (c->chd) chd_close(c->chd);
	if (c->file) fclose(c->file);
	free(c->hunk);
	free(c);
	return 0;
}
