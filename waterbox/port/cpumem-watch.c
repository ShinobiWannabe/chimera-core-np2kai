/* cpumem-watch.c - the CPU's memory functions, with the calls at memory
 * (Chimera issues 496 and 534; memwatch.c).
 *
 * Everything the CPU reads or writes as data goes through NP2kai's
 * memp_read8/16/32 and memp_write8/16/32 (on the IA-32 machine also their
 * _fast twins, which the CPU calls first, and memp_reads/memp_writes for
 * blocks). The upstream file (i386c/cpumem.c or i286c/cpumem.c) is compiled
 * here, unchanged, with those functions renamed np2orig_*, and the real
 * names wrap them: when nothing watches, one flag test and the upstream
 * function; when something does, a read reports the value it fetched and
 * goes on with the value the script left, and a write reports the value
 * before storing what the script left. The upstream file's calls among its
 * own functions use the np2orig_ names, so an access NP2kai splits into
 * smaller ones (a word across a 32 KiB block, say) is reported once, at its
 * own size.
 *
 * Not reported: instruction fetches on the IA-32 machine (the _codefetch
 * functions) and the CPU's own page-table reads and writes (the _paging
 * functions). On the 286 machine instruction bytes are fetched through
 * memp_read8 too, so they are reported as reads.
 *
 * On the IA-32 machine with paging on, the TLB keeps direct host pointers
 * to ordinary RAM (memp_get_direct_host_page); a page watched for reads or
 * writes gets none, so its accesses come here (paging-watch.c drops the
 * ones made before the watch).
 *
 * Only in the watch flavours (np21w, np2w; np2-flags.sh): the plain ones
 * build NP2kai's file itself. */
#if defined(CHIMERA_MEMWATCH)
#if defined(CPUCORE_IA32)
#define memp_read8 np2orig_memp_read8
#define memp_read16 np2orig_memp_read16
#define memp_read32 np2orig_memp_read32
#define memp_read8_fast np2orig_memp_read8_fast
#define memp_read16_fast np2orig_memp_read16_fast
#define memp_read32_fast np2orig_memp_read32_fast
#define memp_write8 np2orig_memp_write8
#define memp_write16 np2orig_memp_write16
#define memp_write32 np2orig_memp_write32
#define memp_write8_fast np2orig_memp_write8_fast
#define memp_write16_fast np2orig_memp_write16_fast
#define memp_write32_fast np2orig_memp_write32_fast
#define memp_reads np2orig_memp_reads
#define memp_writes np2orig_memp_writes
#define memp_get_direct_host_page np2orig_memp_get_direct_host_page
#include "../../extern/NP2kai/i386c/cpumem.c"
#undef memp_read8_fast
#undef memp_read16_fast
#undef memp_read32_fast
#undef memp_write8_fast
#undef memp_write16_fast
#undef memp_write32_fast
#undef memp_get_direct_host_page
#else
#define memp_read8 np2orig_memp_read8
#define memp_read16 np2orig_memp_read16
#define memp_read32 np2orig_memp_read32
#define memp_write8 np2orig_memp_write8
#define memp_write16 np2orig_memp_write16
#define memp_write32 np2orig_memp_write32
#define memp_reads np2orig_memp_reads
#define memp_writes np2orig_memp_writes
#include "../../extern/NP2kai/i286c/cpumem.c"
#endif
#undef memp_read8
#undef memp_read16
#undef memp_read32
#undef memp_write8
#undef memp_write16
#undef memp_write32
#undef memp_reads
#undef memp_writes

#include "memwatch.h"

#define WATCH_READ(type, name, size) \
type MEMCALL name(UINT32 address) \
{ \
	type value; \
	if (!chimera_watch_read_on) return np2orig_##name(address); \
	chimera_watch_depth++; \
	value = np2orig_##name(address); \
	chimera_watch_depth--; \
	return (type)chimera_watch_read(address, value, size); \
}
#define WATCH_WRITE(type, name, size) \
void MEMCALL name(UINT32 address, type value) \
{ \
	if (chimera_watch_write_on) { \
		value = (type)chimera_watch_write(address, value, size); \
		chimera_watch_depth++; \
		np2orig_##name(address, value); \
		chimera_watch_depth--; \
		return; \
	} \
	np2orig_##name(address, value); \
}

WATCH_READ(REG8, memp_read8, 1)
WATCH_READ(REG16, memp_read16, 2)
WATCH_READ(UINT32, memp_read32, 4)
WATCH_WRITE(REG8, memp_write8, 1)
WATCH_WRITE(REG16, memp_write16, 2)
WATCH_WRITE(UINT32, memp_write32, 4)
#if defined(CPUCORE_IA32)
WATCH_READ(REG8, memp_read8_fast, 1)
WATCH_READ(REG16, memp_read16_fast, 2)
WATCH_READ(UINT32, memp_read32_fast, 4)
WATCH_WRITE(REG8, memp_write8_fast, 1)
WATCH_WRITE(REG16, memp_write16_fast, 2)
WATCH_WRITE(UINT32, memp_write32_fast, 4)

UINT8 * MEMCALL memp_get_direct_host_page(UINT32 address)
{
	if ((chimera_watch_read_on || chimera_watch_write_on) && chimera_watch_rw_page(address & CPU_ADRSMASK))
		return NULL;
	return np2orig_memp_get_direct_host_page(address);
}
#endif

/* blocks (descriptor tables, the FPU's state, some string moves): each
 * watched byte is reported on its own, size 1 */
void MEMCALL memp_reads(UINT32 address, void *dat, UINT leng)
{
	UINT i;
	if (!chimera_watch_read_on) { np2orig_memp_reads(address, dat, leng); return; }
	chimera_watch_depth++;
	np2orig_memp_reads(address, dat, leng);
	chimera_watch_depth--;
	for (i = 0; i < leng; i++)
		((UINT8 *)dat)[i] = (UINT8)chimera_watch_read(address + i, ((UINT8 *)dat)[i], 1);
}

void MEMCALL memp_writes(UINT32 address, const void *dat, UINT leng)
{
	UINT8 chunk[256];
	UINT done, n, i;
	if (!chimera_watch_write_on) { np2orig_memp_writes(address, dat, leng); return; }
	for (done = 0; done < leng; done += n) {
		n = leng - done < sizeof chunk ? leng - done : (UINT)sizeof chunk;
		memcpy(chunk, (const UINT8 *)dat + done, n);
		for (i = 0; i < n; i++)
			chunk[i] = (UINT8)chimera_watch_write(address + done + i, chunk[i], 1);
		chimera_watch_depth++;
		np2orig_memp_writes(address + done, chunk, n);
		chimera_watch_depth--;
	}
}
#endif /* CHIMERA_MEMWATCH */
