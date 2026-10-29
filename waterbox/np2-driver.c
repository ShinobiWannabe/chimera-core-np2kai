/* np2-driver.c - NP2kai as a Chimera core.
 *
 * The driver is what the frontend calls: Init, FrameAdvance and the readers
 * of picture, sound and memory. It does what upstream's np2_main does to
 * start a machine (sdl/np2.c), with the machine's configuration taken from
 * the project's settings instead of np2.cfg, and then runs one PC-98 frame
 * (one display sync, pccore_exec) per FrameAdvance.
 *
 * Built twice from the same source: once for the miniBox guest and once
 * natively (run-native.c), so the gate compares like with like. Both read
 * the "settings" and "slots" files of the folder they run in.
 */
#include <compiler.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emulibc.h>
#include <waterbox_settings.h>
#include <waterbox_slots.h>

#include <dosio.h>
#include <pccore.h>
#include <cpucore.h>
#include <keystat.h>
#include <scrnmng.h>
#include <soundmng.h>
#include <sysmng.h>
#include <taskmng.h>
#include <mousemng.h>
#include <commng.h>
#include <fontmng.h>
#include <font/font.h>
#include <vram/scrndraw.h>
#include <fdd/diskdrv.h>
#include <fdd/sxsi.h>
#if defined(SUPPORT_WAB)
#include <wab/wab.h>
#endif
#if defined(SUPPORT_CL_GD5430)
#include <wab/cirrus_vga_extern.h>
#endif

#include "port/chimera-port.h"
#include "np2-driver.h"
#include "np2-input.h"
#include "np2-settings.h"

static char g_loadError[512];
static uint32_t *g_frame;
/* turbo (SetRenderingEnabled) is the frontend's choice, not the machine's:
 * invisible, so a state taken in turbo does not bring it back */
static ECL_INVISIBLE int g_render = 1;
static void list_drives(void);
/* the drives NP2kai read or wrote this frame, and last frame's, which the
 * lights show ([0] the floppy units, [1] the SASI/IDE units) */
static uint8_t g_accessed[2][4], g_lit[2][4];
static int g_width = 640, g_height = 400;
static int16_t *g_audio;
static int g_audioPairs;
static int g_inputRead;
static uint64_t g_frames;
/* the save-data window (see the save data section) */
#define SAVEDATA_WINDOW (256 * 1024)
static uint8_t *g_saveWindow;

/* ------------------------------------------------------------ the clock
 *
 * Emulated milliseconds since power-on, from the CPU's own clock count.
 * CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK is the count NP2kai's sound uses
 * (sound.c); it is 32 bits and wraps every minute and a half at 49 MHz, so
 * each read adds what passed since the last one to a 64-bit total. A reset
 * puts the count back to zero, which reads here as a step backwards and adds
 * nothing. */
static uint64_t g_clocks;
static UINT32 g_lastClock;

#if defined(NP2DRV_HOST_CLOCK)
/* The gate's negative control only: the host's clock, as libretro's
 * GETTICK is. The stall test must fail with it. Never in a real build. */
