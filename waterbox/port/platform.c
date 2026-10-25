/* platform.c - the small parts of NP2kai's platform layer, for the Chimera core.
 *
 * Each block is one of upstream's sdl/*.h interfaces. Most of them are things
 * a sandboxed machine does not have: no sound device (the driver pulls the
 * stream instead, see sound-chimera.c), no serial or MIDI ports, no host
 * fonts, no settings file, no window caption.
 */
#include <compiler.h>
#include <string.h>
#include <time.h>
#include <pccore.h>
#include <soundmng.h>
#include <sysmng.h>
#include <taskmng.h>
#include <timemng.h>
#include <mousemng.h>
#include <joymng.h>
#include <fontmng.h>
#include <commng.h>
#include <ini.h>
#include <np2.h>
#include <generic/softkbd.h>
#include "chimera-port.h"

/* ---- np2.h globals the emulation reads */

NP2OSCFG np2oscfg;
char modulefile[MAX_PATH] = "np2kai";
char draw32bit = 1;
UINT8 scrnmode;
int mmxflag;

/* ---- soundmng.h: no device; the stream is read by the driver */

UINT soundmng_create(UINT rate, UINT ms)
{
	(void)ms;
	/* half a second: far more than one frame makes, so the stream never
	 * runs out of room between two reads */
	return rate / 2;
}
void soundmng_destroy(void) {}
void soundmng_reset(void) {}
void soundmng_play(void) {}
void soundmng_stop(void) {}
void soundmng_sync(void) {}
void soundmng_setreverse(BOOL reverse) { (void)reverse; }
BRESULT soundmng_pcmplay(UINT num, BOOL loop) { (void)num; (void)loop; return FAILURE; }
void soundmng_pcmstop(UINT num) { (void)num; }
BRESULT soundmng_initialize(void) { return SUCCESS; }
void soundmng_deinitialize(void) {}
BRESULT soundmng_pcmload(UINT num, const char *filename) { (void)num; (void)filename; return FAILURE; }
void soundmng_pcmvolume(UINT num, int volume) { (void)num; (void)volume; }
int pcm_volume_default = 25;

/* ---- sysmng.h */

UINT sys_updates;
SYSMNGMISCINFO sys_miscinfo;
void sysmng_initialize(void) {}
void sysmng_deinitialize(void) {}
void sysmng_update(UINT update) { sys_updates |= update; }
void sysmng_cpureset(void) {}
void sysmng_updatecaption(UINT8 flag) { (void)flag; }

/* ---- taskmng.h: the guest's power switch (np2sysp.c's "poweroff") */

BOOL task_avail = TRUE;
void taskmng_initialize(void) { task_avail = TRUE; }
void taskmng_exit(void) { task_avail = FALSE; }
void taskmng_rol(void) {}
BOOL taskmng_sleep(UINT32 tick) { (void)tick; return task_avail; }

/* ---- timemng.h: the calendar clock, from a start time plus emulated time */

static INT64 s_start = 788918400;	/* 1995-01-01 00:00:00 */

void chimera_set_start_time(INT64 seconds) { s_start = seconds; }

BRESULT timemng_gettime(_SYSTIME *st)
{
	UINT32 ms = chimera_gettick();
	time_t t = (time_t)(s_start + ms / 1000);
	struct tm tm;
	if (!gmtime_r(&t, &tm)) return FAILURE;
	st->year = (UINT16)(tm.tm_year + 1900);
	st->month = (UINT16)(tm.tm_mon + 1);
	st->week = (UINT16)tm.tm_wday;
	st->day = (UINT16)tm.tm_mday;
	st->hour = (UINT16)tm.tm_hour;
	st->minute = (UINT16)tm.tm_min;
	st->second = (UINT16)tm.tm_sec;
	st->milli = (UINT16)(ms % 1000);
	return SUCCESS;
}

/* fdd/sxsi.c's GetTickCount() reads this in place of gettimeofday()
 * (np2-flags.sh): emulated time since power-on. */
#include <sys/time.h>
int chimera_gettimeofday(struct timeval *tv, void *tz)
{
	UINT32 ms = chimera_gettick();
	(void)tz;
	tv->tv_sec = (time_t)(ms / 1000);
	tv->tv_usec = (suseconds_t)((ms % 1000) * 1000);
	return 0;
}

/* ---- mousemng.h: relative movement only (docs/PLAN.md, user-decided) */

MOUSEMNG mousemng;

void mousemng_initialize(void)
{
	memset(&mousemng, 0, sizeof mousemng);
	mousemng.btn = uPD8255A_LEFTBIT | uPD8255A_RIGHTBIT;
}

UINT8 mousemng_getstat(SINT16 *x, SINT16 *y, int clear)
{
	*x = mousemng.x;
	*y = mousemng.y;
	if (clear) { mousemng.x = 0; mousemng.y = 0; }
	return mousemng.btn;
}

