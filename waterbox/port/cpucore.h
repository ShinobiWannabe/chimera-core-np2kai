/* cpucore.h - NP2kai's CPU header for the flavour being built, as
 * NP2kai's own include path finds it (this folder comes first), with an
 * include guard, so the 286 files that cpu286-watch.h wraps can include it
 * before NP2kai's file does and change one of its limits. */
#pragma once
#if defined(CPUCORE_IA32)
#include "../../extern/NP2kai/i386c/cpucore.h"
#else
#include "../../extern/NP2kai/i286c/cpucore.h"
#endif
