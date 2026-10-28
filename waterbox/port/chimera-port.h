/* chimera-port.h - what the platform layer (waterbox/port) offers the driver.
 *
 * NP2kai talks to its platform through the sdl/*.h interfaces. These are the
 * extra pieces the driver needs to run the machine from outside: the frame,
 * the sound, the clock and the files.
 */
#ifndef CHIMERA_PORT_H
#define CHIMERA_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* scrnmng.c: the last picture NP2kai drew, 32-bit 0x00RRGGBB, and its size. */
const uint32_t *chimera_screen(int *width, int *height);

/* sound-chimera.c: every sample the machine has made up to the current
 * emulated clock, as 16-bit stereo pairs into out (room for max pairs).
 * Returns the number of pairs. */
int chimera_sound_take(int16_t *out, int max);

/* timemng.c: the calendar the machine starts from, in seconds since
 * 1970-01-01 00:00:00. Emulated time is added to it. */
void chimera_set_start_time(int64_t seconds);

/* np2-driver.c: emulated milliseconds since power-on (GETTICK). */
unsigned int chimera_gettick(void);

/* np2-driver.c: the same time, whole seconds and the milliseconds past them,
 * without GETTICK's 49.7-day wrap; the calendar reads it. */
void chimera_uptime(int64_t *seconds, unsigned int *ms);

/* mousemng.c: motion and buttons for the next frame. */
void chimera_mouse_move(int dx, int dy);
void chimera_mouse_buttons(int left, int right);

/* np2-driver.c: the machine read the keyboard, mouse or joystick this frame
 * (keyboard-reads.c, mouseif-reads.c, bios18-reads.c, platform.c). */
void chimera_input_read(void);

/* joymng.c: the joystick port's state, active low as the PC-98 reads it. */
void chimera_joy_set(uint8_t stat);

/* dosio.c: the files the machine has opened, for save data. */
int chimera_vfs_count(void);
const char *chimera_vfs_name(int i);
int chimera_vfs_written(int i);
int64_t chimera_vfs_size(int i);
int64_t chimera_vfs_read(int i, int64_t off, void *dst, int64_t len);
int64_t chimera_vfs_blocks_held(int i);
/* the file NP2kai opens as `name` is the host's mount `mount` */
void chimera_vfs_alias(const char *name, const char *mount);
int chimera_vfs_join(const char *path, const char *const *parts, int n);
int chimera_vfs_text(const char *path, const char *text);
/* the mount of the firmware file `path` names, compared in small letters
 * (np2-firmware.h), or NULL when it names none */
const char *chimera_firmware_name(const char *path);

/* rhythm-rom.c: with the setting rhythmSamples on "YM2608 ROM" (np2-driver.c
 * sets chimera_rhythm_rom), the drum WAV file `name` (2608_bd.wav and so on,
 * any case) as decoded from the ROM: 1 and the file in memory, or 0. */
extern int chimera_rhythm_rom;
int chimera_rhythm_image(const char *name, const uint8_t **data, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif /* CHIMERA_PORT_H */
