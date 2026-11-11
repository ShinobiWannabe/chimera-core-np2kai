/* memwatch.c - the calls at memory (Chimera issues 496 and 534).
 *
 * A Lua script's event.on_bus_exec, on_bus_read and on_bus_write stop the
 * machine part way through a frame at an address of the System Bus (the
 * physical address, numbered as the core's "System Bus" memory domain, so
 * an address from RAM Search works as it is). Chimera hands the core a
 * function (SetMemoryCallback, callback slot 4) and names the places to
 * watch (AddMemoryWatch, RemoveMemoryWatch); the core calls the function
 * as fn(kind, scope, address, value, size, replacement) at each watched
 * access: kind 0 an instruction about to run, 1 a read once its value is
 * fetched, 2 a write before memory changes. replacement points at the
 * value, which the script may change, for reads and writes.
 *
 * Where the calls come from: trap/steptrap.h (instructions, both CPUs),
 * cpumem-watch.c (the CPU's memory functions, both CPUs) and
 * paging-watch.c (the IA-32 TLB's direct pointers).
 *
 * Everything here is ECL_INVISIBLE: the function is the same slot address
 * in every session, and what is watched belongs to the scripts, not to the
 * machine, so no savestate carries it and a load leaves it as it is. No
 * emulated clock is spent, so a run with a script watching is the same run,
 * frame for frame, as one without.
 *
 * Only in the watch flavours (np21w, np2w), which the setting
 * luaMemoryEvents picks (np2-entry.c); the plain ones have none of it. */
#if defined(CHIMERA_MEMWATCH)
#include <compiler.h>
#include <cpucore.h>
#include <string.h>
#include <emulibc.h>

#include "memwatch.h"

#define WATCH_MAX 1024
#define PAGE_BITS (1u << 20)	/* 4 KiB pages of the 32-bit bus */

ECL_INVISIBLE int chimera_watch_read_on;
ECL_INVISIBLE int chimera_watch_write_on;
ECL_INVISIBLE int chimera_watch_exec_on;
ECL_INVISIBLE int chimera_watch_depth;

ECL_INVISIBLE static uint64_t g_fn;
/* per kind (0 exec, 1 read, 2 write): every address, or a list of up to
 * WATCH_MAX addresses with a bitmap of the pages they are in, so an access
 * outside every watched page is turned away with one bit test */
ECL_INVISIBLE static int g_all[3];
ECL_INVISIBLE static int g_count[3];
ECL_INVISIBLE static uint32_t g_addr[3][WATCH_MAX];
ECL_INVISIBLE static uint8_t g_pages[3][PAGE_BITS / 8];

static int page_marked(int kind, uint32_t address)
{
	uint32_t p = address >> 12;
	return (g_pages[kind][p >> 3] >> (p & 7)) & 1;
}

int chimera_watch_byte(int kind, uint32_t address)
{
	int i;
	if (g_all[kind]) return 1;
	if (!page_marked(kind, address)) return 0;
	for (i = 0; i < g_count[kind]; i++)
		if (g_addr[kind][i] == address) return 1;
	return 0;
}

int chimera_watch_rw_page(uint32_t address)
{
	return g_all[1] || g_all[2] || page_marked(1, address) || page_marked(2, address);
}

static void refresh(int kind)
{
	int i;
	memset(g_pages[kind], 0, sizeof g_pages[kind]);
	for (i = 0; i < g_count[kind]; i++) {
		uint32_t p = g_addr[kind][i] >> 12;
		g_pages[kind][p >> 3] |= (uint8_t)(1 << (p & 7));
	}
	chimera_watch_read_on = g_fn != 0 && (g_all[1] || g_count[1] > 0);
	chimera_watch_write_on = g_fn != 0 && (g_all[2] || g_count[2] > 0);
	chimera_watch_exec_on = g_fn != 0 && (g_all[0] || g_count[0] > 0);
}

