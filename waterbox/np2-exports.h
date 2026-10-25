/* np2-exports.h - every function the driver exports, as one table:
 * X(return type, name, (parameters), (arguments)).
 *
 * The package holds both machines in one core.wbx (docs/PLAN.md section
 * 10): each half is the whole emulator and driver built for one CPU, with
 * these names given the half's prefix (np2-half.h) and everything else
 * made local to the half. np2-entry.c exports the real names and passes
 * each call to the half the project's machine setting chose. A new export
 * goes here as well as in np2-driver.c, or the two-machine build cannot
 * reach it.
 */
#define NP2_EXPORTS(X) \
	X(const char *, GetLoadError, (void), ()) \
	X(void, SetInputPollCallback, (uint64_t fn), (fn)) \
	X(void, SetButton, (int32_t index, int32_t state), (index, state)) \
	X(void, SetAxis, (int32_t index, int32_t value), (index, value)) \
	X(void, FrameAdvance, (uint64_t packed), (packed)) \
	X(int32_t, GetVsyncNumerator, (void), ()) \
	X(int32_t, GetVsyncDenominator, (void), ()) \
	X(uint32_t *, GetVideoBgra, (void), ()) \
	X(int, GetVideoWidth, (void), ()) \
	X(int, GetVideoHeight, (void), ()) \
	X(int16_t *, GetAudio, (void), ()) \
	X(int, GetAudioSampleCount, (void), ()) \
	X(int, InputWasRead, (void), ()) \
	X(int, GetMemoryDomainCount, (void), ()) \
	X(const char *, GetMemoryDomainName, (int i), (i)) \
	X(uint8_t *, GetMemoryDomainPtr, (int i), (i)) \
	X(int64_t, GetMemoryDomainSize, (int i), (i)) \
	X(int, GetMemoryDomainWritable, (int i), (i)) \
	X(int32_t, GetSaveDataFileCount, (void), ()) \
	X(const char *, GetSaveDataFileName, (int32_t index), (index)) \
	X(int64_t, GetSaveDataFileSize, (int32_t index), (index)) \
	X(const uint8_t *, GetSaveDataFileBuffer, (int32_t index), (index)) \
	X(const uint8_t *, GetSaveDataScratch, (void), ()) \
	X(int64_t, ReadSaveDataFile, (int32_t index, int64_t offset, int64_t len), (index, offset, len)) \
	X(uint64_t, GetFrameDigest, (void), ()) \
	X(uint64_t, GetAudioDigest, (void), ()) \
	X(uint64_t, GetEmulatedClocks, (void), ()) \
	X(uint64_t, GetFrameCount, (void), ()) \
	X(void, SetRenderingEnabled, (int32_t on), (on)) \
	X(int32_t, GetDriveCount, (void), ()) \
	X(const char *, GetDriveName, (int32_t i), (i)) \
	X(int32_t, GetDriveLight, (int32_t i), (i)) \
	X(int32_t, GetDriveMediaCount, (int32_t i), (i)) \
	X(const char *, GetDriveMediaName, (int32_t i, int32_t n), (i, n)) \
	X(int32_t, GetDriveMediaSelected, (int32_t i), (i)) \
	X(int32_t, GetDriveMediaInserted, (int32_t i), (i)) \
	X(int32_t, GetRegisterCount, (void), ()) \
	X(const char *, GetRegisterName, (int32_t i), (i)) \
	X(int32_t, GetRegisterBits, (int32_t i), (i)) \
	X(int64_t, GetRegisterValue, (int32_t i), (i))
/* Init is not in the table: the entry's own Init picks the half first. */
