/* calendar.c - the PC-98's calendar and the driver's clock. The calendar
 * past GETTICK's 32-bit wrap: the driver's millisecond total is set as if
 * the machine had run for 50 and for 5000 days at 49.152 MHz; the calendar
 * must read the start date plus that, where the 32-bit milliseconds would
 * have gone back after 49.7 days. Then a minute of 31 kHz frames, a CPU
 * multiplier change and a reset (np2-driver.c, the clock).
 *
 * Built by run-gate.sh against the native build's objects. */
#include "../waterbox/np2-driver.c"
#include <timemng.h>
#include <calendar.h>

/* what run-native.c gives the driver natively */
void *alloc_invisible(size_t size) { return calloc(1, size); }

int main(void)
{
	static const struct { uint64_t days; int year, month, day; } k[] = {
		{ 0, 1995, 1, 1 }, { 50, 1995, 2, 20 }, { 5000, 2008, 9, 9 } };
	int i, bad = 0;
	pccore.realclock = 49152000;
	chimera_set_start_time(788918400);	/* 1995-01-01 00:00:00 */
	for (i = 0; i < (int)NELEMENTS(k); i++) {
		_SYSTIME st;
		/* and 1.5 seconds more, so the milliseconds are checked too */
		g_ms = (k[i].days * 86400 + 1) * 1000 + 500;
		g_msRem = 0;
		g_lastClock = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
		if (timemng_gettime(&st) != SUCCESS || st.year != k[i].year || st.month != k[i].month
		    || st.day != k[i].day || st.hour || st.minute || st.second != 1 || st.milli != 500) {
			printf("%d days: got %04d-%02d-%02d %02d:%02d:%02d.%03d\n", (int)k[i].days,
			       st.year, st.month, st.day, st.hour, st.minute, st.second, st.milli);
			bad++;
		}
	}
	/* A minute of 31 kHz frames (70.32 a second, 1/70.32 s of clocks
	 * each): the calendar must move one minute, as the emulated time did.
	 * It moved a second every 56.4 frames before, 75 s here. */
	{
		const uint64_t rate = 49152000, frame = 698995;	/* 49152000 / 70.32 */
		_SYSTIME before, after;
		long i, frames = (long)(60 * rate / frame), secs;
		pccore.realclock = (UINT32)rate;
		g_ms = 0; g_msRem = 0; g_clocks = 0;
		CPU_CLOCK = 0; CPU_BASECLOCK = 0; CPU_REMCLOCK = 0;
		g_lastClock = 0;
		calendar_initialize();
		before = cal.dt;
		for (i = 0; i < frames; i++) {
			CPU_CLOCK += (UINT32)frame;
			calendar_inc();
		}
		after = cal.dt;
		secs = (after.minute * 60 + after.second) - (before.minute * 60 + before.second);
		if (secs != 59 && secs != 60) {
			printf("a minute of 31 kHz frames moved the calendar %ld s\n", secs);
			bad++;
		}

		/* the multiplier doubled after that minute: the minute stays a
		 * minute, and a second at the new rate adds a second */
		{
			unsigned int t0 = chimera_gettick(), t1, t2;
			pccore.realclock = (UINT32)(rate * 2);
			t1 = chimera_gettick();
			CPU_CLOCK += (UINT32)(rate * 2);
			t2 = chimera_gettick();
			if (t1 != t0 || t2 - t1 != 1000) {
				printf("multiplier change: %u ms, then %u, then %u\n", t0, t1, t2);
				bad++;
			}
		}

		/* a reset puts the CPU's count back to zero from past 2^31: no time
		 * passes, and the next step counts as itself */
		{
			unsigned int t0, t1, t2;
			CPU_CLOCK = 0x90000000u;
			chimera_gettick();
			t0 = chimera_gettick();
			chimera_reset_begin();
			CPU_CLOCK = 0;
			chimera_reset_end();
			t1 = chimera_gettick();
			CPU_CLOCK += (UINT32)(rate * 2);	/* a second at the doubled rate */
			t2 = chimera_gettick();
			if (t1 != t0 || t2 - t1 != 1000) {
				printf("reset: %u ms, then %u, then %u\n", t0, t1, t2);
				bad++;
			}
		}
	}
	printf("%d dates, a 31 kHz minute, a multiplier change and a reset checked, %d wrong\n", (int)NELEMENTS(k), bad);
	return bad != 0;
}
