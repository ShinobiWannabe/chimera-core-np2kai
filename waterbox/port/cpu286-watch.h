/* cpu286-watch.h - for the calls at memory on the 286 machine (memwatch.c).
 *
 * The 286 CPU's read-modify-write instructions on a byte (ADD [m],r, INC,
 * NOT, NEG and the rest, in i286c_mn.c, i286c_f6.c, i286c_fe.c and
 * v30patch.c) work on mem[] through a pointer when the address is below
 * I286_MEMWRITEMAX (640 KiB), and so never reach memp_read8/memp_write8.
 * Those four files are compiled through cpu286-*.c with I286_MEMWRITEMAX
 * made 0 while a script watches reads or writes, so every such access
 * takes NP2kai's other path, memp_read8 then memp_write8, which does the
 * same thing to the machine and is reported. (Words already take that path:
 * INHIBIT_WORDP is always 1 in this build.) */
#include <compiler.h>
#include <cpucore.h>
#include "memwatch.h"
enum { CHIMERA_I286_MEMWRITEMAX = I286_MEMWRITEMAX };
#define I286_MEMWRITEMAX ((UINT32)((chimera_watch_read_on | chimera_watch_write_on) ? 0 : CHIMERA_I286_MEMWRITEMAX))
