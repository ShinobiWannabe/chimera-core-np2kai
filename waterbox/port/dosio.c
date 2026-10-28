/* dosio.c - NP2kai's file layer, for the Chimera core.
 *
 * NP2kai reads and writes disk images, firmware and its own temporary files
 * through these functions. In the sandbox a project's files are read-only
 * mounts, and nothing the machine does may change the user's files, so every
 * file here is a "node": a read-only seed (the mounted file, opened with
 * stdio) under a sparse overlay of 4 KiB blocks held in memory. A write
 * copies the block it touches into the overlay; a read takes the overlay's
 * block where there is one and the seed's bytes where there is not. The
 * overlay lives in ordinary memory, so savestates carry it, and it is what
 * leaves as save data.
 *
 * A file NP2kai creates (a new floppy, a font cache) is a node with no seed.
 * Nodes are never freed while the core runs: a disk ejected and put back
 * finds its writes where it left them.
 *
 * Both builds use this file. Natively the seeds are ordinary host files, and
 * the host files are never written either.
 */
#include <compiler.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dosio.h>
#include "chimera-port.h"
#include "../np2-firmware.h"

#define BLOCK_SHIFT 12
#define BLOCK_BYTES (1u << BLOCK_SHIFT)

typedef struct vnode {
	struct vnode *next;
	char path[MAX_PATH];
	FILE *seed;
	UINT64 seedsize;	/* bytes of the seed still visible */
	FILE **part;	/* a joined seed: these files end to end (chimera_vfs_join) */
	UINT64 *partend;	/* where each part ends in the node */
	int nparts;
	UINT64 size;
	UINT8 **blocks;
	UINT64 nblocks;	/* entries in blocks[] */
	UINT64 held;	/* blocks allocated */
	int deleted;
	int written;
} VNODE;

struct chimera_file {
	VNODE *node;
	UINT64 pos;
	int writable;
};

static VNODE *s_nodes;
static VNODE **s_order;	/* creation order, for the save-data listing */
static int s_count;

/* The guest's mounts are flat names in the current folder; a path NP2kai
 * builds from a folder and a name comes here with the folder on it. */
static const char *mount_name(const char *path)
{
#if defined(CHIMERA_GUEST)
	return file_getname(path);
#else
	return path;
#endif
}

/* The firmware is mounted under its id, in small letters
 * (np2-firmware.h). The name NP2kai asks for is put in small letters for
 * the comparison, so 2608_BD.WAV, 2608_bd.wav and Font.Rom all find their
 * mount. Any other file (a disk, the slots) is opened by its own name. */
static const char *const s_firmware[] = {
#define X(name) name,
	NP2_FIRMWARE_FILES(X)
#undef X
};

const char *chimera_firmware_name(const char *path)
{
	char low[MAX_PATH];
	const char *name = file_getname(path);
	size_t i;
	for (i = 0; name[i] && i < sizeof low - 1; i++)
		low[i] = (char)tolower((unsigned char)name[i]);
	low[i] = '\0';
	for (i = 0; i < NELEMENTS(s_firmware); i++)
		if (!strcmp(low, s_firmware[i]))
			return s_firmware[i];
	return NULL;
}

static VNODE *node_find(const char *path)
{
	VNODE *n;
	for (n = s_nodes; n; n = n->next)
		if (!strcmp(n->path, path))
			return n;
	return NULL;
}

static VNODE *node_new(const char *path)
{
	VNODE *n = (VNODE *)calloc(1, sizeof *n);
	VNODE **o;
	if (!n) return NULL;
	milstr_ncpy(n->path, path, sizeof n->path);
	o = (VNODE **)realloc(s_order, sizeof *s_order * (size_t)(s_count + 1));
	if (!o) { free(n); return NULL; }
	s_order = o;
	s_order[s_count++] = n;
	n->next = s_nodes;
	s_nodes = n;
	return n;
}

