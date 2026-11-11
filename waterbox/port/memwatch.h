/* memwatch.h - the calls at memory (Chimera issues 496 and 534): what is
 * watched, and the call into the engine at each watched access. See
 * memwatch.c, and docs/memory-events.md for what is and is not covered. */
#ifndef CHIMERA_MEMWATCH_H
#define CHIMERA_MEMWATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* true while something watches that kind of access: the one test the
 * memory functions make when nothing is watched */
extern int chimera_watch_read_on;
extern int chimera_watch_write_on;
extern int chimera_watch_exec_on;
/* above zero while the core is inside a watched access (the access itself,
 * or the engine's call): nested accesses are not reported again */
extern int chimera_watch_depth;

/* a read of size bytes at a physical address gave value: the value the
 * machine goes on with (the script may have changed it) */
uint32_t chimera_watch_read(uint32_t address, uint32_t value, int size);
/* a write of size bytes at a physical address is about to store value: the
 * value to store */
uint32_t chimera_watch_write(uint32_t address, uint32_t value, int size);
/* the CPU is about to run an instruction (trap/steptrap.h) */
void chimera_watch_exec(void);
/* is any byte of the 4 KiB page at this physical address watched for reads
 * or writes? Such a page gets no direct host pointer (cpumem-watch.c), so
 * its accesses come through the memory functions. */
int chimera_watch_rw_page(uint32_t address);
/* is this byte watched for reads (kind 1) or writes (kind 2)? */
int chimera_watch_byte(int kind, uint32_t address);

/* the driver's exports (np2-driver.c) */
const char *chimera_watch_scopes(void);
void chimera_watch_set_callback(uint64_t fn);
int32_t chimera_watch_add(int32_t kind, int32_t scope, uint64_t address);
void chimera_watch_remove(int32_t kind, int32_t scope, uint64_t address);

/* paging-watch.c (IA-32 only): drop the direct host pointers the TLB and
 * the code-fetch cache already hold for pages now watched */
void chimera_watch_tlb_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
