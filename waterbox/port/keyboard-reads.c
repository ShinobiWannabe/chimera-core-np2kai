/* keyboard-reads.c - where the machine reads the keyboard, for lag frames.
 *
 * A lag frame is one in which the machine read no input. The keyboard is
 * read at port 0x41 (io/serial.c); the bus mouse in mouseif-reads.c, the
 * joystick through joymng_getstat (port/platform.c) and the BIOS's key
 * calls in bios18-reads.c. The upstream file is compiled here, unchanged,
 * by including it, with its bind function renamed: ours calls upstream's
 * and then puts a wrapper in front of the port that notes the read first.
 * (One upstream file a wrapper: the 286 core's cpucore.h has no include
 * guard, so two of them cannot share a file.)
 */
#define keyboard_bind keyboard_bind_upstream
#include "../../extern/NP2kai/io/serial.c"
#undef keyboard_bind
#include "chimera-port.h"

static REG8 IOINPCALL chimera_i41(UINT port) { chimera_input_read(); return keyboard_i41(port); }
static REG8 IOINPCALL chimera_i43(UINT port) { return keyboard_i43(port); }
static const IOINP chimera_keybrdi41[2] = { chimera_i41, chimera_i43 };

void keyboard_bind(void)
{
	keyboard_bind_upstream();
	iocore_attachsysinpex(0x0041, 0x0cf1, chimera_keybrdi41, 2);
}