#include <time.h>
unsigned int chimera_gettick(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
void chimera_uptime(int64_t *seconds, unsigned int *ms)
{
	unsigned int t = chimera_gettick();
	*seconds = t / 1000;
	*ms = t % 1000;
}
#else
uint64_t np2drv_clocks(void);
unsigned int chimera_gettick(void)
{
	np2drv_clocks();
	if (!pccore.realclock) return 0;
	return (unsigned int)(g_clocks * 1000u / pccore.realclock);
}

/* The same time for the calendar, whole: GETTICK's 32 bits wrap after 49.7
 * days, which is fine for the timers (they take differences) but would set
 * the calendar back. The 64-bit count lasts thousands of years. */
void chimera_uptime(int64_t *seconds, unsigned int *ms)
{
	np2drv_clocks();
	*seconds = 0;
	*ms = 0;
	if (!pccore.realclock) return;
	*seconds = (int64_t)(g_clocks / pccore.realclock);
	*ms = (unsigned int)(g_clocks % pccore.realclock * 1000u / pccore.realclock);
}
#endif

uint64_t np2drv_clocks(void)
{
	UINT32 now = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
	UINT32 step = now - g_lastClock;
	if ((INT32)step > 0) g_clocks += step;
	g_lastClock = now;
	return g_clocks;
}

/* ------------------------------------------------------------ settings */

/* how many drives get a disk at power-on (the Floppies at Start setting) */
static int g_floppiesAtStart = 2;

static void setting_str(const char *key, const char *dflt, char *out, int outsz)
{
	if (wbx_setting_str(key, out, outsz) < 0) {
		strncpy(out, dflt, (size_t)outsz - 1);
		out[outsz - 1] = 0;
	}
}

/* np2cfg as the Linux SDL build starts with no np2.cfg (docs/PLAN.md
 * section 9, user-decided): pccore.c's initialiser, which this build shares,
 * except where the libretro branch of it differs. Then the settings, and then
 * what section 3 forces for one clock. */
static int slot_names(const char *id, char names[][MAX_PATH], int max);

static void configure(void)
{
	char model[16], name[64];

#if defined(SUPPORT_WAB)
	np2cfg.wabasw = 0;	/* pccore.c gives libretro 1, SDL 0 */
#endif
	setting_str("model", "VX", model, sizeof model);
	milstr_ncpy(np2cfg.model, model, NELEMENTS(np2cfg.model));
	/* each default is what pccore.c's initialiser already put there */
	setting_str("baseClock", "", name, sizeof name);
#define X(option, hz) if (!strcmp(name, option)) np2cfg.baseclock = hz;
	NP2_BASE_CLOCKS(X)
#undef X
	/* a list of numbers, so the value comes as text */
	setting_str("clockMultiplier", "", name, sizeof name);
	if (strtol(name, NULL, 10) > 0) np2cfg.multiple = (UINT)strtol(name, NULL, 10);
	np2cfg.EXTMEM = (UINT16)wbx_setting_long("extraMemoryMB", np2cfg.EXTMEM);
	setting_str("soundBoard", "", name, sizeof name);
#define X(option, id) if (!strcmp(name, option)) np2cfg.SOUND_SW = id;
	NP2_SOUND_BOARDS(X)
#undef X
	setting_str("soundGenerator", "NP2", name, sizeof name);
#define X(option, fmgen) if (!strcmp(name, option)) np2cfg.usefmgen = fmgen;
	NP2_SOUND_GENERATORS(X)
#undef X
	setting_str("rhythmSamples", "WAV files", name, sizeof name);
#define X(option, rom) if (!strcmp(name, option)) chimera_rhythm_rom = rom;
	NP2_RHYTHM_SAMPLES(X)
#undef X
	setting_str("floppiesAtStart", "Drives 1 and 2", name, sizeof name);
#define X(option, drives) if (!strcmp(name, option)) g_floppiesAtStart = drives;
	NP2_FLOPPIES_AT_START(X)
#undef X
	np2cfg.samplingrate = 44100;
	/* the real BIOS, sound BIOS, IDE BIOS (PC-9821) and drum samples
	 * are required firmware (owner, 2026-10-24): never NP2kai's stand-ins */
	np2cfg.usebios = 1;
#if defined(SUPPORT_IDEIO)
	/* the real IDE BIOS waits for ever on a bus with no hard disk (it
	 * polls the empty unit for its ready bit), so it is required, and used,
	 * when the project has a hard disk; without one NP2kai's own stands in */
	{
		char hd[1][MAX_PATH];
		np2cfg.idebios = slot_names("hdd", hd, 1) ? 1 : 0;
	}
#endif

	/* section 3: one thread on one clock */
	np2cfg.calendar = 0;	/* the virtual calendar, from timemng */
	np2cfg.cal_vofs = 0;
	np2cfg.keyrepeat_enable = 0;
	np2cfg.fpu_type = FPU_TYPE_SOFTFLOAT;
	np2cfg.MOTOR = 0;
	np2cfg.biospath[0] = '\0';	/* firmware is mounted under its own name */
	file_setcd("");

	chimera_set_start_time(wbx_setting_long("startTime", 788918400));
}

/* ------------------------------------------------------------ the frame rate
 *
 * One frame is one display sync. Chimera asks for the rate once, after
 * Init, so it is the rate of the mode the Monitor setting names
 * (np2-settings.h), worked out as io/gdc.c works out the frame's length:
 * the frame is (baseclock * lines) / (dot clock / dots) base clocks, so the
 * rate is baseclock over that, exactly. */
static int32_t g_vsyncNum = 60, g_vsyncDen = 1;

static void frame_rate(void)
{
	char name[32];
	UINT32 hclock = 0, lines = 0;
	uint64_t num, den, a, b;
	setting_str("monitor", "24khz", name, sizeof name);
#define X(option, clock, dots, frame) \
	if (!hclock || !strcmp(name, option)) { hclock = (clock) / (dots); lines = frame; }
	NP2_MONITORS(X)
#undef X
	num = pccore.baseclock;
	den = (uint64_t)pccore.baseclock * lines / hclock;
	for (a = num, b = den; b; ) { uint64_t t = a % b; a = b; b = t; }
	g_vsyncNum = (int32_t)(num / a);
	g_vsyncDen = (int32_t)(den / a);
}

/* ------------------------------------------------------------ disks
 *
 * The project's files arrive as slots (file_slots.json): each mounted under
 * its own name, listed in swap order. Hard disks have to be named in np2cfg
 * before pccore_init; floppies and the CD go in after the reset, as
 * np2_main does it. */

/* A disk opened with no project (Chimera's --core=<package> <file>, or a
 * file opened from the File menu) comes with no "slots" file: the engine
 * mounts it as "rom" and under its own name, and puts that name in
 * "rom.name". Its extension says which slot it would have gone in
 * (file_slots.json), and NP2kai tells the image's format by it too. */
static const char *bare_slot(char *name)
{
	static const char *const floppy[] = { "d88", "88d", "d98", "98d", "fdi", "hdm", "xdf", "dup", "2hd", "tfd", "nfd", "dcp", "dcu", NULL };
	static const char *const hdd[] = { "hdi", "nhd", "thd", "vhd", NULL };
	static const char *const cdrom[] = { "cue", "iso", NULL };
	static const struct { const char *id; const char *const *ext; } kinds[] = {
		{ "floppy", floppy }, { "hdd", hdd }, { "cdrom", cdrom } };
	const char *ext;
	size_t n, k, e;
	FILE *f;
	if (wbx_slot_count("floppy") || wbx_slot_count("hdd") || wbx_slot_count("cdrom"))
		return NULL;
	f = fopen("rom.name", "rb");
	if (!f) return NULL;
	n = fread(name, 1, MAX_PATH - 1, f);
	fclose(f);
	name[n] = '\0';
	while (n && (name[n - 1] == '\n' || name[n - 1] == '\r')) name[--n] = '\0';
	if (name[0] == '/') memmove(name, name + 1, n--);
	ext = file_getext(name);
	for (k = 0; k < sizeof kinds / sizeof kinds[0]; k++)
		for (e = 0; kinds[k].ext[e]; e++)
			if (!milstr_cmp(ext, kinds[k].ext[e])) {
				/* the name may be mounted only as "rom" (a guest mount
				 * keeps the engine's leading slash in its name) */
				chimera_vfs_alias(name, "rom");
				return kinds[k].id;
			}
	return NULL;
}

static int slot_names(const char *id, char names[][MAX_PATH], int max)
{
	int n = wbx_slot_count(id), i, got = 0;
	const char *bare;
	if (n == 0 && max > 0 && (bare = bare_slot(names[0])) != NULL)
		return strcmp(bare, id) == 0;
	for (i = 0; i < n && got < max; i++)
		if (wbx_slot_name(id, i, names[got], MAX_PATH) && names[got][0])
			got++;
	return got;
}

/* the SASI/IDE unit each hard disk went in, and the CD-ROM drive's, for
 * the drive lights */
static int g_hddUnit[2], g_hddCount, g_cdUnit = -1;

static void attach_hard_disks(void)
{
	char names[4][MAX_PATH];
	int n = slot_names("hdd", names, 2), i, j = 0;
	for (i = 0; i < 4; i++) np2cfg.sasihdd[i][0] = '\0';
	for (i = 0; i < n; i++) {
#if defined(SUPPORT_IDEIO)
		/* the IDE units that are hard disks, in order (np2cfg.idetype) */
		while (j < 4 && np2cfg.idetype[j] != SXSIDEV_HDD) j++;
		if (j >= 4) break;
#endif
		/* without IDE (the 286 machine) the first two are the SASI disks */
		g_hddUnit[g_hddCount++] = j;
		milstr_ncpy(np2cfg.sasihdd[j++], names[i], MAX_PATH);
	}
}

/* every floppy of the project, however many (file_slots.json has no limit) */
static char (*g_floppies)[MAX_PATH];
/* the disk each drive's selector is on, and the disk in it (-1: none) */
static int g_pendingFloppy[2], g_insertedFloppy[2] = { -1, -1 };
static int g_floppyCount;

/* NP2kai's cue reader takes one FILE line, the first: a cue sheet with a
 * file a track (the usual layout, and what Chimera adds to a project by
 * itself) would read every track from the first file. Such a sheet is read
 * as one image instead: its files joined end to end in the file layer, and
 * a sheet of one FILE whose INDEX positions are moved by the sectors of the
 * files before. Only BINARY files of raw 2352-byte tracks are joined;
 * anything else is left to NP2kai as it is. Each line is split as NP2kai's
 * own reader splits it (diskimage/cd/cdd_cue.c: milstr_getarg, words in any
 * case, names with or without quotes), after a UTF-8 byte order mark, which
 * NP2kai's text reader skips too. The joined sheet keeps the lines NP2kai
 * reads (TRACK, PREGAP, INDEX); it ignores the rest (FLAGS, POSTGAP, REM).
 * 1 and the joined sheet's path in out, 0 when the sheet is used as it is. */
#define CUE_MAX_FILES 99

/* the next line of text from *at, its words in argv; the word count */
static int cue_line(const char *text, size_t len, size_t *at, char *line, size_t linesize, char *argv[8])
{
	size_t e = *at, n;
	while (e < len && text[e] != '\n') e++;
	n = e - *at;
	if (n >= linesize) n = linesize - 1;
	memcpy(line, text + *at, n);
	line[n] = '\0';
	*at = e + 1;
	return milstr_getarg(line, argv, 8);
}

static int join_cue(const char *cue, char *out, size_t outsize)
{
	static char text[65536], sheet[65536];
	static char names[CUE_MAX_FILES][MAX_PATH];
	const char *parts[CUE_MAX_FILES];
	UINT64 base[CUE_MAX_FILES], sectors = 0;
	char dir[MAX_PATH], bin[MAX_PATH], line[512], *argv[8], *q;
	int nfiles = 0, i, argc, ok = 1;
	size_t len, start = 0, at, used = 0;
	FILEH fh;
	const char *ext = file_getext(cue);
	if (milstr_cmp(ext, "cue")) return 0;
	fh = file_open_rb(cue);
	if (fh == FILEH_INVALID) return 0;
	len = file_read(fh, text, sizeof text - 1);
	file_close(fh);
	text[len] = '\0';
	if (len >= 3 && !memcmp(text, "\xef\xbb\xbf", 3)) start = 3;
	milstr_ncpy(dir, cue, sizeof dir);
	file_cutname(dir);
	/* the files, and the sector each starts at */
	for (at = start; at < len; ) {
		argc = cue_line(text, len, &at, line, sizeof line, argv);
		if (argc >= 3 && !milstr_cmp(argv[0], "FILE")) {
			/* WAVE, MOTOROLA, AIFF, MP3: not raw sectors */
			if (milstr_cmp(argv[argc - 1], "BINARY") || nfiles >= CUE_MAX_FILES) return 0;
			/* a name written with Windows' separator */
			for (q = argv[1]; *q; q++) if (*q == '\\') *q = '/';
			milstr_ncpy(names[nfiles], dir, MAX_PATH);
			file_catname(names[nfiles], argv[1], MAX_PATH);
			nfiles++;
		}
		else if (argc >= 3 && !milstr_cmp(argv[0], "TRACK")) {
			size_t m = strlen(argv[2]);
			if (milstr_cmp(argv[2], "AUDIO") && (m < 5 || strcmp(argv[2] + m - 5, "/2352"))) ok = 0;
		}
	}
	if (nfiles < 2 || !ok) return 0;
	/* each file starts after the whole sectors of the ones before */
	for (i = 0; i < nfiles; i++) {
		FILELEN size;
		fh = file_open_rb(names[i]);
		if (fh == FILEH_INVALID) return 0;
		size = file_getsize(fh);
		file_close(fh);
		if (size % 2352) return 0;
		parts[i] = names[i];
		base[i] = i ? base[i - 1] + sectors : 0;
		sectors = (UINT64)size / 2352;
	}
	/* the joined file, and a sheet that names it */
	milstr_ncpy(bin, cue, sizeof bin);
	file_cutext(bin);
	milstr_ncat(bin, ".joined.bin", sizeof bin);
	used = (size_t)snprintf(sheet, sizeof sheet, "FILE \"%s\" BINARY\n", file_getname(bin));
	i = -1;
	for (at = start; at < len && used < sizeof sheet - 64; ) {
		unsigned m, sec, f;
		argc = cue_line(text, len, &at, line, sizeof line, argv);
		if (argc >= 3 && !milstr_cmp(argv[0], "FILE")) i++;
		else if (argc >= 3 && !milstr_cmp(argv[0], "TRACK")) {
			for (q = argv[2]; *q; q++) *q = (char)toupper((unsigned char)*q);	/* "mode1/2352" */
			used += (size_t)snprintf(sheet + used, sizeof sheet - used, "TRACK %s %s\n", argv[1], argv[2]);
		}
		else if (argc >= 2 && !milstr_cmp(argv[0], "PREGAP"))
			used += (size_t)snprintf(sheet + used, sizeof sheet - used, "PREGAP %s\n", argv[1]);
		else if (argc >= 3 && !milstr_cmp(argv[0], "INDEX") && i >= 0 && i < nfiles &&
		         sscanf(argv[2], "%u:%u:%u", &m, &sec, &f) == 3) {
			UINT64 pos = base[i] + ((UINT64)m * 60 + sec) * 75 + f;
			used += (size_t)snprintf(sheet + used, sizeof sheet - used, "INDEX %02d %02u:%02u:%02u\n",
				(int)milstr_solveINT(argv[1]), (unsigned)(pos / 4500), (unsigned)(pos / 75 % 60), (unsigned)(pos % 75));
		}
	}
	if (!chimera_vfs_join(bin, parts, nfiles)) return 0;
	milstr_ncpy(out, cue, outsize);
	file_cutext(out);
	milstr_ncat(out, ".joined.cue", outsize);
	return chimera_vfs_text(out, sheet);
}

static void attach_cdrom(void);

static void attach_removable(void)
{
	int i;
	int n = wbx_slot_count("floppy");
	g_floppies = (char (*)[MAX_PATH])calloc((size_t)(n > 0 ? n : 1), MAX_PATH);
	g_floppyCount = g_floppies ? slot_names("floppy", g_floppies, n > 0 ? n : 1) : 0;
	for (i = 0; i < g_floppyCount && i < 2; i++) {
		/* a drive left empty by Floppies at Start has its selector on the
		 * disk it would have had, for its Swap to put in */
		g_pendingFloppy[i] = i;
		if (i >= g_floppiesAtStart) continue;
		diskdrv_setfdd((REG8)i, g_floppies[i], 0);
		g_insertedFloppy[i] = i;
	}
	attach_cdrom();
}

/* the project's CD in the IDE CD-ROM drive; pccore_reset takes it out, and
 * the Reset input puts it back the same way (the joined sheet is made once:
 * the file layer keeps it, under the name the first call chose) */
static char g_cdPath[MAX_PATH];

static void attach_cdrom(void)
{
#if defined(SUPPORT_IDEIO)
	int i;
	if (!g_cdPath[0]) {
		char cd[1][MAX_PATH], joined[MAX_PATH];
		if (!slot_names("cdrom", cd, 1)) return;
		if (join_cue(cd[0], joined, sizeof joined))
			milstr_ncpy(cd[0], joined, MAX_PATH);
		milstr_ncpy(g_cdPath, cd[0], MAX_PATH);
	}
	for (i = 0; i < 4; i++)
		if (np2cfg.idetype[i] == SXSIDEV_CDROM) {
			sxsi_devopen((REG8)i, g_cdPath);
			g_cdUnit = i;
			break;
		}
#endif	/* the 286 machine has no CD-ROM drive */
}

/* ------------------------------------------------------------ the ABI */

ECL_EXPORT const char *GetLoadError(void) { return g_loadError; }

ECL_EXPORT int Init(void)
{
	g_loadError[0] = 0;

	g_frame = (uint32_t *)calloc((size_t)NP2DRV_VIDEO_MAX_W * NP2DRV_VIDEO_MAX_H, 4);
	g_audio = (int16_t *)calloc(NP2DRV_AUDIO_MAX_PAIRS * 2, sizeof *g_audio);
	if (!g_frame || !g_audio) {
		snprintf(g_loadError, sizeof g_loadError, "no room for the frame or sound buffers");
		return 0;
	}

	g_saveWindow = (uint8_t *)alloc_invisible(SAVEDATA_WINDOW);
	if (!g_saveWindow) {
		snprintf(g_loadError, sizeof g_loadError, "no room for the save-data window");
		return 0;
	}

	configure();
	attach_hard_disks();

	/* np2_main's order (sdl/np2.c), less the window, menus and host input */
	fontmng_init();
	keystat_initialize();
	mousemng_initialize();
	scrnmng_initialize();
	if (scrnmng_create(0) != SUCCESS) {
		snprintf(g_loadError, sizeof g_loadError, "the screen could not be made");
		return 0;
	}
	soundmng_initialize();
	commng_initialize();
	sysmng_initialize();
	taskmng_initialize();
	pccore_init();
#if defined(SUPPORT_WAB)
	np2wab_init();
#endif
#if defined(SUPPORT_CL_GD5430)
	pc98_cirrus_vga_init();
#endif
	scrndraw_redraw();
	pccore_reset();
	frame_rate();	/* pccore_reset sets pccore.baseclock */
	attach_removable();
	list_drives();
	g_lastClock = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
	return 1;
}

/* ------------------------------------------------------------ input
 *
 * Chimera sends levels: SetButton and SetAxis before each FrameAdvance say
 * what is held and how far the mouse moved. The driver compares with the
 * last frame and tells NP2kai what changed: a key down or up, a mouse
 * button, the joystick byte, a disk change. Both the levels and the last
 * frame's levels are machine state (ordinary globals), so savestates and
 * movies carry them. */
#define X(name, header, what) what,
static const int k_buttons[] = { NP2_BUTTONS(X) };
#undef X
#define BUTTON_COUNT ((int)NELEMENTS(k_buttons))

static uint8_t g_level[BUTTON_COUNT];
static uint8_t g_last[BUTTON_COUNT];
static int32_t g_axis[NP2_AXIS_COUNT];

/* the engine's input-poll callback: a slot address, the same in every
 * session, so it is invisible to savestates (porting-a-core.md) */
static ECL_INVISIBLE uint64_t g_pollCallback;

ECL_EXPORT void SetInputPollCallback(uint64_t fn) { g_pollCallback = fn; }

void chimera_input_read(void)
{
	g_inputRead = 1;
	if (g_pollCallback)
		((uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))
		 (uintptr_t)g_pollCallback)(0, 0, 0, 0, 0, 0);
}