typedef uintptr_t (*memory_fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

static uint32_t call(int kind, uint32_t address, uint32_t value, int size)
{
	uint64_t now = value;
	chimera_watch_depth++;
	((memory_fn)(uintptr_t)g_fn)((uintptr_t)kind, 0, address, value, (uintptr_t)size,
		kind == 0 ? 0 : (uintptr_t)&now);
	chimera_watch_depth--;
	switch (size) {
	case 1: return (uint8_t)now;
	case 2: return (uint16_t)now;
	default: return (uint32_t)now;
	}
}

/* The address as the bus sees it: NP2kai's memory functions take the
 * address with the A20 gate not yet applied, and apply CPU_ADRSMASK (at
 * least past the low RAM, where the mask changes nothing) */
uint32_t chimera_watch_read(uint32_t address, uint32_t value, int size)
{
	address &= CPU_ADRSMASK;
	if (chimera_watch_depth || !chimera_watch_byte(1, address)) return value;
	return call(1, address, value, size);
}

uint32_t chimera_watch_write(uint32_t address, uint32_t value, int size)
{
	address &= CPU_ADRSMASK;
	if (chimera_watch_depth || !chimera_watch_byte(2, address)) return value;
	return call(2, address, value, size);
}

#if defined(CPUCORE_IA32)
UINT32 MEMCALL np2orig_memp_read32(UINT32 address);

/* The physical address of a linear one, read from the page tables without
 * touching them: no accessed bit set, no fault raised, no TLB entry made, so
 * looking does not change the machine. 0 when the page is not present (the
 * instruction will fault before it runs; nothing to report). */
static int linear_to_physical(uint32_t laddr, uint32_t *paddr)
{
	UINT32 pde, pte;
	chimera_watch_depth++;
	pde = np2orig_memp_read32((CPU_CR3 & 0xfffff000u) + ((laddr >> 22) << 2));
	if (!(pde & 1)) { chimera_watch_depth--; return 0; }
	if ((CPU_CR4 & (1 << 4)) && (pde & (1 << 7))) {	/* CR4.PSE, a 4 MiB page */
		*paddr = (pde & 0xffc00000u) | (laddr & 0x003fffffu);
		chimera_watch_depth--;
		return 1;
	}
	pte = np2orig_memp_read32((pde & 0xfffff000u) + (((laddr >> 12) & 0x3ff) << 2));
	chimera_watch_depth--;
	if (!(pte & 1)) return 0;
	*paddr = (pte & 0xfffff000u) | (laddr & 0xfffu);
	return 1;
}
#endif

void chimera_watch_exec(void)
{
	uint32_t address;
	if (chimera_watch_depth) return;
#if defined(CPUCORE_IA32)
	address = CS_BASE + CPU_EIP;
	if (CPU_STAT_PAGING && !linear_to_physical(address, &address)) return;
#else
	address = CS_BASE + CPU_IP;
#endif
	address &= CPU_ADRSMASK;
	if (chimera_watch_byte(0, address)) call(0, address, 0, 0);
}

/* ------------------------------------------------------------ exports */

const char *chimera_watch_scopes(void) { return "System Bus"; }

void chimera_watch_set_callback(uint64_t fn)
{
	g_fn = fn;
	refresh(0);
	refresh(1);
	refresh(2);
}

int32_t chimera_watch_add(int32_t kind, int32_t scope, uint64_t address)
{
	if (kind < 0 || kind > 2 || scope != 0) return 0;
	if (address == UINT64_MAX)
		g_all[kind] = 1;
	else if (address > 0xffffffffu)
		return 0;
	else {
		int i;
		for (i = 0; i < g_count[kind]; i++)
			if (g_addr[kind][i] == (uint32_t)address) break;
		if (i == g_count[kind]) {
			if (g_count[kind] >= WATCH_MAX) return 0;
			g_addr[kind][g_count[kind]++] = (uint32_t)address;
		}
	}
	refresh(kind);
	if (kind != 0) chimera_watch_tlb_refresh();
	return 1;
}

void chimera_watch_remove(int32_t kind, int32_t scope, uint64_t address)
{
	int i;
	if (kind < 0 || kind > 2 || scope != 0) return;
	if (address == UINT64_MAX)
		g_all[kind] = 0;
	else
		for (i = 0; i < g_count[kind]; i++)
			if (g_addr[kind][i] == (uint32_t)address) {
				g_addr[kind][i] = g_addr[kind][--g_count[kind]];
				break;
			}
	/* a page no longer watched keeps the slow path until its TLB entry
	 * is replaced: slower for a while, never wrong */
	refresh(kind);
}

#if !defined(CPUCORE_IA32)
/* the 286 machine has no TLB and no direct pointers */
void chimera_watch_tlb_refresh(void) { }
#endif
#endif /* CHIMERA_MEMWATCH */