static int node_reserve(VNODE *n, UINT64 size)
{
	UINT64 want = (size + BLOCK_BYTES - 1) >> BLOCK_SHIFT;
	UINT8 **b;
	if (want <= n->nblocks) return 1;
	b = (UINT8 **)realloc(n->blocks, (size_t)want * sizeof *b);
	if (!b) return 0;
	memset(b + n->nblocks, 0, (size_t)(want - n->nblocks) * sizeof *b);
	n->blocks = b;
	n->nblocks = want;
	return 1;
}

/* The seed's bytes in [off, off+len), zeros past its end. */
static void seed_read(VNODE *n, UINT64 off, UINT8 *dst, UINT len)
{
	UINT got = 0;
	if (n->nparts) {
		int p = 0;
		while (got < len && off + got < n->seedsize) {
			UINT64 at = off + got, start, avail;
			UINT want, r = 0;
			while (p < n->nparts && n->partend[p] <= at) p++;
			if (p >= n->nparts) break;
			start = p ? n->partend[p - 1] : 0;
			avail = n->partend[p] - at;
			want = (avail < len - got) ? (UINT)avail : len - got;
			if (fseeko(n->part[p], (off_t)(at - start), SEEK_SET) == 0)
				r = (UINT)fread(dst + got, 1, want, n->part[p]);
			if (r < want) memset(dst + got + r, 0, want - r);
			got += want;
		}
	}
	else if (n->seed && off < n->seedsize) {
		UINT64 avail = n->seedsize - off;
		UINT want = (avail < len) ? (UINT)avail : len;
		if (fseeko(n->seed, (off_t)off, SEEK_SET) == 0)
			got = (UINT)fread(dst, 1, want, n->seed);
	}
	if (got < len)
		memset(dst + got, 0, len - got);
}

static UINT node_read(VNODE *n, UINT64 off, UINT8 *dst, UINT len)
{
	UINT done = 0;
	if (off >= n->size) return 0;
	if (len > n->size - off) len = (UINT)(n->size - off);
	while (done < len) {
		UINT64 bi = (off + done) >> BLOCK_SHIFT;
		UINT bo = (UINT)((off + done) & (BLOCK_BYTES - 1));
		UINT chunk = BLOCK_BYTES - bo;
		if (chunk > len - done) chunk = len - done;
		if (bi < n->nblocks && n->blocks[bi])
			memcpy(dst + done, n->blocks[bi] + bo, chunk);
		else
			seed_read(n, off + done, dst + done, chunk);
		done += chunk;
	}
	return done;
}

static UINT node_write(VNODE *n, UINT64 off, const UINT8 *src, UINT len)
{
	UINT done = 0;
	if (!node_reserve(n, off + len)) return 0;
	while (done < len) {
		UINT64 bi = (off + done) >> BLOCK_SHIFT;
		UINT bo = (UINT)((off + done) & (BLOCK_BYTES - 1));
		UINT chunk = BLOCK_BYTES - bo;
		if (chunk > len - done) chunk = len - done;
		if (!n->blocks[bi]) {
			UINT8 *b = (UINT8 *)malloc(BLOCK_BYTES);
			if (!b) break;
			seed_read(n, bi << BLOCK_SHIFT, b, BLOCK_BYTES);
			n->blocks[bi] = b;
			n->held++;
		}
		memcpy(n->blocks[bi] + bo, src + done, chunk);
		done += chunk;
	}
	if (off + done > n->size) n->size = off + done;
	if (done) n->written = 1;
	return done;
}

static void node_truncate(VNODE *n, UINT64 size)
{
	UINT64 keep = (size + BLOCK_BYTES - 1) >> BLOCK_SHIFT, i;
	for (i = keep; i < n->nblocks; i++) {
		if (n->blocks[i]) { free(n->blocks[i]); n->blocks[i] = NULL; n->held--; }
	}
	/* the bytes past the new end of a kept last block read as zero later */
	if (size < n->size && keep && keep - 1 < n->nblocks && n->blocks[keep - 1]) {
		UINT bo = (UINT)(size & (BLOCK_BYTES - 1));
		if (bo) memset(n->blocks[keep - 1] + bo, 0, BLOCK_BYTES - bo);
	}
	if (size < n->seedsize) n->seedsize = size;
	n->size = size;
	n->written = 1;
}