ECL_EXPORT void SetButton(int32_t index, int32_t state)
{
	if (index >= 0 && index < BUTTON_COUNT)
		g_level[index] = state ? 1 : 0;
}

ECL_EXPORT void SetAxis(int32_t index, int32_t value)
{
	if (index >= 0 && index < NP2_AXIS_COUNT)
		g_axis[index] = value;
}

static int rose(int i) { return g_level[i] && !g_last[i]; }

/* SHIFT (0x70) and Right SHIFT (0x7d). NP2kai sends Right SHIFT as SHIFT
 * unless the machine asked the PC-9801-119 keyboard to tell them apart
 * (keystat.c, keyctrl.kbdtype 2 or more), and lets either key's release
 * end the one SHIFT. So while they are one key, they are sent as one: down
 * when the first goes down, up when the last comes up. */
#define KEY_SHIFT 0x70
#define KEY_RSHIFT 0x7d

static int shifts_merged(void)
{
#if defined(SUPPORT_PC9801_119)
	return keyctrl.kbdtype < 2;
#else
	return 1;
#endif
}

static void shift_levels(int *now, int *last)
{
	int i;
	*now = *last = 0;
	for (i = 0; i < BUTTON_COUNT; i++)
		if (k_buttons[i] == KEY_SHIFT || k_buttons[i] == KEY_RSHIFT) {
			*now |= g_level[i];
			*last |= g_last[i];
		}
}

