/* cue-join.c - the driver's cue sheet joiner (np2-driver.c join_cue) on
 * sheets written the ways real ones are: quoted and bare names, a UTF-8
 * byte order mark, words in small letters, CRLF, a folder in the name with
 * Windows' separator, FLAGS and POSTGAP lines NP2kai does not read. Each
 * must give the one joined sheet the plain sheet gives, and a WAVE file or
 * a 2048-byte track must not be joined.
 *
 * Built by run-gate.sh against the native build's objects, run in a folder
 * of its own: it writes its track files there.
 */
#include "../waterbox/np2-driver.c"
#include <sys/stat.h>

/* what run-native.c gives the driver natively */
void *alloc_invisible(size_t size) { return calloc(1, size); }

static void write_file(const char *name, const char *text, long bytes)
{
	FILE *f = fopen(name, "wb");
	long i;
	if (text) fputs(text, f);
	for (i = 0; i < bytes; i++) fputc((int)(i & 0xff), f);
	fclose(f);
}

/* the joined sheet for a cue sheet, or "" when it is not joined */
static const char *joined(const char *name, const char *text)
{
	static char sheet[4096];
	char out[MAX_PATH];
	FILEH fh;
	UINT n;
	write_file(name, text, 0);
	sheet[0] = '\0';
	if (!join_cue(name, out, sizeof out)) return sheet;
	fh = file_open_rb(out);
	if (fh == FILEH_INVALID) return "(no joined sheet)";
	n = file_read(fh, sheet, sizeof sheet - 1);
	file_close(fh);
	sheet[n] = '\0';
	return sheet;
}

int main(void)
{
	static const struct { const char *name, *text; int join; } k[] = {
		{ "bom.cue", "\xef\xbb\xbf" "FILE \"t1.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
		  "FILE \"t2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:02:00\n", 1 },
		{ "bare.cue", "FILE t1.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n"
		  "FILE t2.bin BINARY\nTRACK 02 AUDIO\nINDEX 00 00:00:00\nINDEX 01 00:02:00\n", 1 },
		{ "mixed.cue", "FILE \"t1.bin\" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n"
		  "FILE t2.bin BINARY\r\n  TRACK 02 AUDIO\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n", 1 },
		{ "lower.cue", "file \"t1.bin\" binary\n  track 01 mode1/2352\n    index 01 00:00:00\n"
		  "file \"t2.bin\" binary\n  track 02 audio\n    flags dcp\n    index 00 00:00:00\n    index 01 00:02:00\n  postgap 00:02:00\n", 1 },
		{ "folder.cue", "FILE \"CD\\t1.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
		  "FILE \"CD\\t2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:02:00\n", 1 },
		{ "wave.cue", "FILE \"t1.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
		  "FILE \"t2.wav\" WAVE\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n", 0 },
		{ "cooked.cue", "FILE \"t1.bin\" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n"
		  "FILE \"t2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n", 0 },
	};
	static char plain[4096];
	int i, bad = 0;
	write_file("t1.bin", NULL, 2352L * 10);
	write_file("t2.bin", NULL, 2352L * 5);
	write_file("t2.wav", NULL, 2352L * 5);
	/* natively a name keeps its folder; in the sandbox it is the flat mount */
	mkdir("CD", 0755);
	write_file("CD/t1.bin", NULL, 2352L * 10);
	write_file("CD/t2.bin", NULL, 2352L * 5);
	snprintf(plain, sizeof plain, "%s", joined("plain.cue",
		"FILE \"t1.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
		"FILE \"t2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:02:00\n"));
	/* track 2 starts after track 1's 10 sectors: INDEX 00 at 00:00:10,
	 * INDEX 01 two seconds (150 sectors) later, 00:02:10 */
	if (!strstr(plain, "INDEX 00 00:00:10\nINDEX 01 00:02:10\n")) {
		printf("plain: wrong sheet:\n%s\n", plain);
		bad++;
	}
	for (i = 0; i < (int)(sizeof k / sizeof k[0]); i++) {
		const char *s = joined(k[i].name, k[i].text);
		char want[4096];
		/* the joined file is named after its sheet */
		snprintf(want, sizeof want, "%s", plain);
		if (k[i].join) {
			char *p = strstr(want, "plain.joined.bin"), stem[64];
			snprintf(stem, sizeof stem, "%.*s.joined.bin", (int)(strchr(k[i].name, '.') - k[i].name), k[i].name);
			if (p) {
				memmove(p + strlen(stem), p + strlen("plain.joined.bin"), strlen(p + strlen("plain.joined.bin")) + 1);
				memcpy(p, stem, strlen(stem));
			}
		}
		else want[0] = '\0';
		if (strcmp(s, want)) { printf("%s: got\n%s\nwanted\n%s\n", k[i].name, s, want); bad++; }
		else printf("%s: %s\n", k[i].name, k[i].join ? "joined as the plain sheet" : "left to NP2kai");
	}
	printf("%d sheets, %d wrong\n", (int)(sizeof k / sizeof k[0]) + 1, bad);
	return bad != 0;
}
