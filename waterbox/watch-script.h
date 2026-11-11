/* watch-script.h - the calls at memory (port/memwatch.c) as both harnesses
 * ask for them, so a run of run-native and a run of run-wbx watch exactly the
 * same, as a Lua script's event.on_bus_exec / on_bus_read / on_bus_write
 * would through Chimera's engine.
 *
 * NP2_WATCH=kind:address,...  kind x (an instruction), r (a read) or w (a
 *                     write); the address in hex, or "any" for every one;
 *                     kind:address=value gives each such read or write that
 *                     value (as a script's function giving one back)
 * NP2_WATCH_FROM=n    start watching before frame n (default 0)
 * NP2_WATCH_OFF=n     stop watching before frame n
 * NP2_WATCH_LOG=file  each call as a line "WATCH frame kind scope address
 *                     value size [r]" (r: the value could be changed);
 *                     default stderr
 * NP2_WATCH_MAX=n     at most n such lines (default 50)
 * After the run: "WATCH scopes=.. replaceable=.. x=.. r=.. w=..", the calls
 * of each kind counted.
 */
#ifndef WATCH_SCRIPT_H
#define WATCH_SCRIPT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WATCH_LIST 64
static struct { int kind; uint64_t address; int set; uint64_t value; } g_watch[WATCH_LIST];
static int g_watchCount;
static long g_watchFrame, g_watchLines, g_watchMax = 50, g_watchFrom, g_watchOff = -1;
static unsigned long long g_watchCalls[3];
static FILE *g_watchLog;

/* the engine's function: count, log, and give back a value where asked */
static uintptr_t watch_note(uintptr_t kind, uintptr_t scope, uintptr_t address, uintptr_t value,
	uintptr_t size, uintptr_t replacement)
{
	int i;
	if (kind < 3) g_watchCalls[kind]++;
	if (g_watchLines < g_watchMax) {
		fprintf(g_watchLog, "WATCH %ld %c %d %08llx %llx %d%s\n", g_watchFrame, "xrw"[kind % 3], (int)scope,
		        (unsigned long long)address, (unsigned long long)value, (int)size, replacement ? " r" : "");
		g_watchLines++;
	}
	for (i = 0; i < g_watchCount; i++)
		if (g_watch[i].set && g_watch[i].kind == (int)kind && replacement &&
		    (g_watch[i].address == UINT64_MAX || g_watch[i].address == address)) {
			*(uint64_t *)replacement = g_watch[i].value;
			return 1;
		}
	return 0;
}

/* returns 0 and says why when the variables cannot be used */
static int watch_load(void)
{
	static char buf[1024];
	char *tok, *save = NULL;
	g_watchLog = stderr;
	if (!getenv("NP2_WATCH")) return 1;
	snprintf(buf, sizeof buf, "%s", getenv("NP2_WATCH"));
	for (tok = strtok_r(buf, ",", &save); tok && g_watchCount < WATCH_LIST; tok = strtok_r(NULL, ",", &save)) {
		char *eq = strchr(tok, '=');
		const char *k = tok[0] ? strchr("xrw", tok[0]) : NULL;
		if (!k || tok[1] != ':') { fprintf(stderr, "NP2_WATCH: bad entry %s\n", tok); return 0; }
		g_watch[g_watchCount].kind = (int)(k - "xrw");
		g_watch[g_watchCount].address = strncmp(tok + 2, "any", 3) == 0 ? UINT64_MAX : strtoull(tok + 2, NULL, 16);
		if (eq) { g_watch[g_watchCount].set = 1; g_watch[g_watchCount].value = strtoull(eq + 1, NULL, 16); }
		g_watchCount++;
	}
	if (getenv("NP2_WATCH_LOG") && !(g_watchLog = fopen(getenv("NP2_WATCH_LOG"), "w"))) { perror("NP2_WATCH_LOG"); return 0; }
	if (getenv("NP2_WATCH_MAX")) g_watchMax = strtol(getenv("NP2_WATCH_MAX"), NULL, 0);
	if (getenv("NP2_WATCH_FROM")) g_watchFrom = strtol(getenv("NP2_WATCH_FROM"), NULL, 0);
	if (getenv("NP2_WATCH_OFF")) g_watchOff = strtol(getenv("NP2_WATCH_OFF"), NULL, 0);
	return 1;
}

/* before each frame n: the watches on or off, as the script asks */
#define WATCH_APPLY(n, Add, Remove) do { \
	int w_; \
	g_watchFrame = (n); \
	for (w_ = 0; w_ < g_watchCount; w_++) { \
		if ((n) == g_watchFrom && !Add(g_watch[w_].kind, 0, g_watch[w_].address)) \
			fprintf(stderr, "NP2_WATCH: the core would not watch entry %d\n", w_); \
		if ((n) == g_watchOff) Remove(g_watch[w_].kind, 0, g_watch[w_].address); \
	} \
} while (0)

#define WATCH_REPORT(scopes, replaceable) do { \
	if (g_watchCount) { \
		printf("WATCH scopes=%s replaceable=%d x=%llu r=%llu w=%llu\n", (scopes), (int)(replaceable), \
		       g_watchCalls[0], g_watchCalls[1], g_watchCalls[2]); \
		if (g_watchLog != stderr) fclose(g_watchLog); \
	} \
} while (0)

#endif