/* A drive is in use when the project has a disk for it: drive 2 needs a
 * second floppy (list_drives offers it only then). */
static int floppy_drive_used(int drive) { return drive < g_floppyCount; }

static void floppy_step(int drive, int delta)
{
	if (!floppy_drive_used(drive)) return;
	g_pendingFloppy[drive] = (g_pendingFloppy[drive] + delta + g_floppyCount) % g_floppyCount;
}

/* Swap puts the selected disk in. When that disk is in the other drive, the
 * two drives trade disks: one image in two drives would be two NP2kai
 * drives, each with its own track cache, writing one disk. diskdrv_setfdd
 * takes the old disk out at once and opens the new one only after its
 * delay, so neither image is ever open in both drives. */
static void floppy_swap(int drive)
{
	int other = 1 - drive, disk = g_pendingFloppy[drive], cur = g_insertedFloppy[drive];
	if (!floppy_drive_used(drive)) return;
	g_insertedFloppy[drive] = disk;
	diskdrv_setfdd((REG8)drive, g_floppies[disk], 0);
	if (disk == g_insertedFloppy[other]) {
		g_pendingFloppy[other] = g_insertedFloppy[other] = cur;
		diskdrv_setfdd((REG8)other, cur >= 0 ? g_floppies[cur] : NULL, 0);
	}
}

