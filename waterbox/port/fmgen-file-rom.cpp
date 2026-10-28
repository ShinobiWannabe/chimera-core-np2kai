/* fmgen-file-rom.cpp - fmgen's drums from the YM2608's own ROM.
 *
 * fmgen opens its six 2608_*.WAV files with stdio (OPNA::LoadRhythmSample,
 * through FileIO). sound/fmgen/fmgen_file.cpp is compiled here, unchanged,
 * with its fopen sent to ours: with the setting rhythmSamples on "YM2608
 * ROM" a drum's file is the WAV file rhythm-rom.c decoded from the ROM,
 * read from memory. A firmware file is otherwise opened by its mount, whatever
 * the case fmgen asks in (dosio.c chimera_firmware_name), and anything else
 * by its own name.
 */
#include <compiler.h>
/* first, for their <cstdio> takes back any fopen macro */
#include "../../extern/NP2kai/sound/fmgen/fmgen_types.h"
#include "../../extern/NP2kai/sound/fmgen/fmgen_headers.h"
#include "../../extern/NP2kai/sound/fmgen/fmgen_file.h"
#include "chimera-port.h"

static FILE *chimera_fmgen_fopen(const char *path, const char *mode)
{
	const char *fw = chimera_firmware_name(path);	/* any case, as dosio.c */
	const uint8_t *img;
	uint32_t size;
	if (chimera_rhythm_image(path, &img, &size))
		return fmemopen((void *)img, size, "rb");
	return fopen(fw ? fw : path, mode);
}

#define fopen chimera_fmgen_fopen
#include "../../extern/NP2kai/sound/fmgen/fmgen_file.cpp"
#undef fopen
