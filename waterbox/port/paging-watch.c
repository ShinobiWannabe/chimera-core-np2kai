/* paging-watch.c - NP2kai's IA-32 paging (i386c/ia32/paging.c), compiled
 * here unchanged, with one function added for the calls at memory
 * (memwatch.c, cpumem-watch.c).
 *
 * With paging on, a TLB entry for ordinary RAM keeps a direct host pointer,
 * and reads and writes through it never reach the memory functions. Once a
 * page is watched, memp_get_direct_host_page gives no pointer for it; this
 * drops the pointers entries already hold, and the code-fetch cache's. The
 * entries stay valid, with the same translation: only the way to the bytes
 * changes, so the machine runs on as it would have. */
#if defined(CPUCORE_IA32) && defined(CHIMERA_MEMWATCH)
#include "../../extern/NP2kai/i386c/ia32/paging.c"
#include "memwatch.h"

void chimera_watch_tlb_refresh(void)
{
#if IA32_MEMORY_FAST_PATH && !defined(SUPPORT_IA32_HAXM)
	int n, i;
	for (n = 0; n < NTLB; n++)
		for (i = 0; i < NENTRY; i++) {
			struct tlb_entry *ep = &tlb[n].entry[i];
			if (TLB_IS_VALID(ep) && ep->host_page != NULL && chimera_watch_rw_page(ep->paddr)) {
				ep->host_page = NULL;
				ep->fast_flags &= ~(TLBF_DIRECT_READ | TLBF_DIRECT_WRITE);
			}
		}
	codefetch_cache_invalidate();
#endif
}
#endif
