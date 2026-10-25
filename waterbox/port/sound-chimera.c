/* sound-chimera.c - NP2kai's sound stream, with a way out for the driver.
 *
 * sound/sound.c is compiled here, unchanged, by including it. The libretro
 * port takes sound with sound_pcmlock, which hands back a FIXED number of
 * samples a call and makes up the difference from the chips' current state.
 * A PC-98 frame is not a whole number of samples (44100 / 56.42 Hz is about
 * 781.6), so that would add or drop sound every frame. Here the driver takes
 * exactly what the emulated clock has reached, and the stream's own static
 * state is in reach for that because this file includes sound.c.
 */
#include "../../extern/NP2kai/sound/sound.c"
#include "chimera-port.h"

int chimera_sound_take(int16_t *out, int max)
{
	const SINT32 *p;
	int n, i;

	if (sndstream.buffer == NULL)
		return 0;
	/* bring the stream up to the current clock, however short the gap */
	{
		UINT32 length = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK - soundcfg.lastclock;
		length = (UINT32)(((UINT64)length * soundcfg.hzbase) / soundcfg.clockbase);
		if (length) {
			streamprepare(length);
			soundcfg.lastclock += length * soundcfg.clockbase / soundcfg.hzbase;
			beep_eventreset();
		}
	}
	n = (int)((sndstream.ptr - sndstream.buffer) / 2);
	if (n > max) n = max;
	p = sndstream.buffer;
	for (i = 0; i < n * 2; i++) {
		SINT32 s = p[i];
		out[i] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
	}
	sndstream.ptr = sndstream.buffer;
	sndstream.remain = sndstream.samples + sndstream.reserve;
	return n;
}