void mousemng_sync(int mpx, int mpy) { mousemng.x += mpx; mousemng.y += mpy; }
void mousemng_enable(UINT proc) { (void)proc; }
void mousemng_disable(UINT proc) { (void)proc; }
void mousemng_toggle(UINT proc) { (void)proc; }
void mousemng_onmove(int x, int y) { (void)x; (void)y; }

BOOL mousemng_buttonevent(UINT event)
{
	switch (event) {
	case MOUSEMNG_LEFTDOWN:  mousemng.btn &= ~uPD8255A_LEFTBIT; break;
	case MOUSEMNG_LEFTUP:    mousemng.btn |= uPD8255A_LEFTBIT; break;
	case MOUSEMNG_RIGHTDOWN: mousemng.btn &= ~uPD8255A_RIGHTBIT; break;
	case MOUSEMNG_RIGHTUP:   mousemng.btn |= uPD8255A_RIGHTBIT; break;
	}
	return TRUE;
}

void chimera_mouse_move(int dx, int dy)
{
	int x = mousemng.x + dx, y = mousemng.y + dy;
	mousemng.x = (SINT16)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
	mousemng.y = (SINT16)(y > 32767 ? 32767 : (y < -32768 ? -32768 : y));
}

void chimera_mouse_buttons(int left, int right)
{
	mousemng_buttonevent(left ? MOUSEMNG_LEFTDOWN : MOUSEMNG_LEFTUP);
	mousemng_buttonevent(right ? MOUSEMNG_RIGHTDOWN : MOUSEMNG_RIGHTUP);
}

/* ---- joymng.h: the sound board's joystick port */

static UINT8 s_joy = 0xff;
void joymng_sync(void) {}
UINT8 joymng_getstat(void) { chimera_input_read(); return s_joy; }
void chimera_joy_set(uint8_t stat) { s_joy = stat; }

/* ---- fontmng.h: no host fonts. FONT.ROM is required firmware, so NP2kai
 * never needs to draw its own font from one (font/fontmake.c). */

BRESULT fontmng_init(void) { return SUCCESS; }
void fontmng_setdeffontname(const char *name) { (void)name; }
void *fontmng_create(int size, UINT type, const char *fontface) { (void)size; (void)type; (void)fontface; return NULL; }
void fontmng_destroy(void *hdl) { (void)hdl; }
BRESULT fontmng_getsize(void *hdl, const char *s, POINT_T *pt) { (void)hdl; (void)s; (void)pt; return FAILURE; }
BRESULT fontmng_getdrawsize(void *hdl, const char *s, POINT_T *pt) { (void)hdl; (void)s; (void)pt; return FAILURE; }
FNTDAT fontmng_get(void *hdl, const char *s) { (void)hdl; (void)s; return NULL; }

/* ---- commng.h: every port is "not connected" */

static UINT ncread(COMMNG self, UINT8 *data) { (void)self; (void)data; return 0; }
static UINT ncwrite(COMMNG self, UINT8 data) { (void)self; (void)data; return 0; }
static UINT ncwriteretry(COMMNG self) { (void)self; return 1; }
static void ncblock(COMMNG self) { (void)self; }
static UINT nclastwritesuccess(COMMNG self) { (void)self; return 1; }
static UINT8 ncgetstat(COMMNG self) { (void)self; return 0xf0; }
static INTPTR ncmsg(COMMNG self, UINT msg, INTPTR param) { (void)self; (void)msg; (void)param; return 0; }
static void ncrelease(COMMNG self) { (void)self; }

static _COMMNG com_nc = {
	COMCONNECT_OFF, ncread, ncwrite, ncwriteretry, ncblock, ncblock,
	nclastwritesuccess, ncgetstat, ncmsg, ncrelease, 0, 0, 0
};

void commng_initialize(void) {}
COMMNG commng_create(UINT device, BOOL onReset) { (void)device; (void)onReset; return &com_nc; }
void commng_destroy(COMMNG hdl) { (void)hdl; }

/* ---- ini.h: no settings file; the driver sets np2cfg from the project */

void ini_read(const OEMCHAR *path, const OEMCHAR *title, const INITBL *tbl, UINT count) { (void)path; (void)title; (void)tbl; (void)count; }
void ini_write(const OEMCHAR *path, const OEMCHAR *title, const INITBL *tbl, UINT count) { (void)path; (void)title; (void)tbl; (void)count; }
void initload(void) {}
void initsave(void) {}
void initgetfile(OEMCHAR *lpPath, unsigned int cchPath) { if (cchPath) lpPath[0] = '\0'; }

/* ---- generic/softkbd.h: no on-screen keyboard to light */

void softkbd_led(REG8 led) { (void)led; }
