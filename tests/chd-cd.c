/* chd-cd.c - CD images in MAME's CHD format (waterbox/port/chd-cd.c) read
 * back through NP2kai as the images they were made from.
 *
 * run-gate.sh writes tests/gen-chd-src.py's CDs and has chdman make CHDs
 * of them in this test's folder:
 *   mixed-lz.chd    mixed.cue, chdman's default codecs (CD LZMA, Deflate,
 *                   FLAC: the data sectors' ECC dropped and remade)
 *   mixed-zs.chd    mixed.cue, CD Zstandard and FLAC
 *   mixed-none.chd  mixed.cue, not compressed
 *   iso.chd         data.iso, its 2048-byte sectors kept cooked
 *   raw.chd         a CHD that is not a CD (chdman createraw)
 *   cut.chd         mixed-lz.chd cut short
 * Each mixed CHD must give back mixed.bin byte for byte, and NP2kai must
 * read its sheet as it reads mixed.cue: the same track table, the same
 * total and the same 2048 bytes in every sector. iso.chd must read as
 * data.iso does, every sector made whole with a header that names it and
 * an EDC and ECC that check. raw.chd and cut.chd must be refused.
 *
 * Built by run-gate.sh against the native build's objects.
 */
#include "../waterbox/np2-driver.c"
#include "diskimage/cd/cdd_cue.h"
#include "diskimage/cd/cdd_iso.h"
#include "diskimage/cddfile.h"

/* what run-native.c gives the driver natively */
void *alloc_invisible(size_t size) { return calloc(1, size); }

/* libchdr's (cdrom.h) */
int ecc_verify(const uint8_t *sector);

static int bad;
static long s_sectors;	/* data sectors compared */

static void fail(const char *what, const char *name)
{
	printf("%s: %s\n", name, what);
	bad++;
}

/* the whole of a file through the file layer; its size in *size */
static UINT8 *slurp(const char *name, UINT64 *size)
{
	FILEH fh = file_open_rb(name);
	UINT8 *b;
	if (fh == FILEH_INVALID) return NULL;
	*size = file_getsize(fh);
	b = (UINT8 *)malloc((size_t)*size + 1);
	if (b) file_read(fh, b, (UINT)*size);
	file_close(fh);
	return b;
}

static UINT32 edc(const UINT8 *s, int n)
{
	UINT32 e = 0;
	int i, j;
	for (i = 0; i < n; i++) {
		e ^= s[i];
		for (j = 0; j < 8; j++) e = (e >> 1) ^ ((e & 1) ? 0xd8018001u : 0);
	}
	return e;
}

/* two opened images: the same tracks, total and 2048-byte sectors */
static void same_disc(_SXSIDEV *a, _SXSIDEV *b, const char *name)
{
	CDINFO ia = (CDINFO)a->hdl, ib = (CDINFO)b->hdl;
	UINT8 sa[2048], sb[2048];
	FILEPOS p;
	UINT t;
	if (a->totals != b->totals) { fail("not the same number of sectors", name); return; }
	if (ia->trks != ib->trks) { fail("not the same number of tracks", name); return; }
	for (t = 0; t <= ia->trks; t++) {
		_CDTRK x = ia->trk[t], y = ib->trk[t];
		/* where the track is in its file, and its sector size there (the
		 * ISO's 2048), are the file's business */
		if (x.adr_ctl != y.adr_ctl || x.point != y.point || x.pos != y.pos || x.pos0 != y.pos0 ||
		    x.pregap_sectors != y.pregap_sectors) {
			printf("%s: track %u differs (pos %u/%u pos0 %u/%u type %02x/%02x pregap %u/%u)\n", name, t,
				(unsigned)x.pos, (unsigned)y.pos, (unsigned)x.pos0, (unsigned)y.pos0, x.adr_ctl, y.adr_ctl,
				(unsigned)x.pregap_sectors, (unsigned)y.pregap_sectors);
			bad++;
			return;
		}
	}
	/* every sector of a data track, from where it starts on the disc to
	 * where the next track (or the lead-out) starts */
	for (t = 0; t < ia->trks; t++) {
		if (ia->trk[t].adr_ctl != TRACKTYPE_DATA) continue;
		for (p = ia->trk[t].pos; p < (FILEPOS)ia->trk[t + 1].pos0; p++) {
			REG8 ra = a->read(a, p, sa, 2048), rb = b->read(b, p, sb, 2048);
			s_sectors++;
			if (ra != rb || memcmp(sa, sb, 2048)) {
				printf("%s: sector %ld reads differently\n", name, (long)p);
				bad++;
				return;
			}
		}
	}
}

