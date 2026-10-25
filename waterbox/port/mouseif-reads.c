/* mouseif-reads.c - where the machine reads the bus mouse, for lag frames:
 * ports 0x7fd9-0x7fdd (io/mouseif.c), wrapped as keyboard-reads.c wraps
 * the keyboard. */
#define mouseif_bind mouseif_bind_upstream
#include "../../extern/NP2kai/io/mouseif.c"
#undef mouseif_bind
#include "chimera-port.h"

static REG8 IOINPCALL chimera_i7fd9(UINT port) { chimera_input_read(); return mouseif_i7fd9(port); }
static REG8 IOINPCALL chimera_i7fdb(UINT port) { chimera_input_read(); return mouseif_i7fdb(port); }
static REG8 IOINPCALL chimera_i7fdd(UINT port) { chimera_input_read(); return mouseif_i7fdd(port); }

void mouseif_bind(void)
{
	mouseif_bind_upstream();
	iocore_attachinp(0x7fd9, chimera_i7fd9);
	iocore_attachinp(0x7fdb, chimera_i7fdb);
	iocore_attachinp(0x7fdd, chimera_i7fdd);
}
