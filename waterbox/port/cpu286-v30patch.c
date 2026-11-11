/* cpu286-v30patch.c - i286c/v30patch.c, unchanged, built with cpu286-watch.h
 * (the calls at memory on the 286 machine). */
#if !defined(CPUCORE_IA32) && defined(CHIMERA_MEMWATCH)
#include "cpu286-watch.h"
#include "../../extern/NP2kai/i286c/v30patch.c"
#endif