static void open_cue(_SXSIDEV *s, const char *cue, const char *name)
{
	memset(s, 0, sizeof *s);
	if (opencue(s, cue) != SUCCESS) fail("NP2kai does not open the sheet", name);
}

int main(void)
{
	static const char *const mixed[] = { "mixed-lz.chd", "mixed-zs.chd", "mixed-none.chd" };
	_SXSIDEV ref, got;
	char cue[MAX_PATH], bin[MAX_PATH];
	UINT8 *want, *have, *iso;
	UINT64 wn, hn, in;
	int i, checked = 0;

	want = slurp("mixed.bin", &wn);
	if (!want) { printf("no mixed.bin\n"); return 1; }
	open_cue(&ref, "mixed.cue", "mixed.cue");
	for (i = 0; i < 3; i++) {
		if (!chimera_chd_cue(mixed[i], cue, sizeof cue)) { fail("not opened", mixed[i]); continue; }
		snprintf(bin, sizeof bin, "%s.bin", mixed[i]);
		have = slurp(bin, &hn);
		if (!have || hn != wn || memcmp(have, want, (size_t)wn)) fail("its image is not mixed.bin", mixed[i]);
		free(have);
		open_cue(&got, cue, mixed[i]);
		if (ref.hdl && got.hdl) same_disc(&ref, &got, mixed[i]);
		checked++;
	}

	/* cooked sectors: each made whole, and read as the ISO reads */
	iso = slurp("data.iso", &in);
	if (!iso) { printf("no data.iso\n"); return 1; }
	if (!chimera_chd_cue("iso.chd", cue, sizeof cue)) fail("not opened", "iso.chd");
	else {
		have = slurp("iso.chd.bin", &hn);
		if (!have || hn != in / 2048 * 2352) fail("its image is not 2352 bytes a sector", "iso.chd");
		else {
			UINT64 n;
			for (n = 0; n < in / 2048; n++) {
				const UINT8 *s = have + n * 2352;
				UINT32 a = (UINT32)n + 150;
				UINT8 head[16] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
					(UINT8)((a / 4500 / 10) << 4 | a / 4500 % 10), (UINT8)((a / 75 % 60 / 10) << 4 | a / 75 % 60 % 10),
					(UINT8)((a % 75 / 10) << 4 | a % 75 % 10), 1 };
				UINT32 e = edc(s, 2064);
				if (memcmp(s, head, 16) || memcmp(s + 16, iso + n * 2048, 2048) ||
				    s[2064] != (UINT8)e || s[2065] != (UINT8)(e >> 8) || s[2066] != (UINT8)(e >> 16) || s[2067] != (UINT8)(e >> 24) ||
				    !ecc_verify(s)) {
					printf("iso.chd: sector %lu is not whole\n", (unsigned long)n);
					bad++;
					break;
				}
			}
		}
		free(have);
		memset(&ref, 0, sizeof ref);
		if (openiso(&ref, "data.iso") != SUCCESS) fail("NP2kai does not open it", "data.iso");
		open_cue(&got, cue, "iso.chd");
		if (ref.hdl && got.hdl) same_disc(&ref, &got, "iso.chd");
		checked++;
	}

	/* the controls */
	if (chimera_chd_cue("raw.chd", cue, sizeof cue)) fail("opened, and it is not a CD", "raw.chd");
	if (chimera_chd_cue("cut.chd", cue, sizeof cue)) fail("opened, and it is cut short", "cut.chd");
	if (chimera_chd_cue("mixed.cue", cue, sizeof cue)) fail("taken for a CHD", "mixed.cue");

	if (bad) { printf("%d problems\n", bad); return 1; }
	printf("%d CHDs read as the images they were made from (LZMA, Zstandard, none; MODE1, MODE1 cooked, audio, both pregaps; %ld data sectors compared); a CHD that is not a CD and a cut one refused\n", checked, s_sectors);
	return 0;
}
