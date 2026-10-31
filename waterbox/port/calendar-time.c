/* calendar-time.c - the PC-98's calendar clock, counted in emulated time.
 *
 * The core uses NP2kai's virtual calendar (np2cfg.calendar 0), the one the
 * machine can set with DATE and TIME. Upstream's calendar_inc, called once
 * a frame, adds a second every 56.4 frames: the 24 kHz frame rate. In the
 * 31 kHz modes (70.3 and 60.1 frames a second) the clock ran 25% and 6.6%
 * fast. calendar.c is compiled here, unchanged, with calendar_inc and
 * calendar_initialize renamed; ours count the emulated milliseconds since
 * the last frame (np2-driver.c's clock, which runs at the machine's real
 * speed in every mode), cal.steps keeping the part of a second not yet
 * added. calendar_set is upstream's, so the date the machine sets stays.
 */
#define calendar_inc calendar_inc_upstream
#define calendar_initialize calendar_initialize_upstream
#include "../../extern/NP2kai/calendar.c"
#undef calendar_inc
#undef calendar_initialize
#include "chimera-port.h"

/* the emulated time, in milliseconds, the calendar has counted up to */
static UINT64 s_counted;

static UINT64 uptime_ms(void)
{
	int64_t seconds;
	unsigned int ms;
	chimera_uptime(&seconds, &ms);
	return (UINT64)seconds * 1000 + ms;
}

void calendar_initialize(void)
{
	calendar_initialize_upstream();
	s_counted = uptime_ms();
}

void calendar_inc(void)
{
	UINT64 now = uptime_ms();
	cal.realchg = 1;
	if (now > s_counted) {
		cal.steps += (UINT)(now - s_counted);
		while (cal.steps >= 1000) {
			cal.steps -= 1000;
			secinc(&cal.dt);
		}
	}
	s_counted = now;
}
