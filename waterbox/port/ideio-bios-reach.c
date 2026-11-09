/* ideio-bios-reach.c - the real IDE BIOS only for disks it can reach.
 *
 * The real IDE BIOS (ide.rom, D8000h) turns the linear sector number of an
 * INT 1Bh call into cylinder, head and sector by first dividing it by 17
 * with a 16-bit DIV (D800:0770 in the dumps at hand). From sector 17 x 65536
 * = 1114112 (544 MiB) on, the quotient does not fit and the CPU raises a
 * divide error; the BIOS's vector for it is a bare IRET, and a 386 or later
 * returns to the DIV itself, so the same divide runs for ever: a black
 * screen with no disk activity. Windows 2000's loader reads its NTFS files
 * from the middle of a 2 GB disk and stops there. A real PC-9821 with this
 * ROM would stop the same way; it is the ROM's 540 MB limit, not a fault
 * of the emulation.
 *
 * NP2kai already gives a disk whose geometry is not 8 heads x 17 sectors its
 * own stand-in BIOS (autoidebios, cbus/ideio.c ideio_reset), which takes
 * any sector number. Here a hard disk with more sectors than the real BIOS
 * can reach gets the stand-in too: cbus/ideio.c is compiled unchanged, but
 * its ide.rom lookup finds nothing while such a disk is attached, the same
 * as a folder without ide.rom.
 */
#include <compiler.h>
#include <dosio.h>
#include <pccore.h>
#include <fdd/sxsi.h>

#define CHIMERA_IDE_BIOS_SECTORS ((FILEPOS)17 << 16)

static FILEH chimera_ide_rom_open(const OEMCHAR *path)
{
#if defined(SUPPORT_IDEIO)
	REG8 i;
	for (i = 0; i < 4; i++) {
		SXSIDEV sxsi = sxsi_getptr(i);
		if (sxsi != NULL && np2cfg.idetype[i] == SXSIDEV_HDD &&
		    sxsi->devtype == SXSIDEV_HDD && (sxsi->flag & SXSIFLAG_READY) &&
		    sxsi->totals > CHIMERA_IDE_BIOS_SECTORS)
			return FILEH_INVALID;
	}
#endif
	return file_open_rb(path);
}

#define file_open_rb(p) chimera_ide_rom_open(p)
#include "../../extern/NP2kai/cbus/ideio.c"