/* Eject takes the disk out and leaves the drive empty; the selector stays
 * where it is, so Swap puts back the disk it is on. */
static void floppy_eject(int drive)
{
	if (!floppy_drive_used(drive) || g_insertedFloppy[drive] < 0) return;
	g_insertedFloppy[drive] = -1;
	diskdrv_setfdd((REG8)drive, NULL, 0);
}

/* The PC's reset switch: the machine starts again, the disks stay in, with
 * everything written to them (the file layer keeps the writes, so the hard
 * disks NP2kai opens again read them back). Reboot Core would start from
 * the project's files instead. */
static void machine_reset(void)
{
	pccore_reset();
	frame_rate();
	attach_cdrom();
}

static void apply_input(void)
{
	int i;
	UINT8 joy = 0xff;
	int mleft = 0, mright = 0;

	for (i = 0; i < BUTTON_COUNT; i++) {
		int what = k_buttons[i];
		if (what < 0x80) {
			if ((what == KEY_SHIFT || what == KEY_RSHIFT) && shifts_merged()) {
				int now, last;
				shift_levels(&now, &last);
				if (what == KEY_SHIFT && now != last) {
					if (now) keystat_keydown(KEY_SHIFT);
					else keystat_keyup(KEY_SHIFT);
				}
				continue;
			}
			if (g_level[i] != g_last[i]) {
				if (g_level[i]) keystat_keydown((REG8)what);
				else keystat_keyup((REG8)what);
			}
			continue;
		}
		switch (what) {
		case NP2BTN_MOUSE_LEFT:  mleft = g_level[i]; break;
		case NP2BTN_MOUSE_RIGHT: mright = g_level[i]; break;
		/* active low, as fmboard_getjoy reads it: the directions in bits
		 * 0-3, the two buttons in bits 6 and 7 */
		case NP2BTN_JOY1_UP:    if (g_level[i]) joy &= ~0x01; break;
		case NP2BTN_JOY1_DOWN:  if (g_level[i]) joy &= ~0x02; break;
		case NP2BTN_JOY1_LEFT:  if (g_level[i]) joy &= ~0x04; break;
		case NP2BTN_JOY1_RIGHT: if (g_level[i]) joy &= ~0x08; break;
		case NP2BTN_JOY1_A:     if (g_level[i]) joy &= ~0x40; break;
		case NP2BTN_JOY1_B:     if (g_level[i]) joy &= ~0x80; break;
		/* Previous and Next move a selector; only Swap puts the selected
		 * disk in, as the DOSBox-X core's buttons do */
		case NP2BTN_FDD1_PREV: if (rose(i)) floppy_step(0, -1); break;
		case NP2BTN_FDD1_NEXT: if (rose(i)) floppy_step(0, +1); break;
		case NP2BTN_FDD2_PREV: if (rose(i)) floppy_step(1, -1); break;
		case NP2BTN_FDD2_NEXT: if (rose(i)) floppy_step(1, +1); break;
		case NP2BTN_FDD1_SWAP: if (rose(i)) floppy_swap(0); break;
		case NP2BTN_FDD2_SWAP: if (rose(i)) floppy_swap(1); break;
		case NP2BTN_RESET:     if (rose(i)) machine_reset(); break;
		case NP2BTN_FDD1_EJECT: if (rose(i)) floppy_eject(0); break;
		case NP2BTN_FDD2_EJECT: if (rose(i)) floppy_eject(1); break;
		}
	}
	chimera_mouse_buttons(mleft, mright);
	chimera_mouse_move(g_axis[NP2_AXIS_MOUSE_X], g_axis[NP2_AXIS_MOUSE_Y]);
	chimera_joy_set(joy);
	memcpy(g_last, g_level, sizeof g_last);
	/* relative axes are spent once applied; the next frame says again */
	g_axis[NP2_AXIS_MOUSE_X] = g_axis[NP2_AXIS_MOUSE_Y] = 0;
}

