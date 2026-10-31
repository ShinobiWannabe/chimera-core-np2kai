/* run-native.c - the native reference harness.
 *
 * Runs the driver exactly as the frontend would, without the sandbox: in a
 * folder holding the project's mounted files (and "settings" and "slots"
 * when the run needs them), for a number of frames, and prints a line a
 * frame with the picture digest, picture size, sound count, sound digest
 * and clock.
 *
 * Usage: run-native <folder> <frames> [out.ppm] [mem.bin]
 *   NP2_PRESS, NP2_MOUSE  input, as press-script.h describes
 *   NP2_SAVEDATA=dir   write the save-data export there after the run
 *   NP2_STALL_FRAME=n  stall the host 300 ms before frame n: the machine must
 *                      not notice (docs/PLAN.md section 3)
 *   NP2_NORENDER=a,b   turbo from frame a to before frame b
 *                      (SetRenderingEnabled(0), then 1 again)
 *   NP2_REGS=1         print the CPU's registers after the run, as REGS
 *   NP2_CFG=1          print what the settings made of the machine after the
 *                      run, as CFG: the switches, chips, volumes and (IA-32)
 *                      the CPU it reports
 *   NP2_WAV=file       write the whole run's sound there, as a 44100 Hz
 *                      16-bit stereo .wav
 *
 * The line ends with the drive lights of the frame, one bit a drive.
 *   out.ppm  the last frame's picture
 *   mem.bin  the main memory domain after the last frame
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "np2-driver.h"
#include "press-script.h"

#include <compiler.h>
#include <pccore.h>
#if defined(CPUCORE_IA32)
#include <cpucore.h>
#endif

/* The guest's allocators, as plain memory. */
void *alloc_invisible(size_t size) { return calloc(1, size); }
void *alloc_sealed(size_t size) { return calloc(1, size); }
void *alloc_plain(size_t size) { return calloc(1, size); }
void _debug_puts(const char *s) { fputs(s, stderr); }

static void write_ppm(const char *path)
{
	FILE *f = fopen(path, "wb");
	const uint32_t *p = GetVideoBgra();
	int w = GetVideoWidth(), h = GetVideoHeight(), i;
	if (!f) { perror(path); return; }
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (i = 0; i < w * h; i++) {
		unsigned char rgb[3] = { (unsigned char)(p[i] >> 16), (unsigned char)(p[i] >> 8), (unsigned char)p[i] };
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
}

int main(int argc, char **argv)
{
	long frames, i, stall, offFrom = -1, offTo = -1;
	FILE *wav = NULL;
	uint64_t samples = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <folder> <frames> [out.ppm] [mem.bin]\n", argv[0]);
		return 2;
	}
	if (chdir(argv[1]) != 0) { perror(argv[1]); return 2; }
	frames = strtol(argv[2], NULL, 0);
	stall = getenv("NP2_STALL_FRAME") ? strtol(getenv("NP2_STALL_FRAME"), NULL, 0) : -1;
	if (getenv("NP2_NORENDER")) sscanf(getenv("NP2_NORENDER"), "%ld,%ld", &offFrom, &offTo);

	if (!press_load()) return 2;
	if (getenv("NP2_WAV") && !(wav = fopen(getenv("NP2_WAV"), "wb"))) { perror(getenv("NP2_WAV")); return 2; }
	if (wav) fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xac\0\0\x10\xb1\x02\0\x04\0\x10\0data\0\0\0\0", 1, 44, wav);
	if (!Init()) {
		fprintf(stderr, "Init failed: %s\n", GetLoadError());
		return 1;
	}
	fprintf(stderr, "vsync %d/%d\n", (int)GetVsyncNumerator(), (int)GetVsyncDenominator());
	for (i = 0; i < frames; i++) {
		int32_t d, lights = 0;
		if (i == stall) usleep(300000);
		if (i == offFrom) SetRenderingEnabled(0);
		if (i == offTo) SetRenderingEnabled(1);
		PRESS_APPLY(i, SetButton, SetAxis);
		FrameAdvance(0);
		samples += (uint64_t)GetAudioSampleCount();
		if (wav) fwrite(GetAudio(), 4, (size_t)GetAudioSampleCount(), wav);
		for (d = 0; d < GetDriveCount(); d++) lights |= GetDriveLight(d) << d;
		printf("%ld %016llx %dx%d %d %016llx %llu %d %x\n", i, (unsigned long long)GetFrameDigest(),
		       GetVideoWidth(), GetVideoHeight(), GetAudioSampleCount(),
		       (unsigned long long)GetAudioDigest(),
		       (unsigned long long)GetEmulatedClocks(), InputWasRead(), lights);
	}
	if (wav) {	/* the two sizes the header left open */
		uint32_t n = (uint32_t)(samples * 4), r = n + 36;
		fseek(wav, 4, SEEK_SET); fwrite(&r, 4, 1, wav);
		fseek(wav, 40, SEEK_SET); fwrite(&n, 4, 1, wav);
		fclose(wav);
	}
	if (getenv("NP2_CFG")) {
		int k;
		printf("CFG dip");
		for (k = 0; k < 3; k++) printf(" %02X", pccore.dipsw[k]);
		printf(" msw");
		for (k = 0; k < 8; k++) printf(" %02X", np2cfg.memsw[k]);
		printf(" gdc %d motor %d vol %d %d %d %d %d %d", np2cfg.uPD72020, np2cfg.MOTOR,
		       np2cfg.vol_master, np2cfg.vol_fm, np2cfg.vol_ssg, np2cfg.vol_adpcm, np2cfg.vol_pcm, np2cfg.vol_rhythm);
#if defined(CPUCORE_IA32)
		printf(" pegcplane %d pci %d %d cpu %s family %u model %u brand %s", np2cfg.usepegcplane, np2cfg.usepci,
		       np2cfg.pci_bios32, i386cpuid.cpu_vendor, (unsigned)i386cpuid.cpu_family, (unsigned)i386cpuid.cpu_model,
		       i386cpuid.cpu_brandstring);
#endif
		printf("\n");
	}
	if (getenv("NP2_REGS")) {
		int32_t r;
		printf("REGS");
		for (r = 0; r < GetRegisterCount(); r++)
			printf(" %s=%llx", GetRegisterName(r), (unsigned long long)GetRegisterValue(r));
		printf("\n");
	}
	fprintf(stderr, "%ld frames, %llu sound pairs, %llu clocks\n", frames,
	        (unsigned long long)samples, (unsigned long long)GetEmulatedClocks());
	if (argc > 3) write_ppm(argv[3]);
	if (getenv("NP2_SAVEDATA")) {
		int32_t k, nf = GetSaveDataFileCount();
		for (k = 0; k < nf; k++) {
			char path[1024];
			int64_t sz = GetSaveDataFileSize(k), done = 0, got;
			FILE *o;
			snprintf(path, sizeof path, "%s/%s", getenv("NP2_SAVEDATA"), GetSaveDataFileName(k));
			o = fopen(path, "wb");
			if (!o) { perror(path); return 4; }
			while (done < sz && (got = ReadSaveDataFile(k, done, sz - done)) > 0) {
				fwrite(GetSaveDataScratch(), 1, (size_t)got, o);
				done += got;
			}
			fclose(o);
			fprintf(stderr, "savedata %s %lld bytes\n", path, (long long)done);
		}
	}
	if (argc > 4) {
		FILE *f = fopen(argv[4], "wb");
		if (f) { fwrite(GetMemoryDomainPtr(0), 1, (size_t)GetMemoryDomainSize(0), f); fclose(f); }
	}
	return 0;
}
