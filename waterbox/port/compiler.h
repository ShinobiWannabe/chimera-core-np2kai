/* compiler.h - NP2kai's per-platform header, for the Chimera core.
 *
 * NP2kai's sources include "compiler.h" and expect each platform to provide
 * one. This is ours, used by both builds (the native reference and the
 * sandboxed core). It takes the libretro branches of upstream's shared
 * headers (no SDL, no window system) and supplies the pieces the libretro
 * port got from libretro-common.
 */
#ifndef COMPILER_H
#define COMPILER_H

#include "compiler_base.h"

/* Upstream's build passes these from git; pccore.c puts them in np2version. */
#ifndef NP2KAI_GIT_TAG
#define NP2KAI_GIT_TAG "chimera"
#endif
#ifndef NP2KAI_GIT_HASH
#define NP2KAI_GIT_HASH "5939e0c6d598"
#endif

#define msgbox(title, msg)
#define __ASSERT(s)
#define RESOURCE_US
#define NP2_SIZE_VGA

/* The millisecond counter NP2kai reads for disk motor timing, key repeat and
 * the SASI BIOS. It is emulated time, never the host's (docs/PLAN.md
 * section 3, item 1). */
#ifdef __cplusplus
extern "C" {
#endif
unsigned int chimera_gettick(void);
#ifdef __cplusplus
}
#endif
#define GETTICK() chimera_gettick()

/* Every rand() in the emulation is glibc's generator, our copy in
 * np2-rand.c, so the native and the sandbox builds cannot differ (PLAN.md
 * section 3, item 10). A function-like macro, defined after compiler_base.h
 * has included <stdlib.h>, so the C++ library's "using ::rand" is left be. */
#ifdef __cplusplus
extern "C" {
#endif
int np2_rand(void);
#ifdef __cplusplus
}
/* for "std::rand()" in C++ library headers included after this one */
namespace std { using ::np2_rand; }
#endif
#define rand() np2_rand()

#include <common/milstr.h>
#include <trace.h>

#endif /* COMPILER_H */