static void copy_frame(void)
{
	int w, h, i;
	const uint32_t *src = chimera_screen(&w, &h);
	if (w > NP2DRV_VIDEO_MAX_W) w = NP2DRV_VIDEO_MAX_W;
	if (h > NP2DRV_VIDEO_MAX_H) h = NP2DRV_VIDEO_MAX_H;
	g_width = w;
	g_height = h;
	/* NP2kai's RGB32 is B, G, R, unused in memory: BGRA once alpha is set */
	for (i = 0; i < w * h; i++)
		g_frame[i] = src[i] | 0xff000000u;
}

ECL_EXPORT void FrameAdvance(uint64_t packed)
{
	/* More than 64 buttons, so every one comes through SetButton (the
	 * wide-input channel) and the packed word is not used. */
	(void)packed;
	g_inputRead = 0;
	memset(g_accessed, 0, sizeof g_accessed);
	apply_input();
	pccore_exec(TRUE);
	chimera_gettick();
	memcpy(g_lit, g_accessed, sizeof g_lit);
	g_audioPairs = chimera_sound_take(g_audio, NP2DRV_AUDIO_MAX_PAIRS);
	if (g_render) copy_frame();
	g_frames++;
}

/* Turbo: skip only what is pure output, the copy into the BGRA buffer.
 * NP2kai still draws into its own surface every frame, because what it
 * draws decides what it draws next (the lines it marks done), so the
 * machine is the same whether a frame was shown or not. */
ECL_EXPORT void SetRenderingEnabled(int32_t on) { g_render = on != 0; }

ECL_EXPORT int32_t GetVsyncNumerator(void) { return g_vsyncNum; }
ECL_EXPORT int32_t GetVsyncDenominator(void) { return g_vsyncDen; }

ECL_EXPORT uint32_t *GetVideoBgra(void) { return g_frame; }
ECL_EXPORT int GetVideoWidth(void) { return g_width; }
ECL_EXPORT int GetVideoHeight(void) { return g_height; }

ECL_EXPORT int16_t *GetAudio(void) { return g_audio; }
ECL_EXPORT int GetAudioSampleCount(void) { return g_audioPairs; }

ECL_EXPORT int InputWasRead(void) { return g_inputRead; }

/* Memory domains. Main Memory first, as tools open on the first: NP2kai's
 * mem[], the 2 MB the machine is built on - the first megabyte as the CPU
 * sees it (with graphics bank 0 and the BIOS), graphics bank 1 and the
 * font above it. Then the extended memory above 1 MB, and the parts of
 * mem[] a person looks for by name, as views of the same bytes: the text
 * screen, each graphics bank's B, R, G planes and its E plane, and the
 * font. Every one is the machine's own memory, so a write is a poke. */
