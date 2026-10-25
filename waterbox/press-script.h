/* press-script.h - the input both harnesses feed the core, so a run of
 * run-native and a run of run-wbx get exactly the same.
 *
 * NP2_PRESS="frame:Button Name[:frames],..." holds each named button (a name
 * from np2-input.h) from that frame for that many frames, 8 by default: a
 * real key is down for longer than one frame.
 * NP2_MOUSE="frame:dx:dy,..." moves the mouse by dx, dy on that frame.
 */
#ifndef PRESS_SCRIPT_H
#define PRESS_SCRIPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "np2-input.h"

typedef struct { long at, len; int button; } press_t;
typedef struct { long at; int dx, dy; } move_t;

static press_t g_press[4096];
static int g_npress;
static move_t g_move[256];
static int g_nmove;

static int press_button_index(const char *name)
{
#define X(n, h, w) n,
	static const char *const names[] = { NP2_BUTTONS(X) };
#undef X
	int i;
	for (i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
		if (!strcmp(names[i], name)) return i;
	return -1;
}

/* returns 0 and says why when the script names a button that does not exist */
static int press_load(void)
{
	static char buf[65536]; char *tok, *save = NULL;
	const char *p = getenv("NP2_PRESS"), *m = getenv("NP2_MOUSE");
	if (p) {
		snprintf(buf, sizeof buf, "%s", p);
		for (tok = strtok_r(buf, ",", &save); tok && g_npress < 4096; tok = strtok_r(NULL, ",", &save)) {
			char *c1 = strchr(tok, ':'), *c2;
			if (!c1) continue;
			*c1++ = 0;
			c2 = strchr(c1, ':');
			if (c2) *c2++ = 0;
			g_press[g_npress].at = atol(tok);
			g_press[g_npress].len = c2 ? atol(c2) : 8;
			g_press[g_npress].button = press_button_index(c1);
			if (g_press[g_npress].button < 0) { fprintf(stderr, "no button called '%s'\n", c1); return 0; }
			g_npress++;
		}
	}
	if (m) {
		snprintf(buf, sizeof buf, "%s", m);
		save = NULL;
		for (tok = strtok_r(buf, ",", &save); tok && g_nmove < 256; tok = strtok_r(NULL, ",", &save)) {
			if (sscanf(tok, "%ld:%d:%d", &g_move[g_nmove].at, &g_move[g_nmove].dx, &g_move[g_nmove].dy) == 3)
				g_nmove++;
		}
	}
	return 1;
}

/* the levels for frame n, through the core's own SetButton and SetAxis */
#define PRESS_APPLY(n, SetButton, SetAxis) do { \
	int k_; \
	for (k_ = 0; k_ < g_npress; k_++) { \
		if ((n) == g_press[k_].at) SetButton(g_press[k_].button, 1); \
		if ((n) == g_press[k_].at + g_press[k_].len) SetButton(g_press[k_].button, 0); \
	} \
	for (k_ = 0; k_ < g_nmove; k_++) \
		if ((n) == g_move[k_].at) { SetAxis(NP2_AXIS_MOUSE_X, g_move[k_].dx); SetAxis(NP2_AXIS_MOUSE_Y, g_move[k_].dy); } \
} while (0)

#endif
