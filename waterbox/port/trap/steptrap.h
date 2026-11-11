/* trap/steptrap.h - takes the place of NP2kai's trap/steptrap.h (this
 * folder is ahead of NP2kai's on the include path), for the calls at memory
 * (Chimera issue 496; docs/memory-events.md).
 *
 * The core is built with ENABLE_TRAP, so both CPU loops (i386c/ia32/cpu.c,
 * i286c/i286c.c, i286c/v30patch.c) call steptrap(cs, eip) before every
 * instruction. NP2kai's own trap/steptrap.c (a debugging log written to the
 * host) is not built; here the call is one test of a flag, true only while
 * a Lua script watches instructions, and memwatch.c does the rest. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern int chimera_watch_exec_on;
void chimera_watch_exec(void);

#ifdef __cplusplus
}
#endif

#define steptrap(cs, eip) do { if (chimera_watch_exec_on) chimera_watch_exec(); } while (0)
