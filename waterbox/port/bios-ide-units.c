/* bios-ide-units.c - the hard disk units the real IDE BIOS is told about.
 *
 * The IDE BIOS (D8000h) reads BIOS work byte 0457h, which a PC-9821's own
 * start-up fills: bits 3-5 are unit 0's drive type and bits 0-2 unit 1's,
 * 7 meaning "no drive", and bits 7 and 6 say units 0 and 1 have 512-byte
 * sectors. Upstream leaves it 0 (two 256-byte-sector drives) unless
 * WINNTFIX is on with only one disk, so the BIOS sends commands to a slave
 * that is not there, waiting 25 seconds for each before it gives up, and
 * then reads the master as 256-byte sectors: no hard disk starts. NP2kai's
 * own stand-in BIOS never reads the byte, so it is left as it was then.
 *
 * bios/bios.c is compiled here unchanged. bios_reinitbyswitch, which fills
 * the work area after every reset and boot, ends by calling
 * cbuspnp_bios_update; in this file that call also fills 0457h, the way
 * upstream's WINNTFIX does for one disk: type 2 (the IDE BIOS puts in the
 * real type of a disk of 32 MB or more itself), 512-byte sectors.
 */
#include <compiler.h>
#include <cbus/cbuspnp.h>	/* declared before the macro below renames its calls */

static void chimera_ide_units(void);
#define cbuspnp_bios_update() (cbuspnp_bios_update(), chimera_ide_units())
#include "../../extern/NP2kai/bios/bios.c"

#if defined(SUPPORT_IDEIO)
/* a unit the IDE emulation answers as a hard disk (cbus/ideio.c devinit) */
static int chimera_ide_hdd(REG8 unit)
{
	SXSIDEV sxsi = sxsi_getptr(unit);
	return sxsi != NULL && sxsi->devtype == SXSIDEV_HDD && (sxsi->flag & SXSIFLAG_READY);
}
#endif

static void chimera_ide_units(void)
{
#if defined(SUPPORT_IDEIO)
	/* only for the real IDE BIOS: with NP2kai's stand-in nothing changes */
	if ((pccore.hddif & PCHDD_IDE) && ideio.bios == IDETC_BIOS) {
		mem[0x0457] = (chimera_ide_hdd(0) ? 0x90 : 0x38) | (chimera_ide_hdd(1) ? 0x42 : 0x07);
	}
#endif
}