static const struct {
	const char *name;
	uint32_t offset, size;	/* in mem[]; size 0: the extended memory */
} k_domains[] = {
	{ "Main Memory", 0, sizeof mem },
	{ "Extended Memory", 0, 0 },
	{ "Text VRAM", 0xa0000, 0x4000 },
	{ "Graphics VRAM 0", VRAM0_B, 0x18000 },
	{ "Graphics VRAM 0 E", VRAM0_E, 0x8000 },
	{ "Graphics VRAM 1", VRAM1_B, 0x18000 },
	{ "Graphics VRAM 1 E", VRAM1_E, 0x8000 },
	{ "Font", FONT_ADRS, FONTMEMORYSIZE },
};
#define DOMAIN_COUNT ((int)NELEMENTS(k_domains))

ECL_EXPORT int GetMemoryDomainCount(void) { return DOMAIN_COUNT; }

ECL_EXPORT const char *GetMemoryDomainName(int i)
{
	return i >= 0 && i < DOMAIN_COUNT ? k_domains[i].name : NULL;
}

ECL_EXPORT uint8_t *GetMemoryDomainPtr(int i)
{
	if (i < 0 || i >= DOMAIN_COUNT) return NULL;
	return k_domains[i].size ? mem + k_domains[i].offset : CPU_EXTMEM;
}

ECL_EXPORT int64_t GetMemoryDomainSize(int i)
{
	if (i < 0 || i >= DOMAIN_COUNT) return 0;
	if (k_domains[i].size) return k_domains[i].size;
	return CPU_EXTMEM ? (int64_t)CPU_EXTMEMSIZE : 0;
}

ECL_EXPORT int GetMemoryDomainWritable(int i) { return i >= 0 && i < DOMAIN_COUNT; }

/* With no extended memory (Extended Memory 0 MB) its domain has no bytes:
 * kept in the list, so every domain keeps its number, but left out of the
 * memory tools. */
ECL_EXPORT int32_t GetMemoryDomainHidden(int32_t i)
{
	return GetMemoryDomainSize(i) == 0;
}

/* ------------------------------------------------------------ drives
 *
 * One entry for each medium the project put in the machine: the floppy
 * drives that hold a disk, each hard disk, the CD-ROM drive. A light is on
 * for a frame in which the machine read or wrote that drive: NP2kai's own
 * access notices (port/sysmng.h) mark it during the frame, and the frame's
 * end latches it. The floppy drives also say what they hold: every floppy
 * of the project can go into either (not into both at once), Previous and
 * Next move the selector and Swap puts the selected one in (apply_input). All of it is guest
 * memory, so a rewind takes it back. */
enum { DRIVE_FDD, DRIVE_SXSI };
#define DRIVE_MAX 8
static struct { const char *name; int kind, unit; } g_drives[DRIVE_MAX];
static int g_driveCount;

void chimera_drive_access(int kind, int unit)
{
	if (kind >= 0 && kind < 2 && unit >= 0 && unit < 4)
		g_accessed[kind][unit] = 1;
}

static void add_drive(const char *name, int kind, int unit)
{
	if (g_driveCount < DRIVE_MAX) {
		g_drives[g_driveCount].name = name;
		g_drives[g_driveCount].kind = kind;
		g_drives[g_driveCount].unit = unit;
		g_driveCount++;
	}
}

static void list_drives(void)
{
	static const char *const hdd[] = { "Hard Disk 1", "Hard Disk 2" };
	int i;
	g_driveCount = 0;
	if (g_floppyCount > 0) add_drive("Floppy 1", DRIVE_FDD, 0);
	if (g_floppyCount > 1) add_drive("Floppy 2", DRIVE_FDD, 1);
	for (i = 0; i < g_hddCount; i++) add_drive(hdd[i], DRIVE_SXSI, g_hddUnit[i]);
	if (g_cdUnit >= 0) add_drive("CD-ROM", DRIVE_SXSI, g_cdUnit);
}

ECL_EXPORT int32_t GetDriveCount(void) { return g_driveCount; }

ECL_EXPORT const char *GetDriveName(int32_t i)
{
	return i >= 0 && i < g_driveCount ? g_drives[i].name : NULL;
}

ECL_EXPORT int32_t GetDriveLight(int32_t i)
{
	return i >= 0 && i < g_driveCount && g_lit[g_drives[i].kind][g_drives[i].unit];
}

static int is_floppy_drive(int32_t i) { return i >= 0 && i < g_driveCount && g_drives[i].kind == DRIVE_FDD; }

ECL_EXPORT int32_t GetDriveMediaCount(int32_t i) { return is_floppy_drive(i) ? g_floppyCount : 0; }

ECL_EXPORT const char *GetDriveMediaName(int32_t i, int32_t n)
{
	return is_floppy_drive(i) && n >= 0 && n < g_floppyCount ? g_floppies[n] : NULL;
}

ECL_EXPORT int32_t GetDriveMediaSelected(int32_t i)
{
	return is_floppy_drive(i) ? g_pendingFloppy[g_drives[i].unit] : 0;
}

ECL_EXPORT int32_t GetDriveMediaInserted(int32_t i)
{
	return is_floppy_drive(i) ? g_insertedFloppy[g_drives[i].unit] : -1;
}

/* ------------------------------------------------------------ registers
 *
 * The CPU's registers as the debugger shows them, read only. The flags are
 * the whole register: both cores keep the overflow flag apart. */
