/* np2-entry.c - the two-machine core's exports (docs/PLAN.md section 10).
 *
 * core.wbx holds four halves: the IA-32 emulator with its driver (np21_*)
 * and the 286 one (np2_*), and each of them again with the calls at memory
 * (np21w_*, np2w_*; Chimera issues 496 and 534, port/memwatch.c). Each has
 * everything but its driver's entry points made local, so the copies of
 * NP2kai cannot meet. Init reads the "machine" and "luaMemoryEvents"
 * settings once and every later call goes to that half; the others are
 * never run, and their memory never changes after the seal.
 *
 * The plain halves are NP2kai's CPU as it is: a Lua script's
 * event.on_bus_exec and the rest are refused there (Chimera asks for the
 * memory scopes before Init, and is told there are none). The watch halves
 * test a flag at every instruction and every memory access, a few per cent
 * of the speed, and run the same machine frame for frame.
 */
#include <stdint.h>
#include <string.h>

#include <emulibc.h>
#include <waterbox_settings.h>

#include "np2-exports.h"

#define HALVES(X, ...) X(np21, __VA_ARGS__) X(np2, __VA_ARGS__) X(np21w, __VA_ARGS__) X(np2w, __VA_ARGS__)
#define DECL(h, ret, name, params) ret h##_##name params;
#define X(ret, name, params, args) HALVES(DECL, ret, name, params)
NP2_EXPORTS(X)
#undef X
#define X(name, params, args) HALVES(DECL, void, name, params)
NP2_EXPORTS_BOTH(X)
#undef X
#define X(ret, name, params, args) DECL(np21w, ret, name, params) DECL(np2w, ret, name, params)
NP2_EXPORTS_WATCH(X)
#undef X
int np21_Init(void);
int np2_Init(void);
int np21w_Init(void);
int np2w_Init(void);

/* which half: 0 the IA-32 machine (PC9821), 1 the 286 machine (PC98), and
 * 2 and 3 the same with the calls at memory. Chosen before the seal and
 * never changed, so states carry it. */
static int g_half;

static int memory_events(void) { return wbx_setting_bool("luaMemoryEvents", 0); }

ECL_EXPORT int Init(void)
{
	char machine[16];
	if (wbx_setting_str("machine", machine, sizeof machine) < 0)
		machine[0] = '\0';
	g_half = (strcmp(machine, "PC98") == 0) + (memory_events() ? 2 : 0);
	switch (g_half) {
	case 1: return np2_Init();
	case 2: return np21w_Init();
	case 3: return np2w_Init();
	default: return np21_Init();
	}
}

#define X(ret, name, params, args) \
	ECL_EXPORT ret name params { \
		switch (g_half) { \
		case 1: return np2_##name args; \
		case 2: return np21w_##name args; \
		case 3: return np2w_##name args; \
		default: return np21_##name args; \
		} \
	}
NP2_EXPORTS(X)
#undef X

/* What Chimera hands the core before Init, when the half is not chosen yet
 * (its engine sets the input-poll callback first, session.cpp): every half
 * takes it, so the half Init picks has it. */
#define X(name, params, args) \
	ECL_EXPORT void name params { np21_##name args; np2_##name args; np21w_##name args; np2w_##name args; }
NP2_EXPORTS_BOTH(X)
#undef X

/* ---- the calls at memory. Chimera asks for the scopes and hands over its
 * function before Init (session.cpp), so these read the setting
 * themselves; watches come after Init, to the half it chose. With the
 * setting off there are no scopes, and Chimera refuses the events with its
 * own words ("This core cannot call a Lua function at ..."). */
ECL_EXPORT const char *GetMemoryCallbackScopes(void)
{
	return memory_events() ? np21w_GetMemoryCallbackScopes() : "";
}

ECL_EXPORT int32_t GetMemoryCallbackReplaceable(void)
{
	return memory_events() ? np21w_GetMemoryCallbackReplaceable() : 0;
}

ECL_EXPORT void SetMemoryCallback(uint64_t fn)
{
	np21w_SetMemoryCallback(fn);
	np2w_SetMemoryCallback(fn);
}

ECL_EXPORT int32_t AddMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	switch (g_half) {
	case 2: return np21w_AddMemoryWatch(kind, scope, address);
	case 3: return np2w_AddMemoryWatch(kind, scope, address);
	default: return 0;
	}
}

ECL_EXPORT void RemoveMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	switch (g_half) {
	case 2: np21w_RemoveMemoryWatch(kind, scope, address); break;
	case 3: np2w_RemoveMemoryWatch(kind, scope, address); break;
	default: break;
	}
}
