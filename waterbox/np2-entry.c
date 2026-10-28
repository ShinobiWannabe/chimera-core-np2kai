/* np2-entry.c - the two-machine core's exports (docs/PLAN.md section 10).
 *
 * core.wbx holds two halves: the IA-32 emulator with its driver (np21_*)
 * and the 286 one (np2_*), each with everything but its driver's entry
 * points made local, so the two copies of NP2kai cannot meet. Init reads
 * the "machine" setting once and every later call goes to that half; the
 * other half is never run, and its memory never changes after the seal.
 */
#include <stdint.h>
#include <string.h>

#include <emulibc.h>
#include <waterbox_settings.h>

#include "np2-exports.h"

#define X(ret, name, params, args) ret np21_##name params; ret np2_##name params;
NP2_EXPORTS(X)
#undef X
#define X(name, params, args) void np21_##name params; void np2_##name params;
NP2_EXPORTS_BOTH(X)
#undef X
int np21_Init(void);
int np2_Init(void);

/* which half: 0 the IA-32 machine (PC9821), 1 the 286 machine (PC98).
 * Chosen before the seal and never changed, so states carry it. */
static int g_half;

ECL_EXPORT int Init(void)
{
	char machine[16];
	if (wbx_setting_str("machine", machine, sizeof machine) < 0)
		machine[0] = '\0';
	g_half = strcmp(machine, "PC98") == 0;
	return g_half ? np2_Init() : np21_Init();
}

#define X(ret, name, params, args) \
	ECL_EXPORT ret name params { return g_half ? np2_##name args : np21_##name args; }
NP2_EXPORTS(X)
#undef X

/* What Chimera hands the core before Init, when the half is not chosen yet
 * (its engine sets the input-poll callback first, session.cpp): both halves
 * take it, so the half Init picks has it. */
#define X(name, params, args) \
	ECL_EXPORT void name params { np21_##name args; np2_##name args; }
NP2_EXPORTS_BOTH(X)
#undef X
