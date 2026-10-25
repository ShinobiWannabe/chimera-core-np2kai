/* np2-rand.c - the C library's rand(), as glibc makes it, for NP2kai.
 *
 * NP2kai calls rand() and never srand(), so the numbers are whatever the C
 * library gives from its default seed of 1. The owner chose NP2kai's Linux
 * builds as the reference (docs/PLAN.md section 3, user-decided), so this is
 * glibc's default generator (random_r.c, TYPE_3): 34 words seeded with
 * 16807 * x mod 2147483647, each next word the sum of the words 31 and 3
 * back, the first 344 thrown away, each result shifted right by one.
 *
 * port/compiler.h defines rand() as np2_rand(), so every call in the
 * emulation lands here and none reaches the C library, which differs
 * between the native (glibc) and the sandbox (musl) builds. The state is an
 * ordinary global: savestates carry it, and a machine reset does not reseed
 * it, as the real program does not.
 */
#include <stdint.h>

#define R_DEG 34

int32_t np2_rand_word(void);
int np2_rand(void);

static int32_t s_r[R_DEG];
static int s_pos;
static int s_seeded;

void np2_srand(unsigned int seed)
{
	int i;
	s_r[0] = (int32_t)(seed ? seed : 1);
	for (i = 1; i < 31; i++) {
		/* 16807 * r mod (2^31 - 1), by Schrage's method as glibc does it */
		int32_t hi = s_r[i - 1] / 127773, lo = s_r[i - 1] % 127773;
		int32_t w = 16807 * lo - 2836 * hi;
		s_r[i] = w < 0 ? w + 2147483647 : w;
	}
	for (i = 31; i < R_DEG; i++)
		s_r[i] = s_r[i - 31];
	s_pos = 0;
	s_seeded = 1;
	for (i = 0; i < 344 - R_DEG; i++)
		(void)np2_rand_word();
}

/* the next word of the sequence r[k] = r[k-31] + r[k-3] */
int32_t np2_rand_word(void)
{
	int32_t v = (int32_t)((uint32_t)s_r[(s_pos + R_DEG - 31) % R_DEG]
	                      + (uint32_t)s_r[(s_pos + R_DEG - 3) % R_DEG]);
	s_r[s_pos] = v;
	s_pos = (s_pos + 1) % R_DEG;
	return v;
}

int np2_rand(void)
{
	if (!s_seeded) np2_srand(1);
	return (int)((uint32_t)np2_rand_word() >> 1);
}