#if defined(CPUCORE_IA32)
static const char *const k_regNames[] = {
	"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI", "EIP", "EFLAGS",
	"CS", "SS", "DS", "ES", "FS", "GS", "CR0", "CR3" };
#else
static const char *const k_regNames[] = {
	"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "IP", "FLAGS",
	"CS", "SS", "DS", "ES" };
#endif
#define REG_COUNT ((int32_t)NELEMENTS(k_regNames))

ECL_EXPORT int32_t GetRegisterCount(void) { return REG_COUNT; }

ECL_EXPORT const char *GetRegisterName(int32_t i)
{
	return i >= 0 && i < REG_COUNT ? k_regNames[i] : NULL;
}

ECL_EXPORT int32_t GetRegisterBits(int32_t i)
{
#if defined(CPUCORE_IA32)
	return i < 10 || i >= 16 ? 32 : 16;
#else
	(void)i;
	return 16;
#endif
}

ECL_EXPORT int64_t GetRegisterValue(int32_t i)
{
	switch (i) {
#if defined(CPUCORE_IA32)
	case 0: return CPU_EAX;  case 1: return CPU_ECX;  case 2: return CPU_EDX;  case 3: return CPU_EBX;
	case 4: return CPU_ESP;  case 5: return CPU_EBP;  case 6: return CPU_ESI;  case 7: return CPU_EDI;
	case 8: return CPU_EIP;  case 9: return REAL_EFLAGREG;
	case 10: return CPU_CS;  case 11: return CPU_SS;  case 12: return CPU_DS;  case 13: return CPU_ES;
	case 14: return CPU_FS;  case 15: return CPU_GS;  case 16: return CPU_CR0; case 17: return CPU_CR3;
#else
	case 0: return CPU_AX;   case 1: return CPU_CX;   case 2: return CPU_DX;   case 3: return CPU_BX;
	case 4: return CPU_SP;   case 5: return CPU_BP;   case 6: return CPU_SI;   case 7: return CPU_DI;
	case 8: return CPU_IP;
	case 9: return (CPU_FLAG & 0xf7ff) | (i286core.s.ovflag ? 0x0800 : 0);	/* i286c.mcr REAL_FLAGREG */
	case 10: return CPU_CS;  case 11: return CPU_SS;  case 12: return CPU_DS;  case 13: return CPU_ES;
#endif
	default: return 0;
	}
}

/* -------------------------------------------- save data (docs/save-data.md)
 *
 * What a PC-98 keeps is its disks: every floppy or hard disk the machine has
 * written to leaves under the name it was mounted with, the whole image, so
 * it goes back into the project as a disk. The bytes are a read-only seed
 * under a sparse overlay (port/dosio.c), so there is no address where a
 * whole disk exists, and they go out through the window, a piece at a time.
 * The window is ECL_INVISIBLE memory: a view the host reads between frames,
 * not machine state. NP2kai's own scratch files (font.tmp) are not disks and
 * are left out. */
static int g_saveList[256];
static int g_saveCount;

ECL_EXPORT int32_t GetSaveDataFileCount(void)
{
	int i, n = chimera_vfs_count();
	g_saveCount = 0;
	for (i = 0; i < n && g_saveCount < (int)NELEMENTS(g_saveList); i++) {
		const char *name = chimera_vfs_name(i);
		if (!chimera_vfs_written(i) || !name || !milstr_cmp(file_getname(name), "font.tmp"))
			continue;
		g_saveList[g_saveCount++] = i;
	}
	return g_saveCount;
}

ECL_EXPORT const char *GetSaveDataFileName(int32_t index)
{
	if (index < 0 || index >= g_saveCount) return NULL;
	return file_getname(chimera_vfs_name(g_saveList[index]));
}

ECL_EXPORT int64_t GetSaveDataFileSize(int32_t index)
{
	if (index < 0 || index >= g_saveCount) return 0;
	return chimera_vfs_size(g_saveList[index]);
}

ECL_EXPORT const uint8_t *GetSaveDataFileBuffer(int32_t index)
{
	(void)index;
	return NULL;	/* use the window */
}

ECL_EXPORT const uint8_t *GetSaveDataScratch(void) { return g_saveWindow; }

ECL_EXPORT int64_t ReadSaveDataFile(int32_t index, int64_t offset, int64_t len)
{
	if (!g_saveWindow || index < 0 || index >= g_saveCount || len <= 0) return 0;
	if (len > SAVEDATA_WINDOW) len = SAVEDATA_WINDOW;
	return chimera_vfs_read(g_saveList[index], offset, g_saveWindow, len);
}

/* A digest of the visible picture, one a frame, for the gate. */
ECL_EXPORT uint64_t GetFrameDigest(void)
{
	uint64_t h = 1469598103934665603ULL;
	int i;
	for (i = 0; i < g_width * g_height; i++) { h ^= g_frame[i]; h *= 1099511628211ULL; }
	h ^= (uint64_t)g_width * 65537u + (uint64_t)g_height;
	h *= 1099511628211ULL;
	return h;
}

/* And of the frame's sound, so the gate compares what is heard too. */
ECL_EXPORT uint64_t GetAudioDigest(void)
{
	uint64_t h = 1469598103934665603ULL;
	int i;
	for (i = 0; i < g_audioPairs * 2; i++) { h ^= (uint16_t)g_audio[i]; h *= 1099511628211ULL; }
	return h;
}

ECL_EXPORT uint64_t GetEmulatedClocks(void) { return np2drv_clocks(); }
ECL_EXPORT uint64_t GetFrameCount(void) { return g_frames; }