/* A node for this path: the one already made, or one seeded from the mount. */
/* One file the host mounted under another name: a disk opened with no
 * project is "rom", and NP2kai opens it by its own name, whose extension
 * says what kind of image it is (np2-driver.c bare_slot). */
static char s_aliasName[MAX_PATH], s_aliasMount[MAX_PATH];

void chimera_vfs_alias(const char *name, const char *mount)
{
	milstr_ncpy(s_aliasName, name, sizeof s_aliasName);
	milstr_ncpy(s_aliasMount, mount, sizeof s_aliasMount);
}

static VNODE *node_open(const char *path)
{
	VNODE *n = node_find(path);
	const char *fw;
	FILE *f;
	if (n) return n->deleted ? NULL : n;
	fw = chimera_firmware_name(path);
	f = fopen(fw ? fw : mount_name(path), "rb");
	if (!f && s_aliasName[0] && !strcmp(file_getname(path), s_aliasName))
		f = fopen(s_aliasMount, "rb");
	if (!f) return NULL;
	n = node_new(path);
	if (!n) { fclose(f); return NULL; }
	n->seed = f;
	fseeko(f, 0, SEEK_END);
	n->seedsize = n->size = (UINT64)ftello(f);
	return n;
}

/* A read-only file made of others, end to end: a cue sheet's track files
 * as one image, which is all NP2kai's cue reader takes (np2-driver.c
 * join_cue). Nothing is copied; reads go to the parts. */
int chimera_vfs_join(const char *path, const char *const *parts, int n)
{
	VNODE *v;
	int i;
	UINT64 end = 0;
	if (n <= 0 || node_find(path)) return 0;
	v = node_new(path);
	if (!v) return 0;
	v->part = (FILE **)calloc((size_t)n, sizeof *v->part);
	v->partend = (UINT64 *)calloc((size_t)n, sizeof *v->partend);
	if (!v->part || !v->partend) { v->deleted = 1; return 0; }
	for (i = 0; i < n; i++) {
		FILE *f = fopen(mount_name(parts[i]), "rb");
		if (!f) { v->deleted = 1; return 0; }
		fseeko(f, 0, SEEK_END);
		end += (UINT64)ftello(f);
		v->part[i] = f;
		v->partend[i] = end;
	}
	v->nparts = n;
	v->seedsize = v->size = end;
	return 1;
}

/* A small file the core writes for itself (the joined cue sheet): there
 * from the start, so it is not save data. */
int chimera_vfs_text(const char *path, const char *text)
{
	VNODE *v;
	UINT len = (UINT)strlen(text);
	if (node_find(path)) return 0;
	v = node_new(path);
	if (!v || node_write(v, 0, (const UINT8 *)text, len) != len) return 0;
	v->written = 0;
	return 1;
}

static FILEH handle_new(VNODE *n, int writable)
{
	struct chimera_file *h;
	if (!n) return FILEH_INVALID;
	h = (struct chimera_file *)calloc(1, sizeof *h);
	if (!h) return FILEH_INVALID;
	h->node = n;
	h->writable = writable;
	return h;
}

void dosio_init(void) {}
void dosio_term(void) {}

FILEH file_open(const OEMCHAR *path) { return handle_new(node_open(path), 1); }
FILEH file_open_rb(const OEMCHAR *path) { return handle_new(node_open(path), 0); }
FILEH file_open_rw(const OEMCHAR *path) { return handle_new(node_open(path), 1); }

FILEH file_create(const OEMCHAR *path)
{
	VNODE *n = node_find(path);
	if (!n) n = node_new(path);
	if (!n) return FILEH_INVALID;
	n->deleted = 0;
	n->seed = NULL;	/* a seed handle stays open; it is just not looked at */
	n->nparts = 0;
	n->seedsize = 0;
	node_truncate(n, 0);
	return handle_new(n, 1);
}

