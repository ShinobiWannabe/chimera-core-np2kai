/* np2-driver.h - the driver's limits and the calls run-native.c makes. */
#ifndef NP2_DRIVER_H
#define NP2_DRIVER_H

#include <stdint.h>

/* The largest picture: WAB_MAX_WIDTH x WAB_MAX_HEIGHT (wab/wab.h). */
#define NP2DRV_VIDEO_MAX_W 1600
#define NP2DRV_VIDEO_MAX_H 1200
/* Sound pairs one frame can hold: half a second at 44100 Hz. */
#define NP2DRV_AUDIO_MAX_PAIRS 22050

const char *GetLoadError(void);
int Init(void);
void FrameAdvance(uint64_t packed);
int32_t GetVsyncNumerator(void);
int32_t GetVsyncDenominator(void);
uint32_t *GetVideoBgra(void);
int GetVideoWidth(void);
int GetVideoHeight(void);
int16_t *GetAudio(void);
int GetAudioSampleCount(void);
int GetMemoryDomainCount(void);
const char *GetMemoryDomainName(int i);
uint8_t *GetMemoryDomainPtr(int i);
int64_t GetMemoryDomainSize(int i);
uint64_t GetFrameDigest(void);
uint64_t GetEmulatedClocks(void);
uint64_t GetAudioDigest(void);
void SetButton(int32_t index, int32_t state);
void SetAxis(int32_t index, int32_t value);
int InputWasRead(void);
int32_t GetSaveDataFileCount(void);
const char *GetSaveDataFileName(int32_t index);
int64_t GetSaveDataFileSize(int32_t index);
const uint8_t *GetSaveDataScratch(void);
int64_t ReadSaveDataFile(int32_t index, int64_t offset, int64_t len);
void SetRenderingEnabled(int32_t on);
int32_t GetDriveCount(void);
int32_t GetDriveLight(int32_t i);
int32_t GetRegisterCount(void);
const char *GetRegisterName(int32_t i);
int64_t GetRegisterValue(int32_t i);

#endif
