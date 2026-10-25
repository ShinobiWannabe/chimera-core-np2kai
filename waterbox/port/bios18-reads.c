/* bios18-reads.c - the keyboard BIOS's reads, for lag frames.
 *
 * Most PC-98 programs never touch the keyboard port: they ask the BIOS
 * (INT 18h) for a key, the key buffer or the shift state, and the port is
 * read only by the BIOS's interrupt handler when a key arrives. NP2kai's
 * own BIOS answers INT 18h in C (bios/bios18.c), so that file is compiled
 * here, unchanged, with its entry renamed, and ours notes the read first.
 * With the real BIOS (bios.rom) these calls run as x86 code and only the
 * ports are seen (keyboard-reads.c, mouseif-reads.c).
 */
#define bios0x18 bios0x18_upstream
#include "../../extern/NP2kai/bios/bios18.c"
#undef bios0x18
#include "chimera-port.h"

void bios0x18(void)
{
	switch (CPU_AH) {
	case 0x00:	/* read a key */
	case 0x01:	/* sense the key buffer */
	case 0x02:	/* sense the shift keys */
	case 0x04:	/* sense a group of keys */
		chimera_input_read();
		break;
	}
	bios0x18_upstream();
}