FILEPOS file_seek(FILEH h, FILEPOS pointer, int method)
{
	INT64 base = 0;
	if (!h) return -1;
	if (method == FSEEK_CUR) base = (INT64)h->pos;
	else if (method == FSEEK_END) base = (INT64)h->node->size;
	base += pointer;
	if (base < 0) base = 0;
	h->pos = (UINT64)base;
	return (FILEPOS)h->pos;
}

UINT file_read(FILEH h, void *data, UINT length)
{
	UINT r;
	if (!h) return 0;
	r = node_read(h->node, h->pos, (UINT8 *)data, length);
	h->pos += r;
	return r;
}

UINT file_write(FILEH h, const void *data, UINT length)
{
	UINT r;
	if (!h || !h->writable) return 0;
	r = node_write(h->node, h->pos, (const UINT8 *)data, length);
	h->pos += r;
	return r;
}

short file_close(FILEH h) { free(h); return 0; }
FILELEN file_getsize(FILEH h) { return h ? (FILELEN)h->node->size : 0; }
short file_sync(FILEH h) { (void)h; return 0; }

short file_setsize(FILEH h, FILELEN length)
{
	if (!h || !h->writable) return -1;
	if ((UINT64)length < h->node->size) node_truncate(h->node, (UINT64)length);
	else if ((UINT64)length > h->node->size) {
		if (!node_reserve(h->node, (UINT64)length)) return -1;
		h->node->size = (UINT64)length;
		h->node->written = 1;
	}
	return 0;
}

/* Files have no dates in here. A fixed one keeps the statsave check that
 * compares a disk's date with the state's from ever seeing a difference. */
short file_getdatetime(FILEH h, DOSDATE *d, DOSTIME *t)
{
	if (!h) return -1;
	if (d) { d->year = 1990; d->month = 1; d->day = 1; }
	if (t) { t->hour = 0; t->minute = 0; t->second = 0; }
	return 0;
}
short file_setdatetime(FILEH h, const DOSDATE *d, const DOSTIME *t) { (void)h; (void)d; (void)t; return 0; }

short file_delete(const OEMCHAR *path)
{
	VNODE *n = node_find(path);
	if (!n) {
		n = node_open(path);
		if (!n) return -1;
	}
	n->deleted = 1;
	return 0;
}

short file_attr(const OEMCHAR *path)
{
	return node_open(path) ? 0 : -1;
}

short file_rename(const OEMCHAR *existpath, const OEMCHAR *newpath) { (void)existpath; (void)newpath; return -1; }
short file_dircreate(const OEMCHAR *path) { (void)path; return -1; }
short file_dirdelete(const OEMCHAR *path) { (void)path; return -1; }
BRESULT file_getshortname(const OEMCHAR *path, OEMCHAR *shortname, UINT cch) { (void)path; (void)shortname; (void)cch; return FAILURE; }
BOOL file_islink(const OEMCHAR *path) { (void)path; return FALSE; }
BOOL file_infoislink(const FLINFO *fli, const OEMCHAR *path) { (void)fli; (void)path; return FALSE; }
BOOL file_islocked(const OEMCHAR *path) { (void)path; return FALSE; }

BRESULT file_getinfo(const OEMCHAR *path, FLINFO *fli)
{
	VNODE *n = node_open(path);
	if (!n) return FAILURE;
	if (fli) {
		memset(fli, 0, sizeof *fli);
		fli->caps = FLICAPS_SIZE | FLICAPS_ATTR;
		fli->size = (UINT32)n->size;
		milstr_ncpy(fli->path, file_getname(path), sizeof fli->path);
	}
	return SUCCESS;
}

/* ---- the current folder: where firmware is looked for */

static OEMCHAR curpath[MAX_PATH];
static OEMCHAR *curfilep = curpath;

