/* tms3631-pitch.c - the PC-9801-14's note table, a whole 16 times too high
 * upstream.
 *
 * sound/tms3631g.c steps each channel TMS3631_MUL (4) times a sample, but
 * tms3631_initialize also multiplies each step by TMS3631_MUL, so A4 (440 Hz)
 * plays at 7040 Hz. sound/tms3631c.c is compiled here, unchanged, with its
 * table builder renamed, and ours builds the table with the multiplier once.
 */
#define tms3631_initialize tms3631_initialize_upstream
#include "../../extern/NP2kai/sound/tms3631c.c"
#undef tms3631_initialize

void tms3631_initialize(UINT rate)
{
	UINT i, j;
	double f;

	tms3631_initialize_upstream(rate);
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 12; j++)
		{
			f = 440.0 * pow(2.0, (i - 3.0) + ((j - 9.0) / 12.0));
			f = f * (1 << (TMS3631_FREQ + 1)) / ((double)rate * TMS3631_MUL);
			tms3631cfg.freqtbl[(i * 16) + j + 1] = (UINT32)floor(f + 0.5);
		}
	}
}
