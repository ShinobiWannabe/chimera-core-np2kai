/* run-wbx.c - drives core.wbx through the miniBox host, as the frontend does.
 *
 * It prints the same line a frame as run-native.c, so the two builds can be
 * compared with diff: frame number, picture digest, picture size, sound
 * pairs, sound digest and the emulated clock.
 *
 * Usage: run-wbx <core.wbx> <folder> <frames> [out.ppm] [mem.bin]
 *
 * NP2_PRESS, NP2_MOUSE and NP2_SAVEDATA as in run-native.c.
 * NP2_NORENDER=a,b and NP2_REGS=1 as in run-native.c, and the same drive
 * lights at the end of each line.
 * NP2_STATE_AT=n saves a state before frame n, runs to the end, loads it and
 * runs from n again: the second run's lines must equal the first's (a
 * line "STATE same" or "STATE DIFFER" at the end says which).
 *
 * Every regular file in the folder is mounted under its own name, which is
 * what the engine does with a project's slots, settings and firmware.
 */
#include "minibox.h"
#include "press-script.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct { FILE *f; } freader;

static intptr_t file_read(uintptr_t ud, uint8_t *d, uintptr_t s)
{
	return (intptr_t)fread(d, 1, s, ((freader *)ud)->f);
}

/* a savestate in memory */
typedef struct { uint8_t *p; size_t len, cap, pos; } membuf;
static int32_t mem_write(uintptr_t ud, const uint8_t *d, uintptr_t s)
{
	membuf *m = (membuf *)ud;
	if (m->len + s > m->cap) {
		size_t cap = m->cap ? m->cap * 2 : (1u << 20);
		while (cap < m->len + s) cap *= 2;
		m->p = (uint8_t *)realloc(m->p, cap);
		if (!m->p) return 0;
		m->cap = cap;
	}
	memcpy(m->p + m->len, d, s);
	m->len += s;
	return 1;
}
static intptr_t mem_read(uintptr_t ud, uint8_t *d, uintptr_t s)
{
	membuf *m = (membuf *)ud;
	size_t n = m->len - m->pos;
	if (n > s) n = s;
	memcpy(d, m->p + m->pos, n);
	m->pos += n;
	return (intptr_t)n;
}

typedef int      (MB_GUEST_ABI *intfn)(void);
typedef void     (MB_GUEST_ABI *framefn)(uint64_t);
typedef uint64_t (MB_GUEST_ABI *u64fn)(void);
typedef const char *(MB_GUEST_ABI *strfn)(void);
typedef uintptr_t (MB_GUEST_ABI *ptrfn)(void);
typedef uintptr_t (MB_GUEST_ABI *ptrifn)(int);
typedef int64_t  (MB_GUEST_ABI *i64ifn)(int);
typedef void     (MB_GUEST_ABI *setfn)(int32_t, int32_t);
typedef const char *(MB_GUEST_ABI *strifn)(int32_t);
typedef int64_t  (MB_GUEST_ABI *readfn)(int32_t, int64_t, int64_t);
typedef void     (MB_GUEST_ABI *set1fn)(int32_t);
typedef int32_t  (MB_GUEST_ABI *i32ifn)(int32_t);
typedef int64_t  (MB_GUEST_ABI *i64i32fn)(int32_t);

/* NP2_POLLS: the engine's input-poll callback, given before Init as
 * Chimera's engine gives it (session.cpp), counting the core's calls */
