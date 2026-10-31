/* chd-cd.h - a CHD CD image as a cue sheet NP2kai reads (chd-cd.c). */
#ifndef CHD_CD_H
#define CHD_CD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* For a .chd at path: makes its raw image (path + ".bin", read from the CHD
 * as NP2kai reads it) and a cue sheet naming it (path + ".cue") in the file
 * layer, and puts the sheet's path in out. 1 when made, 0 when path is not
 * a .chd or not a CD image this core reads. */
int chimera_chd_cue(const char *path, char *out, size_t outsize);

#ifdef __cplusplus
}
#endif

#endif
