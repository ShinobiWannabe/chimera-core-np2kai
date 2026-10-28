/* firmware-names.c - every firmware file is found whatever the case of the
 * name asked for (port/dosio.c chimera_firmware_name): each id of
 * waterbox/np2-firmware.h asked in small letters, in capitals and in mixed
 * case, bare and after a folder, must give that id; a name that is no
 * firmware must give none. Built by run-gate.sh against the native build's
 * dosio.o. Prints one line a failure and exits 1 if there was any. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "../waterbox/np2-firmware.h"

const char *chimera_firmware_name(const char *path);

static int bad, checked;

static void expect(const char *ask, const char *want)
{
	const char *got = chimera_firmware_name(ask);
	checked++;
	if (want ? !got || strcmp(got, want) : got != NULL) {
		printf("%s -> %s, not %s\n", ask, got ? got : "(none)", want ? want : "(none)");
		bad++;
	}
}

static void each(const char *id)
{
	char v[3][64], path[80];
	size_t i, n = strlen(id);
	int k;
	for (i = 0; i <= n; i++) {
		v[0][i] = (char)tolower((unsigned char)id[i]);
		v[1][i] = (char)toupper((unsigned char)id[i]);
		v[2][i] = (char)(i % 2 ? toupper((unsigned char)id[i]) : tolower((unsigned char)id[i]));
	}
	for (k = 0; k < 3; k++) {
		expect(v[k], id);
		snprintf(path, sizeof path, "firmware/%s", v[k]);
		expect(path, id);
	}
}

int main(void)
{
#define X(name) each(name);
	NP2_FIRMWARE_FILES(X)
#undef X
	expect("game.HDM", NULL);
	expect("font.rom.bak", NULL);
	expect("slots", NULL);
	printf("%d names checked, %d wrong\n", checked, bad);
	return bad != 0;
}
