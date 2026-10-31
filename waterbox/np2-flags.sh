# np2-flags.sh - the compile flags both builds share, sourced by
# build-native.sh and build-guest.sh so the two cannot drift apart.
#
# The defines are upstream's libretro build (sdl/Makefile.common, NP2DEFINE and
# NP21DEFINE) with these taken out (docs/PLAN.md sections 2 and 3):
#   SUPPORT_VIDEOFILTER   picture filters, not part of the machine
#   SUPPORT_RESUME        resume files written to the host
#   SUPPORT_S98           S98 sound logs written to the host
#   SUPPORT_DEBUGSS       debug snapshots written to the host
#   VERMOUTH_LIB          MIDI synthesis from patch files on the host
#   SUPPORT_FPU_DOSBOX2_COMPATIBLE  names the GPL DOSBox FPU; SoftFloat 3 only
# USE_MAME stays beside USE_MAME_BSD, as upstream sets them: with both, the OPL
# chips come from ymfm (BSD-3-Clause, sound/mamebsd) and the GPL files in
# sound/mame compile to nothing. tools/sources.py does not compile them at all.
# sends every rand() in the emulation to waterbox/port/np2-rand.c,
# glibc's generator, so the native and the sandbox builds cannot differ
# (section 3, item 10).
# and these never set: SUPPORT_NP2_THREAD, SUPPORT_NP2_TICKCOUNT,
# SUPPORT_ASYNC_CPU, SUPPORT_NET, SUPPORT_LGY98, SUPPORT_TRIDENT_TGUI,
# SUPPORT_HOSTDRV.
#
# Variables in: NP2 (the upstream tree), PORT (waterbox/port), FLAVOUR (np2 or np21).
NP2_DEFINES="-D__LIBRETRO__ -DCHIMERA_CORE -DBIOS_IO_EMULATION -DOSLANG_UTF8 -DOSLINEBREAK_CRLF \
 -DSUPPORT_8BPP -DSUPPORT_16BPP -DSUPPORT_24BPP -DSUPPORT_32BPP -DSUPPORT_CL_GD5430 \
 -DSUPPORT_CRT15KHZ -DSUPPORT_FAST_MEMORYCHECK -DSUPPORT_FMGEN -DSUPPORT_GPIB -DSUPPORT_HRTIMER \
 -DSUPPORT_KAI_IMAGES -DSUPPORT_LARGE_HDD -DSUPPORT_NORMALDISP -DSUPPORT_PC9861K -DSUPPORT_PX \
 -DSUPPORT_RS232C_FIFO -DSUPPORT_SCSI -DSUPPORT_SMPU98 -DSUPPORT_SOFTKBD=0 -DSUPPORT_SOUND_SB16 \
 -DSUPPORT_STATSAVE=10 -DSUPPORT_UTF8 -DSUPPORT_V30EXT -DSUPPORT_V30ORIGINAL -DSUPPORT_VPCVHD \
 -DSUPPORT_SWSEEKSND -DSUPPORT_WAB -DUSE_MAME -DUSE_MAME_BSD -DVAEG_FIX"
NP21_DEFINES="-DCPUCORE_IA32 -DIA32_PAGING_EACHSIZE -DIA32_REBOOT_ON_PANIC -DSUPPORT_CRT31KHZ \
 -DSUPPORT_PC9801_119 -DSUPPORT_PC9821 -DSUPPORT_IDEIO -DSUPPORT_LARGE_MEMORY -DSUPPORT_PEGC \
 -DSUPPORT_VGA_MODEX -DSUPPORT_GAMEPORT -DSUPPORT_IDEIO_48BIT -DSUPPORT_FPU_SOFTFLOAT3 -DSUPPORT_PCI \
 -DUSE_FPU -DUSE_MMX -DUSE_3DNOW -DUSE_SSE -DUSE_SSE2 -DUSE_SSE3 -DUSE_SSSE3 -DUSE_SSE4_1 \
 -DUSE_SSE4_2 -DUSE_SSE4A -DUSE_TSC -DUSE_FASTPAGING -DUSE_VME -DSUPPORT_MEMDBG32"

