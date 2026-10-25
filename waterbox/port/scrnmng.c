/* scrnmng.c - NP2kai's screen manager, for the Chimera core.
 *
 * NP2kai draws each frame into a surface it locks from here (scrndraw.c),
 * and the PC-9821's graphics cards (WAB, Cirrus) hand over a finished picture
 * through scrnmng_bltwab. Either way the picture ends up in one buffer the
 * driver reads after the frame. It is always 32 bits a pixel: the 16-bit path
 * the libretro port also has would lose colour depth for nothing.
 *
 * The surface is sized to the largest picture NP2kai can make, once, so a
 * change of mode never reallocates and never moves the buffer.
 */
#include <compiler.h>
#include <string.h>
#include <scrnmng.h>
#include <pccore.h>
#if defined(SUPPORT_WAB)
#include <wab/wab.h>
#endif
#include "chimera-port.h"

/* WAB_MAX_WIDTH x WAB_MAX_HEIGHT from wab.h, and room for every PC-98 mode */
#define SURF_MAX_W 1600
#define SURF_MAX_H 1200

SCRNMNG scrnmng;
static SCRNSURF scrnsurf;
static UINT32 s_surface[SURF_MAX_W * SURF_MAX_H];

void scrnmng_initialize(void)
{
	scrnmng.width = 640;
	scrnmng.height = 400;
}

BRESULT scrnmng_create(UINT8 mode)
{
	(void)mode;
	scrnmng.bpp = 32;
	scrnmng.pc98surf = s_surface;
	scrnmng.dispsurf = s_surface;
	scrnmng.enable = TRUE;
	return SUCCESS;
}

void scrnmng_destroy(void)
{
	scrnmng.enable = FALSE;
}

void scrnmng_getsize(int *pw, int *ph)
{
	*pw = scrnmng.width;
	*ph = scrnmng.height;
}

static int clampw(int w) { return w < 1 ? 1 : (w > SURF_MAX_W ? SURF_MAX_W : w); }
static int clamph(int h) { return h < 1 ? 1 : (h > SURF_MAX_H ? SURF_MAX_H : h); }

void scrnmng_setwidth(int posx, int width)
{
	(void)posx;
	scrnmng.width = clampw(width);
}

void scrnmng_setheight(int posy, int height)
{
	(void)posy;
	scrnmng.height = clamph(height);
}

/* wab.c calls this to take the screen over at the card's own size. */
void scrnmng_setsize(int posx, int posy, int width, int height)
{
	(void)posx; (void)posy;
	scrnmng.width = clampw(width);
	scrnmng.height = clamph(height);
}

const SCRNSURF *scrnmng_surflock(void)
{
	if (!scrnmng.enable) return NULL;
	scrnsurf.ptr = (UINT8 *)s_surface;
	scrnsurf.width = scrnmng.width;
	scrnsurf.height = scrnmng.height;
	scrnsurf.bpp = 32;
	scrnsurf.xalign = 4;
	scrnsurf.yalign = scrnmng.width * 4;
	scrnsurf.extend = 0;
	return &scrnsurf;
}

void scrnmng_surfunlock(const SCRNSURF *surf) { (void)surf; }

RGB16 scrnmng_makepal16(RGB32 pal32)
{
	RGB16 ret = (RGB16)((pal32.p.r & 0xf8) << 8);
	ret += (RGB16)((pal32.p.g & 0xfc) << 3);
	ret += (RGB16)(pal32.p.b >> 3);
	return ret;
}

void scrnmng_update(void) {}
void scrnmng_updatecursor(void) {}
void scrnmng_updatefsres(void) {}
void scrnmng_blthdc(void) {}

void scrnmng_bltwab(void)
{
#if defined(SUPPORT_WAB)
	/* np2wabwnd.pBuffer is WAB_MAX_WIDTH wide whatever the mode, the same
	 * 0x00RRGGBB words, laid out at the current width (wab.c). */
	if (np2wabwnd.pBuffer)
		memcpy(s_surface, np2wabwnd.pBuffer,
		       (size_t)scrnmng.width * (size_t)scrnmng.height * 4);
#endif
}

BRESULT scrnmng_entermenu(SCRNMENU *smenu) { (void)smenu; return FAILURE; }
void scrnmng_leavemenu(void) {}
void scrnmng_menudraw(const RECT_T *rct) { (void)rct; }

const uint32_t *chimera_screen(int *width, int *height)
{
	*width = scrnmng.width;
	*height = scrnmng.height;
	return s_surface;
}
