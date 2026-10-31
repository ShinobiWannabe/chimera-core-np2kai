/* bkdsk-hdb.c - BKDSK's N-88 BASIC 2HD images (.hdb), track 0 readable.
 *
 * On a BASIC 2HD disk the first track (cylinder 0, head 0) is 26 FM
 * sectors of 128 bytes, and every other track 26 MFM sectors of 256. The
 * .hdb image keeps them as they are: 3328 bytes for track 0, then 6656 a
 * track. Upstream's loader (diskimage/fd/fdd_bkdsk.c) finds the tracks
 * right but reads with DCP's reader, which halves track 0's sectors only
 * when the union's DCP header (never set for BKDSK) says BASIC-2HD, and
 * which asks for MFM and N 1 there. So track 0 read as 256-byte MFM
 * sectors, half of them past its end, and its own FM sectors failed: no
 * BASIC disk could start. That file is compiled here, unchanged, with its
 * loader renamed; ours loads as it does, then reads, writes and reads the
 * IDs of track 0 as FM with N 0, and leaves the other tracks to DCP's
 * reader as before.
 */
#define fdd_set_bkdsk fdd_set_bkdsk_upstream
#include "../../extern/NP2kai/diskimage/fd/fdd_bkdsk.c"
#undef fdd_set_bkdsk

#ifdef SUPPORT_KAI_IMAGES

#define HDB_T0_N	0	/* track 0: 128 bytes a sector, FM */

static int hdb_track0(void)
{
	return fdc.treg[fdc.us] == 0 && fdc.hd == 0;
}

/* fdd_seeksector_common and DCP's end-of-track check, for track 0: FM,
 * where every other track is MFM. 0xff is "any density": NP2kai's own
 * INT 1Bh sets it for READ DATA and WRITE DATA (bios/bios1b.c), and the
 * common check takes it too. */
static BRESULT hdb_seeksector0(FDDFILE fdd)
{
	if ((CTRL_FDMEDIA != fdd->inf.xdf.disktype) || (fdc.rpm[fdc.us] != fdd->inf.xdf.rpm)) {
		fddlasterror = 0xe0;
		return FAILURE;
	}
	if (!fdc.R || fdc.R > fdd->inf.xdf.sectors || (fdc.mf != 0x00 && fdc.mf != 0xff) ||
	    fdc.N != HDB_T0_N || fdc.eot > fdd->inf.xdf.sectors) {
		fddlasterror = 0xc0;
		return FAILURE;
	}
	return SUCCESS;
}

static BRESULT hdb_read(FDDFILE fdd)
{
	FILEH hdl;
	long seekp;

	if (!hdb_track0()) {
		return fdd_read_dcp(fdd);
	}
	fddlasterror = 0x00;
	if (hdb_seeksector0(fdd)) {
		return FAILURE;
	}
	seekp = (long)(fdc.R - 1) << 7;
	hdl = file_open_rb(fdd->fname);
	if (hdl == FILEH_INVALID) {
		fddlasterror = 0xe0;
		return FAILURE;
	}
	if (file_seek(hdl, seekp, FSEEK_SET) != seekp || file_read(hdl, fdc.buf, 128) != 128) {
		file_close(hdl);
		fddlasterror = 0xe0;
		return FAILURE;
	}
	file_close(hdl);
	fdc.bufcnt = 128;
	return SUCCESS;
}

static BRESULT hdb_write(FDDFILE fdd)
{
	FILEH hdl;
	long seekp;

	if (!hdb_track0()) {
		return fdd_write_dcp(fdd);
	}
	fddlasterror = 0x00;
	if (hdb_seeksector0(fdd)) {
		fddlasterror = 0xe0;
		return FAILURE;
	}
	if (fdd->protect) {
		fddlasterror = 0x70;
		return FAILURE;
	}
	seekp = (long)(fdc.R - 1) << 7;
	hdl = file_open(fdd->fname);
	if (hdl == FILEH_INVALID) {
		fddlasterror = 0xc0;
		return FAILURE;
	}
	if (file_seek(hdl, seekp, FSEEK_SET) != seekp || file_write(hdl, fdc.buf, 128) != 128) {
		file_close(hdl);
		fddlasterror = 0xc0;
		return FAILURE;
	}
	file_close(hdl);
	fdc.bufcnt = 128;
	return SUCCESS;
}

/* fdd_readid_common, with track 0's IDs FM and N 0 */
static BRESULT hdb_readid(FDDFILE fdd)
{
	fddlasterror = 0x00;
	if (fdc.crcn >= fdd->inf.xdf.sectors) {
		fdc.crcn = 0;
		if (fdc.mt) {
			fdc.hd ^= 1;
			if (fdc.hd == 0) {
				fdc.treg[fdc.us]++;
			}
		} else {
			fdc.treg[fdc.us]++;
		}
	}
	if (!hdb_track0()) {
		return fdd_readid_common(fdd);
	}
	if (fdc.mf || (fdc.rpm[fdc.us] != fdd->inf.xdf.rpm) || (CTRL_FDMEDIA != fdd->inf.xdf.disktype)) {
		fddlasterror = 0xe0;
		return FAILURE;
	}
	fdc.C = 0;
	fdc.H = 0;
	fdc.R = ++fdc.crcn;
	fdc.N = HDB_T0_N;
	return SUCCESS;
}

BRESULT fdd_set_bkdsk(FDDFILE fdd, FDDFUNC fdd_fn, const OEMCHAR *fname, int ro)
{
	if (fdd_set_bkdsk_upstream(fdd, fdd_fn, fname, ro) != SUCCESS) {
		return FAILURE;
	}
	fdd->inf.dcp.head.mediatype = 0;	/* not DCP's BASIC-2HD halving */
	fdd_fn->read = hdb_read;
	fdd_fn->write = hdb_write;
	fdd_fn->readid = hdb_readid;
	return SUCCESS;
}

#endif
