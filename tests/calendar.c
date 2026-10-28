/* calendar.c - the PC-98's calendar past GETTICK's 32-bit wrap. The
 * driver's clock total is set as if the machine had run for 50 and for
 * 5000 days at 49.152 MHz; the calendar must read the start date plus
 * that, where the 32-bit milliseconds would have gone back after 49.7 days.
 *
 * Built by run-gate.sh against the native build's objects. */
#include "../waterbox/np2-driver.c"
#include <timemng.h>

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
		g_clocks = (k[i].days * 86400 + 1) * pccore.realclock + pccore.realclock / 2;
		g_lastClock = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
		if (timemng_gettime(&st) != SUCCESS || st.year != k[i].year || st.month != k[i].month
		    || st.day != k[i].day || st.hour || st.minute || st.second != 1 || st.milli != 500) {
			printf("%d days: got %04d-%02d-%02d %02d:%02d:%02d.%03d\n", (int)k[i].days,
			       st.year, st.month, st.day, st.hour, st.minute, st.second, st.milli);
			bad++;
		}
	}
	printf("%d dates, %d wrong\n", (int)NELEMENTS(k), bad);
	return bad != 0;
}
