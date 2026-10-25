/* boardlol-fixed.c - the SNE Little Orchestra L, which crashes upstream.
 *
 * cbus/boardlol.c never calls opna_reset, so the chip's mode and sound
 * generators are never set up, and its bind registers the FM chip's state
 * with psggen_getpcm as if it were an SSG, which reads past it at the first
 * sound (its psgpanset makes the same mistake). The file is compiled here, unchanged, for its ports; reset and bind
 * are ours, set up as the other YM2203 boards are (board26k.c, boardmo.c):
 * a YM2203 with its FM fixed to the left and its SSG to the right.
 */
#define boardlol_reset boardlol_reset_upstream
#define boardlol_bind boardlol_bind_upstream
#include "../../extern/NP2kai/cbus/boardlol.c"
#undef boardlol_reset
#undef boardlol_bind

static void boardlol_setpan(void)
{
	UINT i;

	/* not upstream's psgpanset, which hands the chip itself to psggen */
	for (i = 0; i < 3; i++)
	{
		psggen_setpan(&g_opna[0].psg, i, 1);
		g_opna[0].s.reg[0xb4 + i] = 1 << 7;
		opngen_setreg(&g_opna[0].opngen, 0, 0xb4 + i, 1 << 7);
	}
}

void boardlol_reset(const NP2CFG *pConfig)
{
	opna_reset(&g_opna[0], OPNA_MODE_2203 | OPNA_HAS_TIMER | OPNA_S98);
	opna_timer(&g_opna[0], (pConfig->snd26opt & 0xc0) | 0x10, NEVENT_FMTIMERA, NEVENT_FMTIMERB);
	opngen_setcfg(&g_opna[0].opngen, 3, OPN_STEREO | 0x007);
	soundrom_loadex(pConfig->snd26opt & 7, OEMTEXT("26"));
	g_opna[0].s.base = (pConfig->snd26opt & 0x10) ? 0x000 : 0x100;
	boardlol_setpan();
}

void boardlol_bind(void)
{
	boardlol_setpan();
	opna_bind(&g_opna[0]);
	boardlol_setpan();
	cbuscore_attachsndex(0x188 - g_opna[0].s.base, opn_o, opn_i);
}