static long g_polls;
static uintptr_t MB_GUEST_ABI count_poll(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e, uintptr_t f)
{
	(void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
	g_polls++;
	return 0;
}

static uintptr_t proc(mb_host *h, const char *name)
{
	mb_return r;
	wbx_get_proc_addr(h, name, &r);
	if (r.error_message[0] || !r.data) {
		fprintf(stderr, "export %s: %s\n", name, r.error_message[0] ? (const char *)r.error_message : "missing");
		exit(2);
	}
	return r.data;
}

static void die(mb_return *r, const char *what)
{
	if (r->error_message[0]) { fprintf(stderr, "%s: %s\n", what, r->error_message); exit(1); }
}

static double now_s(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	mb_return r;
	mb_host *h;
	FILE *f;
	freader fr;
	DIR *d;
	struct dirent *de;
	long frames, n;
	uint64_t samples = 0;
	double t0, wall;
	/* to be checked against waterbox.config when the package is written */
	mb_memory_layout_template layout = {
		.sbrk_size   = 256u << 20,
		.sealed_size = 16u << 20,
		.invis_size  = 16u << 20,
		.plain_size  = 16u << 20,
		.mmap_size   = (uintptr_t)1024 << 20,
	};

	if (argc < 4) {
		fprintf(stderr, "usage: %s <core.wbx> <folder> <frames> [out.ppm] [mem.bin]\n", argv[0]);
		return 2;
	}
	frames = strtol(argv[3], NULL, 10);
	if (!press_load()) return 2;

	f = fopen(argv[1], "rb");
	if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
	fr.f = f;
	wbx_create_host(&layout, "core.wbx", file_read, (uintptr_t)&fr, &r);
	fclose(f);
	die(&r, "create_host");
	h = (mb_host *)r.data;

	d = opendir(argv[2]);
	if (!d) { fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
	while ((de = readdir(d))) {
		char path[1024];
		struct stat st;
		snprintf(path, sizeof path, "%s/%s", argv[2], de->d_name);
		if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
		/* large images are streamed by path rather than copied in */
		if (st.st_size > (64L << 20)) {
			wbx_mount_file_path(h, de->d_name, path, &r);
		} else {
			freader *mr = malloc(sizeof *mr);
			mr->f = fopen(path, "rb");
			if (!mr->f) continue;
			wbx_mount_file(h, de->d_name, file_read, (uintptr_t)mr, false, &r);
		}
		if (r.error_message[0])
			fprintf(stderr, "mount %s: %s\n", de->d_name, r.error_message);
	}
	closedir(d);

	wbx_activate_host(h, &r); die(&r, "activate");
	if (getenv("NP2_POLLS")) {
		wbx_get_callback_addr(h, count_poll, 3, &r); die(&r, "callback");
		((void (MB_GUEST_ABI *)(uint64_t))proc(h, "SetInputPollCallback"))((uint64_t)r.data);
	}
	if (!((intfn)proc(h, "Init"))()) {
		char why[256];
		const char *msg = ((strfn)proc(h, "GetLoadError"))();
		if (msg && *msg) fprintf(stderr, "Init failed: %s\n", msg);
		wbx_get_death(h, why, sizeof why, &r);
		if (r.data) fprintf(stderr, "the guest died: %s\n", why);
		return 1;
	}
	fprintf(stderr, "vsync %d/%d\n", ((intfn)proc(h, "GetVsyncNumerator"))(), ((intfn)proc(h, "GetVsyncDenominator"))());
	wbx_deactivate_host(h, &r); die(&r, "deactivate");
	wbx_seal(h, &r); die(&r, "seal");
	wbx_activate_host(h, &r); die(&r, "activate2");

	{
		framefn FrameAdvance = (framefn)proc(h, "FrameAdvance");
		u64fn Digest = (u64fn)proc(h, "GetFrameDigest");
		u64fn Clocks = (u64fn)proc(h, "GetEmulatedClocks");
		u64fn AudioDigest = (u64fn)proc(h, "GetAudioDigest");
		intfn W = (intfn)proc(h, "GetVideoWidth");
		intfn H = (intfn)proc(h, "GetVideoHeight");
		intfn Samples = (intfn)proc(h, "GetAudioSampleCount");
		intfn WasRead = (intfn)proc(h, "InputWasRead");
		setfn SetButton = (setfn)proc(h, "SetButton");
		setfn SetAxis = (setfn)proc(h, "SetAxis");

		set1fn SetRendering = (set1fn)proc(h, "SetRenderingEnabled");
		intfn DriveCount = (intfn)proc(h, "GetDriveCount");
		i32ifn DriveLight = (i32ifn)proc(h, "GetDriveLight");
		long offFrom = -1, offTo = -1;
		long stateAt = getenv("NP2_STATE_AT") ? atol(getenv("NP2_STATE_AT")) : -1;
		if (getenv("NP2_NORENDER")) sscanf(getenv("NP2_NORENDER"), "%ld,%ld", &offFrom, &offTo);
		membuf st = {0};
		uint64_t hashA = 1469598103934665603ULL, hashB = 1469598103934665603ULL;
		t0 = now_s();
		for (n = 0; n < frames; n++) {
			char why[256];
			int d, lights = 0;
			if (n == stateAt) { wbx_save_state(h, mem_write, (uintptr_t)&st, &r); die(&r, "save_state"); }
			if (n == offFrom) SetRendering(0);
			if (n == offTo) SetRendering(1);
			PRESS_APPLY(n, SetButton, SetAxis);
			FrameAdvance(0);
			wbx_get_death(h, why, sizeof why, &r);
			if (r.data) { fprintf(stderr, "the guest died at frame %ld: %s\n", n, why); return 3; }
			samples += (uint64_t)Samples();
			for (d = 0; d < DriveCount(); d++) lights |= DriveLight(d) << d;
			printf("%ld %016llx %dx%d %d %016llx %llu %d %x\n", n, (unsigned long long)Digest(),
			       W(), H(), Samples(), (unsigned long long)AudioDigest(),
			       (unsigned long long)Clocks(), WasRead(), lights);
			if (stateAt >= 0 && n >= stateAt) {
				hashA ^= Digest(); hashA *= 1099511628211ULL;
				hashA ^= AudioDigest(); hashA *= 1099511628211ULL;
			}
		}
		if (stateAt >= 0 && st.len) {
			st.pos = 0;
			wbx_load_state(h, mem_read, (uintptr_t)&st, &r); die(&r, "load_state");
			for (n = stateAt; n < frames; n++) {
				PRESS_APPLY(n, SetButton, SetAxis);
				/* NP2_STATE_CONTROL: the negative control, one extra key
				 * after the load, which must make the leg say DIFFER */
				if (getenv("NP2_STATE_CONTROL") && n == stateAt + 10) SetButton(press_button_index("Key RETURN"), 1);
				if (getenv("NP2_STATE_CONTROL") && n == stateAt + 18) SetButton(press_button_index("Key RETURN"), 0);
				FrameAdvance(0);
				hashB ^= Digest(); hashB *= 1099511628211ULL;
				hashB ^= AudioDigest(); hashB *= 1099511628211ULL;
			}
			printf("STATE %s bytes=%zu\n", hashA == hashB ? "same" : "DIFFER", st.len);
		}
		if (getenv("NP2_REGS")) {
			intfn RegCount = (intfn)proc(h, "GetRegisterCount");
			strifn RegName = (strifn)proc(h, "GetRegisterName");
			i64i32fn RegValue = (i64i32fn)proc(h, "GetRegisterValue");
			int k;
			printf("REGS");
			for (k = 0; k < RegCount(); k++) printf(" %s=%llx", RegName(k), (unsigned long long)RegValue(k));
			printf("\n");
		}
		if (getenv("NP2_POLLS")) printf("POLLS %ld\n", g_polls);
		if (getenv("NP2_MEDIA")) {
			/* what each floppy drive holds, and the domains the tools hide */
			i32ifn Inserted = (i32ifn)proc(h, "GetDriveMediaInserted");
			intfn DomCount = (intfn)proc(h, "GetMemoryDomainCount");
			i32ifn Hidden = (i32ifn)proc(h, "GetMemoryDomainHidden");
			int k;
			printf("MEDIA");
			for (k = 0; k < DriveCount(); k++) printf(" %d", Inserted(k));
			printf("\nHIDDEN");
			for (k = 0; k < DomCount(); k++) printf(" %d", Hidden(k));
			printf("\n");
		}
		wall = now_s() - t0;
		fprintf(stderr, "%ld frames, %llu sound pairs, %llu clocks, %.1f frames a second\n",
		        frames, (unsigned long long)samples, (unsigned long long)Clocks(),
		        wall > 0 ? frames / wall : 0.0);

		if (argc > 4) {
			const uint32_t *fb = (const uint32_t *)((ptrfn)proc(h, "GetVideoBgra"))();
			int w = W(), hh = H(), i;
			FILE *o = fopen(argv[4], "wb");
			if (o) {
				fprintf(o, "P6\n%d %d\n255\n", w, hh);
				for (i = 0; i < w * hh; i++) {
					unsigned char rgb[3] = { (unsigned char)(fb[i] >> 16), (unsigned char)(fb[i] >> 8), (unsigned char)fb[i] };
					fwrite(rgb, 1, 3, o);
				}
				fclose(o);
			}
		}
		if (getenv("NP2_SAVEDATA")) {
			intfn Count = (intfn)proc(h, "GetSaveDataFileCount");
			strifn Name = (strifn)proc(h, "GetSaveDataFileName");
			i64ifn Size = (i64ifn)proc(h, "GetSaveDataFileSize");
			readfn Read = (readfn)proc(h, "ReadSaveDataFile");
			ptrfn Scratch = (ptrfn)proc(h, "GetSaveDataScratch");
			int32_t k, nf = (int32_t)Count();
			for (k = 0; k < nf; k++) {
				char path[1024];
				int64_t sz = Size(k), done = 0, got;
				FILE *o;
				snprintf(path, sizeof path, "%s/%s", getenv("NP2_SAVEDATA"), Name(k));
				o = fopen(path, "wb");
				if (!o) { perror(path); return 4; }
				while (done < sz && (got = Read(k, done, sz - done)) > 0) {
					fwrite((const void *)Scratch(), 1, (size_t)got, o);
					done += got;
				}
				fclose(o);
				fprintf(stderr, "savedata %s %lld bytes\n", path, (long long)done);
			}
		}
		if (argc > 5) {
			const uint8_t *m = (const uint8_t *)((ptrifn)proc(h, "GetMemoryDomainPtr"))(0);
			int64_t sz = ((i64ifn)proc(h, "GetMemoryDomainSize"))(0);
			FILE *o = fopen(argv[5], "wb");
			if (o) { fwrite(m, 1, (size_t)sz, o); fclose(o); }
		}
	}

	wbx_deactivate_host(h, &r);
	wbx_destroy_host(h, &r);
	return 0;
}