void file_setcd(const OEMCHAR *exepath)
{
	file_cpyname(curpath, exepath, sizeof curpath);
	curfilep = file_getname(curpath);
	*curfilep = '\0';
}

OEMCHAR *file_getcd(const OEMCHAR *path)
{
	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return curpath;
}

FILEH file_open_c(const OEMCHAR *path) { return file_open(file_getcd(path)); }
FILEH file_open_rb_c(const OEMCHAR *path) { return file_open_rb(file_getcd(path)); }
FILEH file_create_c(const OEMCHAR *path) { return file_create(file_getcd(path)); }
short file_delete_c(const OEMCHAR *path) { return file_delete(file_getcd(path)); }
short file_attr_c(const OEMCHAR *path) { return file_attr(file_getcd(path)); }

/* No folders to list: NP2kai only lists one to find a disk's saved state
 * beside it, and those live inside the savestate here. */
FLISTH file_list1st(const OEMCHAR *dir, FLINFO *fli) { (void)dir; (void)fli; return FLISTH_INVALID; }
BRESULT file_listnext(FLISTH hdl, FLINFO *fli) { (void)hdl; (void)fli; return FAILURE; }
void file_listclose(FLISTH hdl) { (void)hdl; }

/* ---- path strings, as upstream's sdl/dosio.c has them */

void file_catname(OEMCHAR *path, const OEMCHAR *name, int maxlen)
{
	while (maxlen > 0 && *path) { path++; maxlen--; }
	file_cpyname(path, name, maxlen);
}

OEMCHAR *file_getname(const OEMCHAR *path)
{
	const OEMCHAR *ret = path;
	int csize;
	while ((csize = milstr_charsize(path)) != 0) {
		if (csize == 1 && *path == OEMPATHDIVC) ret = path + 1;
		path += csize;
	}
	return (OEMCHAR *)ret;
}

void file_cutname(OEMCHAR *path) { *file_getname(path) = '\0'; }

OEMCHAR *file_getext(const OEMCHAR *path)
{
	const OEMCHAR *p = file_getname(path), *q = NULL;
	for (; *p; p++) if (*p == '.') q = p + 1;
	return (OEMCHAR *)(q ? q : p);
}

void file_cutext(OEMCHAR *path)
{
	OEMCHAR *p = file_getname(path), *q = NULL;
	for (; *p; p++) if (*p == '.') q = p;
	if (q) *q = '\0';
}

void file_cutseparator(OEMCHAR *path)
{
	int pos = (int)strlen(path) - 1;
	if (pos > 0 && path[pos] == OEMPATHDIVC && (pos != 1 || path[0] != '.'))
		path[pos] = '\0';
}

void file_setseparator(OEMCHAR *path, int maxlen)
{
	int pos = (int)OEMSTRNLEN(path, maxlen);
	if (pos && path[pos - 1] != OEMPATHDIVC && pos + 2 < maxlen) {
		path[pos++] = OEMPATHDIVC;
		path[pos] = '\0';
	}
}

/* ---- for the driver: the files the machine changed, as save data */

int chimera_vfs_count(void) { return s_count; }

const char *chimera_vfs_name(int i)
{
	return (i >= 0 && i < s_count) ? s_order[i]->path : NULL;
}

int chimera_vfs_written(int i)
{
	return (i >= 0 && i < s_count) ? (s_order[i]->written && !s_order[i]->deleted) : 0;
}

INT64 chimera_vfs_size(int i)
{
	return (i >= 0 && i < s_count) ? (INT64)s_order[i]->size : 0;
}

INT64 chimera_vfs_read(int i, INT64 off, void *dst, INT64 len)
{
	if (i < 0 || i >= s_count || off < 0 || len <= 0) return 0;
	if (len > 0x7fffffff) len = 0x7fffffff;
	return (INT64)node_read(s_order[i], (UINT64)off, (UINT8 *)dst, (UINT)len);
}

INT64 chimera_vfs_blocks_held(int i)
{
	return (i >= 0 && i < s_count) ? (INT64)s_order[i]->held : 0;
}
