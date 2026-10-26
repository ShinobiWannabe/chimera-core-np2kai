/* opna-fmgen-caps.c - fmgen given only the registers the board's chip has,
 * as Neko Project 21/W ver0.86 rev106 does ("26Kなのにリズム音が鳴ってしまっ
 * ていた不具合修正").
 *
 * fmgen is always a YM2608, and sound/opna.c hands it every register write,
 * so with the fmgen sound generator a board with a YM2203 (PC-9801-26K,
 * Sound Orchestra, Little Orchestra L, Multimedia Orchestra) also played
 * the rhythm, ADPCM, channels 4 to 6 and the LFO when a program wrote those
 * registers, and an 86 without ADPCM memory played ADPCM. NP2's own
 * generators already look at the chip's capabilities (opna->s.cCaps).
 * sound/opna.c is compiled here, unchanged, with its fmgen register writes
 * sent through NP21/W rev106's filter (win9x/ext/opna.cpp,
 * writeFmgenRegister). As in NP21/W, the writes opna_reset makes to set
 * fmgen up go to fmgen unfiltered. Only the sound changes, never the machine.
 */
#include <compiler.h>
#include "../../extern/NP2kai/sound/opna.h"
#if defined(SUPPORT_FMGEN)
#include <sound/fmgen/fmgen_fmgwrap.h>

static UINT8 fmgen_unfiltered;	/* set while opna_reset runs */

/* NP21/W rev106's writeFmgenRegister, as it is */
static void writeFmgenRegister(POPNA opna, UINT nAddress, REG8 cData)
{
	const UINT8 cCaps = opna->s.cCaps;

	if (nAddress < 0x100)
	{
		if (nAddress < 0x10)
		{
			if (!(cCaps & OPNA_HAS_PSG))
			{
				return;
			}
		}
		else if (nAddress < 0x20)
		{
			if (!(cCaps & OPNA_HAS_RHYTHM))
			{
				return;
			}
		}
		else
		{
			if (!(cCaps & OPNA_HAS_FM) || (nAddress >= 0xc0))
			{
				return;
			}

			// YM2203 has no LFO, status-mask register, or PAN/AMS/PMS.
			if ((nAddress == 0x22) || (nAddress == 0x29) ||
				(((nAddress & 0xfc) == 0xb4)))
			{
				if (!(cCaps & OPNA_HAS_EXTENDEDFM))
				{
					return;
				}
			}

			// 0x28 can address channels 4-6 on OPNA/OPN2, but only 1-3 on OPN.
			if (nAddress == 0x28)
			{
				const REG8 cChannel = cData & 0x0f;
				if (cChannel >= 3)
				{
					if (!(cCaps & OPNA_HAS_EXTENDEDFM) ||
						(cChannel < 4) || (cChannel >= 7))
					{
						return;
					}
				}
			}

			// FMGEN's OPNA accepts AM enable (bit 7) here; YM2203 ignores it.
			if (!(cCaps & OPNA_HAS_EXTENDEDFM) &&
				((nAddress & 0xf0) == 0x60))
			{
				cData &= 0x1f;
			}
		}
	}
	else
	{
		const UINT nExtendedAddress = nAddress & 0xff;

		if (nExtendedAddress < 0x12)
		{
			if (!(cCaps & OPNA_HAS_ADPCM))
			{
				return;
			}
		}
		else if (nExtendedAddress >= 0x30)
		{
			if (!(cCaps & OPNA_HAS_EXTENDEDFM))
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	OPNA_SetReg(opna->fmgen, nAddress, cData);
}

static void chimera_fmgen_setreg(POPNA opna, void *fmgen, UINT nAddress, REG8 cData)
{
	if (fmgen_unfiltered)
	{
		OPNA_SetReg(fmgen, nAddress, cData);
	}
	else
	{
		writeFmgenRegister(opna, nAddress, cData);
	}
}

/* every OPNA_SetReg in sound/opna.c has its instance in scope as opna */
#define OPNA_SetReg(fmgen, nAddress, cData) chimera_fmgen_setreg(opna, (fmgen), (nAddress), (cData))
#endif	/* SUPPORT_FMGEN */

#define opna_reset opna_reset_upstream
#include "../../extern/NP2kai/sound/opna.c"
#undef opna_reset

void opna_reset(POPNA opna, REG8 cCaps)
{
#if defined(SUPPORT_FMGEN)
	fmgen_unfiltered = 1;
#endif
	opna_reset_upstream(opna, cCaps);
#if defined(SUPPORT_FMGEN)
	fmgen_unfiltered = 0;
#endif
}
