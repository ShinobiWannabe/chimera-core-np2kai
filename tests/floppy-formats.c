/* floppy-formats.c - every floppy layout the floppy slot takes beyond the
 * first ones, read through NP2kai as the FDC reads it and as NP2kai's own
 * INT 1Bh does (any density). tests/gen-formats.py
 * writes the images and formats.txt; each image must load by its name, and
 * every sector of every track must come back as the generator wrote it
 * (its own C, H, R and N, "NP2FMT", the extension, then its pattern; .fdd's
 * C 1 H 0 R 2 is its ID's fill byte instead). The
 * controls, one byte short or with a broken header, must not load.
 *
 * The driver's own list is checked too: a disk opened with no project goes
 * to the floppy slot by each of these extensions (bare_slot).
 *
 * Built by run-gate.sh against the native build's objects, run in the
 * folder gen-formats.py wrote. */
#include "../waterbox/np2-driver.c"
#include <diskimage/fddfile.h>

/* what run-native.c gives the driver natively */
void *alloc_invisible(size_t size) { return calloc(1, size); }

/* the sector gen-formats.py writes at C, H, R with N (not the boot sector) */
static void expected(UINT8 *buf, const char *ext, int c, int h, int r, int n)
{
	int size = 128 << n, i, k;
	k = sprintf((char *)buf + 4, "NP2FMT%s", ext) + 4;
	buf[0] = (UINT8)c; buf[1] = (UINT8)h; buf[2] = (UINT8)r; buf[3] = (UINT8)n;
	for (i = 0; i < size - k; i++)
		buf[k + i] = (UINT8)(((c * 2 + h) * 31 + r * 7 + i) & 0xff);
}

/* reads C, H, R with N from drive 0 with the given FDC density (0x40 MFM,
 * 0x00 FM, 0xff "any", as NP2kai's INT 1Bh asks); 0 when it fails */
static int read_one(int c, int h, int r, int n, int media, int rpm, int mf)
{
	fdc.us = 0;
	fdc.ctrlfd = (UINT8)media;
	fdc.rpm[0] = (UINT8)rpm;
	fdc.mf = (UINT8)mf;
	fdc.treg[0] = (UINT8)c;
	fdc.ncn = (UINT8)c;
	fdc.hd = (UINT8)h;
	fdc.C = (UINT8)c; fdc.H = (UINT8)h; fdc.R = (UINT8)r; fdc.N = (UINT8)n;
	fdc.eot = (UINT8)r;
	fdc.bufcnt = 0;
	return fdd_read() == SUCCESS && fdc.bufcnt == (UINT)(128 << n);
}

/* reads a sector as the FDC does (its own density) and as the BIOS does
 * (any density): both must work and give the same bytes, left in fdc.buf */
static int read_sector(int c, int h, int r, int n, int media, int rpm)
{
	static UINT8 bios[1024];
	if (!read_one(c, h, r, n, media, rpm, 0xff)) return 0;
	memcpy(bios, fdc.buf, 128 << n);
	return read_one(c, h, r, n, media, rpm, n ? 0x40 : 0x00) && !memcmp(bios, fdc.buf, 128 << n);
}

int main(void)
{
	static const UINT8 counter[] = { 0x8c, 0xc8, 0x8e, 0xd8 };
	char line[256], name[64], kind[16], ext[8];
	UINT8 want[1024];
	int cyls, secs, n, n0, f, files = 0, sectors = 0, refused = 0, bad = 0;
	FILE *list = fopen("formats.txt", "r");
	if (!list) { printf("no formats.txt\n"); return 1; }
	fddfile_initialize();
	while (fgets(line, sizeof line, list)) {
		f = sscanf(line, "%63s %15s %d %d %d %d", name, kind, &cyls, &secs, &n, &n0);
		strcpy(ext, strrchr(name, '.') + 1);
		if (f == 2 && !strcmp(kind, "refuse")) {
			if (fdd_set(0, name, FTYPE_NONE, 1) == SUCCESS) {
				printf("%s: loaded, but it must be refused\n", name);
				bad++;
			} else refused++;
			fdd_eject(0);
			continue;
		}
		if (f < 5) { printf("bad line: %s", line); bad++; continue; }
		if (f == 5) n0 = n;
		{
			char got[MAX_PATH];
			FILE *rn = fopen("rom.name", "wb");
			const char *slot;
			fputs(name, rn);
			fclose(rn);
			slot = bare_slot(got);
			if (!slot || strcmp(slot, "floppy")) {
				printf("%s: a disk opened alone goes to slot %s, not floppy\n", name, slot ? slot : "(none)");
				bad++;
			}
			remove("rom.name");
		}
		if (fdd_set(0, name, FTYPE_NONE, 1) != SUCCESS) {
			printf("%s: not loaded\n", name);
			bad++;
			continue;
		}
		{
			int media = !strcmp(kind, "2DD") ? DISKTYPE_2DD : DISKTYPE_2HD;
			int rpm = fddfile[0].inf.xdf.rpm, c, h, r, fails = 0;
			for (c = 0; c < cyls; c++)
				for (h = 0; h < 2; h++) {
					int tn = (c == 0 && h == 0) ? n0 : n;
					for (r = 1; r <= secs; r++) {
						const UINT8 *buf = fdc.buf;
						int ok = read_sector(c, h, r, tn, media, rpm);
						if (ok && c == 0 && h == 0 && (r - 1) * (128 << tn) < 146) {
							/* the counter boot sector (146 bytes) */
							if (r == 1)
								ok = !memcmp(buf, counter, sizeof counter);
						}
						else if (ok) {
							expected(want, ext, c, h, r, tn);
							/* .fdd's sector with no data in the file: its ID's fill byte */
							if (!strcmp(ext, "fdd") && c == 1 && h == 0 && r == 2)
								memset(want, 0xe5, 128 << tn);
							ok = !memcmp(buf, want, 128 << tn);
						}
						if (!ok && fails++ < 3)
							printf("%s: C %d H %d R %d N %d does not read back (error %02x)\n",
							       name, c, h, r, tn, fddlasterror);
						bad += !ok;
						sectors++;
					}
				}
			if (fails > 3) printf("%s: %d sectors in all do not read back\n", name, fails);
		}
		fdd_eject(0);
		files++;
	}
	fclose(list);
	printf("%d layouts load and read back %d sectors as written, %d controls refused, %d wrong\n",
	       files, sectors, refused, bad);
	return bad != 0 || files == 0 || refused != files;
}