NP2_INCLUDES="-I$PORT -I$NP2 -I$NP2/sdl -I$NP2/cbus -I$NP2/common -I$NP2/diskimage -I$NP2/diskimage/cd \
 -I$NP2/diskimage/fd -I$NP2/embed -I$NP2/embed/menu -I$NP2/embed/menubase -I$NP2/fdd -I$NP2/font \
 -I$NP2/generic -I$NP2/io -I$NP2/mem -I$NP2/sound -I$NP2/sound/mame -I$NP2/sound/fmgen \
 -I$NP2/trap -I$NP2/vram -I$NP2/wab"
if [ "$FLAVOUR" = np21 ]; then
  NP2_DEFINES="$NP2_DEFINES $NP21_DEFINES"
  NP2_INCLUDES="$NP2_INCLUDES -I$NP2/i386c -I$NP2/i386c/ia32 -I$NP2/i386c/ia32/instructions \
 -I$NP2/i386c/ia32/instructions/fpu -I$NP2/i386c/ia32/instructions/fpu/softfloat \
 -I$NP2/i386c/ia32/instructions/mmx -I$NP2/i386c/ia32/instructions/sse"
else
  NP2_INCLUDES="$NP2_INCLUDES -I$NP2/i286c"
fi
# Upstream's own warning switches (COMMON_C_FLAGS, COMMON_CXX_FLAGS).
NP2_CFLAGS="-Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion"
NP2_CXXFLAGS="-fno-sized-deallocation"

# Every local variable starts at zero (issue #2). NP2kai copies locals it
# never fully set into the machine (descriptor_t's d_pad, from the
# "descriptor_t sd;" locals in i386c/ia32), so whatever was on the stack
# reached savestates. On Windows the host's exception delivery leaves its
# own bytes on the guest stack (miniBox docs/RED-ZONE.md), so a Windows
# savestate could differ from a Linux one by those bytes.
NP2_ZEROINIT="-ftrivial-auto-var-init=zero"
NP2_CFLAGS="$NP2_CFLAGS $NP2_ZEROINIT"
NP2_CXXFLAGS="$NP2_CXXFLAGS $NP2_ZEROINIT"

# libchdr (extern/libchdr, BSD-3-Clause; its deps/ are LZMA SDK, public
# domain; miniz, MIT; zstd's decoder, BSD-3-Clause; dr_flac, public domain):
# MAME's CHD reader, for CD images in .chd (waterbox/port/chd-cd.c). It is
# built from its own unity.c (and the A/V codec, which unity.c leaves out
# but the codec table names) with its own flags, not NP2kai's: none of
# NP2kai's defines or include folders reach it. ZSTD_DISABLE_ASM keeps the
# zstd decoder to C (its assembler file is not part of unity.c).
# LIBCHDR_DIR=dir builds it from another checkout.
LIBCHDR=${LIBCHDR_DIR:-$NP2/../libchdr}
LIBCHDR_FLAGS="-I$LIBCHDR/include -I$LIBCHDR/deps/lzma-26.02/include -I$LIBCHDR/deps/miniz-3.1.2 \
 -I$LIBCHDR/deps/zstd-1.5.7 -DZSTD_DISABLE_ASM -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1"
[ -f "$LIBCHDR/unity.c" ] || { echo "no libchdr at $LIBCHDR (git submodule update --init extern/libchdr)" >&2; exit 1; }

# Per-file flags. fdd/sxsi.c builds its own GetTickCount() on gettimeofday()
# and times a CD change with it (sxsi.c:29,306,344), so in that file the host
# clock is swapped for the emulated one (waterbox/port/platform.c).
np2_file_flags() {
  case "$1" in
    */fdd/sxsi.c) echo "-Dgettimeofday=chimera_gettimeofday" ;;
    */port/chd-cd.c) echo "-I$LIBCHDR/include" ;;
  esac
}
