#!/bin/sh
# run-gate.sh - the core gate: the native build against the sandbox build,
# leg by leg, each saying what it compared, and each with a negative control
# that must fail (docs/gates.md in Chimera).
#
# The sandbox side is the package's core.wbx, both machines in one
# (build-guest.sh both); each machine is held to its own native build.
#
# Usage: run-gate.sh
# Environment:
#   NP2_FIRMWARE  a folder holding the required firmware, the user's own:
#                 FONT.ROM, BIOS.ROM, SOUND.ROM, IDE.ROM (or
#                 IDEBIOS.ROM) and the six 2608_*.WAV, any case. Without
#                 all of them every leg that boots says SKIP. With MAME's
#                 ym2608_adpcm_rom.bin there too, the drum legs also play
#                 the drums from the YM2608's ROM; without it they say SKIP.
#   NP2_FONT      FONT.ROM to use instead of the folder's (optional).
#   NP2_GAMES     a folder of game folders, each holding its disks and a
#                 "slots" file (and "settings" if it wants), run as the
#                 native-against-sandbox leg. Local only: never committed.
#   NP2_FRAMES    frames a leg runs (default 1500).
#   MINIBOX_DIR   the miniBox guest kit (default ../chimera/extern/...).
# Exit status: 0 when no leg failed. A SKIP is not a pass, and is counted.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
frames=${NP2_FRAMES:-1500}
work=$root/build/gate
native=$root/build/native-np21/run-native
native286=$root/build/native-np2/run-native
wbx=$root/build/wbx-both/core.wbx
runwbx=$root/build/run-wbx
pass=0; fail=0; skip=0
ok()   { echo "PASS $*"; pass=$((pass + 1)); }
bad()  { echo "FAIL $*"; fail=$((fail + 1)); }
skip() { echo "SKIP $*"; skip=$((skip + 1)); }

rm -rf "$work"; mkdir -p "$work"
for f in np21 np2; do
	sh "$here/build-native.sh" $f > "$work/build-native-$f.log" 2>&1 || { echo "native build failed, see $work/build-native-$f.log"; exit 1; }
done
sh "$here/build-guest.sh" both > "$work/build-guest.log" 2>&1 || { echo "guest build failed, see $work/build-guest.log"; exit 1; }
sh "$here/build-run-wbx.sh" > "$work/build-run-wbx.log" 2>&1 || { echo "run-wbx build failed"; exit 1; }

# ---- leg: no host clock and no host random numbers in the core ----------
# Static: what the core's objects (all but the harness's own) import from
# the C library. The control is a build with NP2DRV_HOST_CLOCK, libretro's
# clock, which must show clock_gettime, or this leg could not see one.
host_calls() {	# host_calls <obj dir>
	ls "$1"/*.o | grep -v '_run-native\.o$' | xargs nm -u 2>/dev/null |
		grep -E ' (clock_gettime|gettimeofday|time|ftime|rand|random|srand|srandom|rand_r)$' | sort -u
}
NP2_OUT=$work/native-hostclock NP2_EXTRA=-DNP2DRV_HOST_CLOCK sh "$here/build-native.sh" np21 > "$work/build-hostclock.log" 2>&1
real=$(host_calls "$root/build/native-np21/obj")
ctl=$(host_calls "$work/native-hostclock/obj")
if [ -z "$real" ] && echo "$ctl" | grep -q clock_gettime; then
	ok "no host clock or host rand in the core's objects (the host-clock control imports clock_gettime)"
else
	bad "host clock or rand imported: '$real' (control: '$ctl')"
fi

# the required firmware (waterbox.config "firmware"), each under its id, in
# small letters as Chimera mounts it
FIRMWARE_IDS="bios.rom sound.rom ide.rom 2608_bd.wav 2608_sd.wav 2608_top.wav 2608_hh.wav 2608_tom.wav 2608_rim.wav"
fw_file() {	# fw_file <id> -> the file in NP2_FIRMWARE, any case (IDE.ROM may be IDEBIOS.ROM)
	for n in "$1" $( [ "$1" = ide.rom ] && echo idebios.rom ); do
		f=$(find "${NP2_FIRMWARE:-/nonexistent}" -maxdepth 1 -type f -iname "$n" 2>/dev/null | head -1)
		[ -n "$f" ] && { echo "$f"; return 0; }
	done
	return 1
}
fw_missing=""
[ -n "${NP2_FONT:-}" ] || NP2_FONT=$(fw_file FONT.ROM) || NP2_FONT=""
[ -f "$NP2_FONT" ] || fw_missing=" FONT.ROM"
for id in $FIRMWARE_IDS; do fw_file "$id" > /dev/null || fw_missing="$fw_missing $id"; done
# a folder for one run: the firmware, the disks, the slots and settings
folder() {	# folder <name> -> path
	d=$work/run-$1; mkdir -p "$d"; cp "$NP2_FONT" "$d/font.rom"
	for id in $FIRMWARE_IDS; do cp "$(fw_file "$id")" "$d/$id"; done
	echo "$d"
}
lines() { grep '^[0-9]'; }

# ---- leg: native against sandbox ------------------------------------------
compare() {	# compare <label> <folder> [env...]; $vs is the native build (default np21)
	label=$1; d=$2; shift 2
	env "$@" "${vs:-$native}" "$d" "$frames" 2>"$work/$label.n.err" | lines > "$work/$label.n"
	env "$@" "$runwbx" "$wbx" "$d" "$frames" 2>"$work/$label.w.err" | lines > "$work/$label.w"
	n=$(wc -l < "$work/$label.n")
	if [ "$n" -eq "$frames" ] && cmp -s "$work/$label.n" "$work/$label.w"; then
		ok "$label: native and sandbox the same over $n frames (picture, sound, clock, input read)"
	else
		bad "$label: native and sandbox differ (see $work/$label.n and .w)"
	fi
}

if [ -n "$fw_missing" ]; then
	skip "every boot leg: required firmware missing from NP2_FIRMWARE ($fw_missing; the user's own, never committed)"
else
	d=$(folder nodisk); echo '{}' > "$d/slots"
	compare nodisk "$d"

	# ---- leg: save data, with the generated test disk ----------------------
	d=$(folder save); python3 "$root/tests/gen-testdisk.py" "$d/test.hdm"; echo '{"floppy":["test.hdm"]}' > "$d/slots"
	compare savedisk "$d"
	mkdir -p "$work/sd"
	NP2_SAVEDATA=$work/sd "$runwbx" "$wbx" "$d" 600 > /dev/null 2>&1
	if python3 - "$d/test.hdm" "$work/sd/test.hdm" <<'PY'
import sys
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
changed = [i for i in range(len(a)) if a[i] != b[i]]
sys.exit(0 if len(a) == len(b) and b[1024:2048] == a[0:1024] and changed and changed[0] >= 1024 and changed[-1] < 2048 else 1)
PY
	then ok "save data: the exported disk holds the sector the machine wrote, and nothing else changed"
	else bad "save data: the exported disk is not what the machine wrote"; fi
	# control: the same disk without the write exports nothing
	dn=$(folder savenone); python3 "$root/tests/gen-testdisk.py" --no-write "$dn/test.hdm"; cp "$d/slots" "$dn/"
	mkdir -p "$work/sdn"; NP2_SAVEDATA=$work/sdn "$runwbx" "$wbx" "$dn" 600 > /dev/null 2>&1
	if [ -z "$(ls "$work/sdn")" ]; then ok "save data control: a disk nobody wrote is not exported"
	else bad "save data control: an unwritten disk was exported"; fi

	# ---- leg: the drive lights and the registers ------------------------------
	# The test disk is read once to boot and written once, so the floppy's light
	# comes on, and not on every frame; the disk-less machine has no drive to
	# light. Its program ends in a loop at 1FC0:001B with the BIOS's answer, 0,
	# in AH: the registers must say so, the same in both builds.
	lit=$(awk '$8 != "0"' "$work/savedisk.w" | wc -l)
	unlit=$(awk '$8 != "0"' "$work/nodisk.w" | wc -l)
	if [ "$lit" -gt 0 ] && [ "$lit" -lt "$frames" ] && [ "$unlit" -eq 0 ]; then
		ok "drive lights: the floppy is lit on $lit of $frames frames, the same in both builds; with no disk, never"
	else bad "drive lights: lit on $lit frames with the disk, $unlit without"; fi
	rn=$(NP2_REGS=1 "$native" "$d" "$frames" 2>/dev/null | grep '^REGS')
	rw=$(NP2_REGS=1 "$runwbx" "$wbx" "$d" "$frames" 2>/dev/null | grep '^REGS')
	case "$rw" in
		*" EAX=90 "*" EIP=1b "*" CS=1fc0 "*) regs_ok=1 ;;
		*) regs_ok=0 ;;
	esac
	if [ "$regs_ok" = 1 ] && [ "$rn" = "$rw" ]; then ok "registers: the program stops at 1FC0:001B with AH 0, the same in both builds"
	else bad "registers: native '$rn', sandbox '$rw'"; fi

	# ---- leg: turbo leaves the machine as it was -----------------------------
	# Frames 100 to 299 not drawn into the picture buffer: every other column of
	# every line, and every column outside those frames, the same as a drawn
	# run, and the machine's memory at the end the same. The control: some frame
	# in that window shows a stale picture, or nothing was skipped.
	NP2_NORENDER=100,300 "$runwbx" "$wbx" "$d" "$frames" /dev/null "$work/turbo.mem" 2>/dev/null | lines > "$work/turbo.w"
	"$runwbx" "$wbx" "$d" "$frames" /dev/null "$work/drawn.mem" > /dev/null 2>&1
	if awk 'NR == FNR { a[$1] = $0; next }
		{ n = $1; split(a[n], x, " "); same = 1
		  for (i = 3; i <= NF; i++) if (x[i] != $i) same = 0
		  if (n < 100 || n >= 300) { if (a[n] != $0) same = 0 } else if (x[2] != $2) stale++
		  if (!same) bad++ }
		END { exit !(bad == 0 && stale > 0) }' "$work/savedisk.w" "$work/turbo.w" && cmp -s "$work/turbo.mem" "$work/drawn.mem"
	then ok "turbo: 200 frames undrawn change nothing but the picture of those frames, and the memory is the same"
	else bad "turbo: the undrawn run differs (see $work/turbo.w)"; fi

	# ---- leg: savestates, and the control that must differ -----------------
	at=$((frames / 2))
	s=$(NP2_STATE_AT=$at "$runwbx" "$wbx" "$d" "$frames" 2>/dev/null | grep '^STATE')
	c=$(NP2_STATE_AT=$at NP2_STATE_CONTROL=1 "$runwbx" "$wbx" "$d" "$frames" 2>/dev/null | grep '^STATE')
	case "$s/$c" in
		"STATE same"*"/STATE DIFFER"*) ok "savestate at frame $at: the replay after loading is the same ($s); the control with one extra key differs" ;;
		*) bad "savestate: '$s', control '$c'" ;;
	esac

	# ---- leg: a host stall changes nothing ---------------------------------
	"$native" "$d" "$frames" 2>/dev/null | lines > "$work/stall.a"
	NP2_STALL_FRAME=$at "$native" "$d" "$frames" 2>/dev/null | lines > "$work/stall.b"
	if cmp -s "$work/stall.a" "$work/stall.b"; then ok "a 300 ms stall at frame $at changes nothing"
	else bad "a host stall changed the run"; fi

	# ---- leg: settings reach the machine -----------------------------------
	d2=$(folder settings); cp "$d/test.hdm" "$d/slots" "$d2/"
	printf '{"monitor":"31khz","baseClock":"1.9968 MHz","clockMultiplier":"8","soundBoard":"PC9801-26K"}' > "$d2/settings"
	compare settings "$d2"
	v=$(grep vsync "$work/settings.w.err"); vn=$(grep vsync "$work/settings.n.err")
	if [ "$v" = "vsync 998400/14197" ] && [ "$vn" = "$v" ] && ! cmp -s "$work/settings.n" "$work/savedisk.n"; then
		ok "settings: the 31 kHz monitor at 1.9968 MHz reports 998400/14197 in both builds, and the run differs from the default one"
	else bad "settings: rate '$v' / '$vn', or the settings changed nothing"; fi

	# ---- leg: the drums, from the WAV files and from the YM2608's ROM ------
	# The rhythm disk keys each drum in turn. With either sound generator the
	# drums sound: the run differs from one with the WAV files taken away
	# (NP2's own generator asked for them in small letters and was silent).
	# With the ROM (ym2608_adpcm_rom.bin in NP2_FIRMWARE, the user's own,
	# never committed) and no WAV files, native and sandbox are the same and
	# only the sound differs from the WAV run.
	only_sound() {	# only_sound <a> <b>: the same frames but for the sound, and the sound not
		awk 'NR == FNR { a[$1] = $0; next }
			{ split(a[$1], x, " "); for (i = 1; i <= NF; i++) if (i != 5 && x[i] != $i) bad++
			  if (x[5] != $5) diff++ }
			END { exit !(bad == 0 && diff > 0) }' "$work/$1.w" "$work/$2.w"
	}
	rom=$(fw_file ym2608_adpcm_rom.bin) || rom=""
	dsave=$d	# folder() sets d
	for g in NP2 fmgen; do
		dr=$(folder rhythm-$g); python3 "$root/tests/gen-testdisk.py" --rhythm "$dr/rhythm.hdm"; echo '{"floppy":["rhythm.hdm"]}' > "$dr/slots"
		printf '{"soundGenerator":"%s"}' "$g" > "$dr/settings"
		compare "rhythm-$g" "$dr"
		dq=$(folder rhythm-$g-none); cp "$dr/rhythm.hdm" "$dr/slots" "$dr/settings" "$dq/"; rm -f "$dq"/2608_*
		"$runwbx" "$wbx" "$dq" "$frames" 2>/dev/null | lines > "$work/rhythm-$g-none.w"
		if only_sound "rhythm-$g-none" "rhythm-$g"; then ok "drums, $g: the WAV files sound (the run differs from one without them only in the sound)"
		else bad "drums, $g: the WAV files make no sound, or change more than the sound"; fi
		if [ -z "$rom" ]; then skip "drums, $g, YM2608 ROM: ym2608_adpcm_rom.bin not in NP2_FIRMWARE (the user's own, never committed)"; continue; fi
		dm=$(folder rhythm-$g-rom); cp "$dr/rhythm.hdm" "$dr/slots" "$dm/"; rm -f "$dm"/2608_*; cp "$rom" "$dm/ym2608_adpcm_rom.bin"
		printf '{"soundGenerator":"%s","rhythmSamples":"YM2608 ROM"}' "$g" > "$dm/settings"
		compare "rhythm-$g-rom" "$dm"
		if only_sound "rhythm-$g" "rhythm-$g-rom" && only_sound "rhythm-$g-none" "rhythm-$g-rom"; then
			ok "drums, $g, YM2608 ROM: they sound with no WAV files, and differ from the WAV run only in the sound"
		else bad "drums, $g, YM2608 ROM: silent, or more than the sound changed"; fi
	done

	# ---- leg: firmware found whatever the case of the name asked for --------
	# Chimera mounts each firmware file under its id, in small letters; NP2kai
	# and fmgen ask in either case (2608_bd.wav, 2608_BD.WAV, FONT.ROM).
	# tests/firmware-names.c asks for every id in small letters, capitals and
	# mixed case. Then each file must be used: on the rhythm disk, taking any
	# one file away changes the run (its frames or the memory at the end); ide.rom, read only with a hard disk, on
	# its own with a blank one fitted.
	if gcc -o "$work/firmware-names" "$root/tests/firmware-names.c" "$root/build/native-np21/obj/waterbox_port_dosio.o" \
		"$root/build/native-np21/obj/extern_NP2kai_common_milstr.o" 2>"$work/firmware-names.err" &&
		"$work/firmware-names" > "$work/firmware-names.out"; then
		ok "firmware names: $(tail -1 "$work/firmware-names.out") (each id in small letters, capitals and mixed case)"
	else bad "firmware names: $(cat "$work/firmware-names.out" "$work/firmware-names.err" 2>/dev/null | tail -3)"; fi
	di=$(folder used-ide); echo '{"hdd":["blank.nhd"]}' > "$di/slots"
	python3 - "$di/blank.nhd" <<'PY'
import struct, sys
cyl, heads, secs, size = 20, 8, 17, 512
hdr = b"T98HDDIMAGE.R0\0\0" + bytes(0x100) + struct.pack("<IIHHH", 512, cyl, heads, secs, size)
hdr += bytes(512 - len(hdr))
open(sys.argv[1], "wb").write(hdr + bytes(cyl * heads * secs * size))
PY
	"$runwbx" "$wbx" "$di" "$frames" 2>/dev/null | lines > "$work/used-ide.ref"
	dc=$(folder used-noide); rm -f "$dc"/*; cp "$di"/* "$dc/"; rm -f "$dc/ide.rom"
	"$runwbx" "$wbx" "$dc" "$frames" 2>/dev/null | lines > "$work/used-ide.no"
	if [ "$(wc -l < "$work/used-ide.ref")" -eq "$frames" ] && ! cmp -s "$work/used-ide.ref" "$work/used-ide.no"; then
		ok "firmware used: with a hard disk, taking ide.rom away changes the run"
	else bad "firmware used: ide.rom not found or not used"; fi

	# ---- leg: the real IDE BIOS boots a hard disk --------------------------
	# The counter as the hard disk's first sector, with ide.rom: the BIOS
	# must be told the disk is there (0457h, port/bios-ide-units.c) or it
	# waits 25 s on the absent second unit and boots nothing. Its frame count
	# (1FC70h) must have started, the same in both builds; the control, the
	# same disk with an empty first sector, never starts it.
	hdd_frames() {	# hdd_frames <memory dump> -> the counter's frames
		python3 -c "import sys; m = open(sys.argv[1], 'rb').read(); print(int.from_bytes(m[0x1fc70:0x1fc72], 'little'))" "$1"
	}
	dh=$(folder ide-boot); python3 "$root/tests/gen-testdisk.py" --hdd "$dh/c.nhd"; echo '{"hdd":["c.nhd"]}' > "$dh/slots"
	compare ide-boot "$dh"
	"$runwbx" "$wbx" "$dh" "$frames" /dev/null "$work/ide-boot.mem" > /dev/null 2>&1
	dn=$(folder ide-noboot); python3 "$root/tests/gen-testdisk.py" --hdd --no-boot "$dn/c.nhd"; cp "$dh/slots" "$dn/"
	"$runwbx" "$wbx" "$dn" "$frames" /dev/null "$work/ide-noboot.mem" > /dev/null 2>&1
	hb=$(hdd_frames "$work/ide-boot.mem"); hn=$(hdd_frames "$work/ide-noboot.mem")
	if [ "${hb:-0}" -gt 0 ] && [ "${hn:-1}" -eq 0 ]; then
		ok "real IDE BIOS: the hard disk boots and its program counts $hb frames; the control with no boot sector counts none"
	else bad "real IDE BIOS: the hard disk's program counted '$hb' frames, the control '$hn'"; fi
	dh=$(folder used-ref); python3 "$root/tests/gen-testdisk.py" --rhythm "$dh/rhythm.hdm"
	echo '{"floppy":["rhythm.hdm"]}' > "$dh/slots"
	[ -n "$rom" ] && cp "$rom" "$dh/ym2608_adpcm_rom.bin"
	for set in 'NP2:WAV files' 'fmgen:WAV files' 'NP2:YM2608 ROM' 'fmgen:YM2608 ROM'; do
		g=${set%%:*}; r=${set#*:}; lbl=used-$g-$(echo "$r" | tr -c 'A-Za-z0-9\n' '_')
		if [ "$r" = "YM2608 ROM" ] && [ -z "$rom" ]; then skip "firmware used, $g, $r: ym2608_adpcm_rom.bin not in NP2_FIRMWARE"; continue; fi
		printf '{"soundGenerator":"%s","rhythmSamples":"%s"}' "$g" "$r" > "$dh/settings"
		"$runwbx" "$wbx" "$dh" "$frames" /dev/null "$work/$lbl.ref.mem" 2>/dev/null | lines > "$work/$lbl.ref"
		if [ "$r" = "WAV files" ]; then files="font.rom bios.rom sound.rom 2608_bd.wav 2608_sd.wav 2608_top.wav 2608_hh.wav 2608_tom.wav 2608_rim.wav"
		else files="font.rom bios.rom sound.rom ym2608_adpcm_rom.bin"; fi
		unused=""
		for f in $files; do
			dc=$(folder "$lbl-no-$f"); rm -f "$dc"/*; cp "$dh"/* "$dc/"; rm -f "$dc/$f"
			"$runwbx" "$wbx" "$dc" "$frames" /dev/null "$work/$lbl.no.mem" 2>/dev/null | lines > "$work/$lbl.no-$f"
			cmp -s "$work/$lbl.ref" "$work/$lbl.no-$f" && cmp -s "$work/$lbl.ref.mem" "$work/$lbl.no.mem" && unused="$unused $f"
			rm -rf "${dc:?}" "$work/$lbl.no.mem"
		done
		if [ -z "$unused" ] && [ "$(wc -l < "$work/$lbl.ref")" -eq "$frames" ]; then
			ok "firmware used, $g, $r: taking away any of the $(echo $files | wc -w) files changes the run (frames or memory)"
		else bad "firmware used, $g, $r: not found or not used:$unused"; fi
	done
	d=$dsave

	# ---- leg: the 286 machine, the other half of the same core.wbx --------
	# Its settings say machine PC98; it is held to the 286 native build, and
	# must not be the IA-32 run, or the setting reached nothing.
	to286() {	# to286 <folder>: add machine PC98 to its settings
		python3 - "$1/settings" <<'PY'
import json, os, sys
p = sys.argv[1]
s = json.load(open(p)) if os.path.exists(p) else {}
s["machine"] = "PC98"
json.dump(s, open(p, "w"))
PY
	}
	d3=$(folder 286); cp "$d/test.hdm" "$d/slots" "$d3/"; to286 "$d3"
	vs=$native286 compare savedisk286 "$d3"
	if ! cmp -s "$work/savedisk286.w" "$work/savedisk.w"; then ok "286: the machine setting picks the other half (its run is not the IA-32 one)"
	else bad "286: the PC98 run is the IA-32 run"; fi
	s=$(NP2_STATE_AT=$at "$runwbx" "$wbx" "$d3" "$frames" 2>/dev/null | grep '^STATE')
	c=$(NP2_STATE_AT=$at NP2_STATE_CONTROL=1 "$runwbx" "$wbx" "$d3" "$frames" 2>/dev/null | grep '^STATE')
	case "$s/$c" in
		"STATE same"*"/STATE DIFFER"*) ok "286 savestate at frame $at: the replay after loading is the same; the control differs" ;;
		*) bad "286 savestate: '$s', control '$c'" ;;
	esac

	# ---- leg: the input-poll callback on both machines ----------------------
	# Chimera's engine gives SetInputPollCallback before Init (session.cpp),
	# when the entry has not picked a half yet; run-wbx does the same with
	# NP2_POLLS. The counter disk asks INT 18h for a key every frame once it
	# has booted, so the callback runs on either machine.
	dp=$(folder polls); python3 "$root/tests/gen-testdisk.py" --counter "$dp/c.hdm"; echo '{"floppy":["c.hdm"]}' > "$dp/slots"
	p21=$(NP2_POLLS=1 "$runwbx" "$wbx" "$dp" 600 2>/dev/null | sed -n 's/^POLLS //p')
	to286 "$dp"
	p286=$(NP2_POLLS=1 "$runwbx" "$wbx" "$dp" 600 2>/dev/null | sed -n 's/^POLLS //p')
	if [ "${p21:-0}" -gt 0 ] && [ "${p286:-0}" -gt 0 ]; then ok "input polls: the callback given before Init runs on both machines ($p21 and $p286 calls)"
	else bad "input polls: PC9821 '$p21', PC98 '$p286' calls"; fi

	# ---- leg: SHIFT and Right SHIFT are one key while the machine merges them
	# The BIOS's shift state (0000:053Ah bit 0) at frame 250: SHIFT held from
	# 100 to 300 with Right SHIFT tapped from 150 to 200 is still shifted;
	# the control, SHIFT let go at 200, is not.
	ds=$(folder shift); cp "$dp/c.hdm" "$dp/slots" "$ds/"
	shift_bit() {	# shift_bit <press script> <settings> -> 0 or 1
		printf '%s' "$2" > "$ds/settings"
		NP2_PRESS=$1 "$runwbx" "$wbx" "$ds" 250 /dev/null "$work/shift.mem" > /dev/null 2>&1
		python3 -c "import sys; print(open(sys.argv[1], 'rb').read()[0x53a] & 1)" "$work/shift.mem"
	}
	for m in PC9821 PC98; do
		a=$(shift_bit "100:Key SHIFT:200,150:Key Right SHIFT:50" "{\"machine\":\"$m\"}")
		c=$(shift_bit "100:Key SHIFT:100,150:Key Right SHIFT:50" "{\"machine\":\"$m\"}")
		if [ "$a" = 1 ] && [ "$c" = 0 ]; then ok "shift, $m: letting go of Right SHIFT leaves SHIFT held; the control is let go"
		else bad "shift, $m: held '$a', control '$c'"; fi
	done

	# ---- leg: one floppy is never in both drives, and an empty domain hides --
	# Three floppies: drive 1 holds 0, drive 2 holds 1. Selecting disk 1 for
	# drive 1 and pressing Swap makes the drives trade disks; selecting disk 2
	# puts it in. With no extended memory its domain is hidden from the tools.
	df=$(folder floppies); cp "$dp/c.hdm" "$df/a.hdm"; cp "$dp/c.hdm" "$df/b.hdm"; cp "$dp/c.hdm" "$df/c.hdm"
	echo '{"floppy":["a.hdm","b.hdm","c.hdm"]}' > "$df/slots"; echo '{"extraMemoryMB":0}' > "$df/settings"
	m1=$(NP2_MEDIA=1 NP2_PRESS="100:Drive 1 Next Floppy Disk:4,120:Drive 1 Swap Floppy Disk:4" "$runwbx" "$wbx" "$df" 200 2>/dev/null | grep '^MEDIA')
	m2=$(NP2_MEDIA=1 NP2_PRESS="100:Drive 1 Next Floppy Disk:4,110:Drive 1 Next Floppy Disk:4,120:Drive 1 Swap Floppy Disk:4" "$runwbx" "$wbx" "$df" 200 2>/dev/null | grep '^MEDIA\|^HIDDEN' | tr '\n' ' ')
	if [ "$m1" = "MEDIA 1 0" ] && [ "$m2" = "MEDIA 2 1 HIDDEN 0 1 0 0 0 0 0 0 " ]; then
		ok "floppies: Swap with drive 2's disk makes the drives trade, and puts another in; with 0 MB the extended memory domain is hidden"
	else bad "floppies: '$m1' then '$m2'"; fi

	# ---- leg: no limit on floppies ---------------------------------------
	# Eighteen floppies: Previous from the first goes round to the last.
	dm=$(folder many-floppies); list=
	for i in $(seq 0 17); do cp "$dp/c.hdm" "$dm/d$i.hdm"; list="$list\"d$i.hdm\","; done
	echo "{\"floppy\":[${list%,}]}" > "$dm/slots"
	m3=$(NP2_MEDIA=1 NP2_PRESS="100:Drive 1 Previous Floppy Disk:4,120:Drive 1 Swap Floppy Disk:4" "$runwbx" "$wbx" "$dm" 200 2>/dev/null | grep '^MEDIA')
	if [ "$m3" = "MEDIA 17 1" ]; then ok "floppies: all 18 of 18 can be put in"
	else bad "floppies: 18 disks gave '$m3'"; fi

	# ---- leg: cue sheets written the ways real ones are --------------------
	# tests/cue-join.c runs the driver's joiner on sheets with a byte order
	# mark, bare names, CRLF, small letters, FLAGS/POSTGAP and a Windows
	# folder; each must join as the plain sheet does, and a WAVE file or a
	# 2048-byte track must be left to NP2kai.
	unit_test() {	# unit_test <name> -> builds tests/<name>.c with the driver in $work/<name>
		mkdir -p "$work/$1"
		( NP2=$root/extern/NP2kai; PORT=$here/port; FLAVOUR=np21; . "$here/np2-flags.sh"
		  mb=${MINIBOX_DIR:-$root/../chimera/extern/chimera-common-minibox}
		  gcc -O1 -fno-strict-aliasing $NP2_DEFINES $NP2_INCLUDES -I"$mb/extern/emulibc" -I"$mb/source/guest/include" -I"$mb/extern/jsmn" \
		    $NP2_CFLAGS -w -c "$root/tests/$1.c" -o "$work/$1/$1.o" &&
		  g++ -o "$work/$1/$1" "$work/$1/$1.o" $(ls "$root"/build/native-np21/obj/*.o | grep -v '_np2-driver\.o$\|_run-native\.o$') -lm
		) > "$work/$1/build.log" 2>&1
		(cd "$work/$1" && "./$1" > out.txt 2>&1)
	}
	if unit_test cue-join; then ok "cue sheets: $(tail -1 "$work/cue-join/out.txt")"
	else bad "cue sheets: $(tail -3 "$work/cue-join/out.txt" "$work/cue-join/build.log" 2>/dev/null)"; fi

	# ---- leg: the calendar past 49.7 days ----------------------------------
	# tests/calendar.c sets the driver's clock total to 50 and 5000 days of
	# running; the calendar must read the start date plus that, not wrap as
	# GETTICK's 32-bit milliseconds do.
	if unit_test calendar; then ok "calendar: $(tail -1 "$work/calendar/out.txt")"
	else bad "calendar: $(tail -3 "$work/calendar/out.txt" "$work/calendar/build.log" 2>/dev/null)"; fi

	# ---- leg: the user's games, local only ----------------------------------
	if [ -n "${NP2_GAMES:-}" ] && [ -d "${NP2_GAMES:-}" ]; then
		for g in "$NP2_GAMES"/*/; do
			name=$(basename "$g" | tr -c 'A-Za-z0-9._\n-' '_'); dg=$(folder "game-$name"); cp "$g"/* "$dg/" 2>/dev/null
			# a folder without a slots file gets one from its files' kinds,
			# in name order (file_slots.json)
			[ -f "$dg/slots" ] || python3 - "$dg" <<'PY'
import json, os, sys
d = sys.argv[1]
kinds = {"floppy": ("d88", "88d", "d98", "98d", "fdi", "hdm", "xdf", "dup", "2hd", "tfd", "nfd", "dcp", "dcu"),
         "hdd": ("hdi", "nhd", "thd", "vhd"), "cdrom": ("cue", "iso")}
slots = {k: sorted(f for f in os.listdir(d) if f.lower().rsplit(".", 1)[-1] in exts) for k, exts in kinds.items()}
slots["hdd"] = slots["hdd"][:2]; slots["cdrom"] = slots["cdrom"][:1]
json.dump({k: v for k, v in slots.items() if v}, open(os.path.join(d, "slots"), "w"))
PY
				compare "game-$name" "$dg"
			# the 286 machine too, for a game it can run: no CD-ROM drive
			if ! grep -q cdrom "$dg/slots"; then
				d9=$(folder "game286-$name"); cp "$dg"/* "$d9/"; to286 "$d9"
				vs=$native286 compare "game286-$name" "$d9"
			fi
		done
	else
		skip "games: no NP2_GAMES folder (local content, never committed)"
	fi

	# ---- leg: named games, boot to play, local only -------------------------
	# Each tests/games/*.json names a game by its sha1, found anywhere under
	# NP2_GAMES, the keys that take it from boot into play, and what the
	# screen must then show. Native and sandbox must run the same, with
	# the same memory at the end; the control drops the last presses, and
	# the screen check must then fail.
	for conf in "$root"/tests/games/*.json; do
		title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["title"])' "$conf")
		eval "$(python3 - "$conf" <<'PY2'
import json, shlex, sys
c = json.load(open(sys.argv[1]))
press = c["press"].split(",")
ctl = ",".join(p for p in press if int(p.split(":")[0]) < c["before"])
for k, v in (("g_sha1", c["sha1"]), ("g_ext", c["file"].rsplit(".", 1)[-1]), ("g_slot", c["slot"]),
             ("g_press", c["press"]), ("g_ctl", ctl), ("g_before", c["before"]), ("g_frames", c["frames"])):
    print("%s=%s" % (k, shlex.quote(str(v))))
PY2
)"
		file=
		if [ -n "${NP2_GAMES:-}" ] && [ -d "${NP2_GAMES:-}" ]; then
			file=$(find -L "$NP2_GAMES" -type f -iname "*.$g_ext" -exec sha1sum {} + 2>/dev/null |
				awk -v s="$g_sha1" '$1 == s { sub(/^[0-9a-f]+  /, ""); print; exit }')
		fi
		if [ -z "$file" ]; then
			skip "$title: not found in NP2_GAMES by sha1 (local content, never committed)"; continue
		fi
		lbl=named-$(basename "$conf" .json); dn=$(folder "$lbl")
		cp "$file" "$dn/game.$g_ext"; echo "{\"$g_slot\":[\"game.$g_ext\"]}" > "$dn/slots"
		NP2_PRESS=$g_press "$native" "$dn" "$g_before" "$work/$lbl.before.ppm" "$work/$lbl.before.mem" > /dev/null 2>&1
		NP2_PRESS=$g_press "$native" "$dn" "$g_frames" "$work/$lbl.n.ppm" "$work/$lbl.n.mem" 2>/dev/null | lines > "$work/$lbl.n"
		NP2_PRESS=$g_press "$runwbx" "$wbx" "$dn" "$g_frames" /dev/null "$work/$lbl.w.mem" 2>/dev/null | lines > "$work/$lbl.w"
		NP2_PRESS=$g_ctl "$native" "$dn" "$g_frames" "$work/$lbl.c.ppm" "$work/$lbl.c.mem" > /dev/null 2>&1
		python3 "$root/tests/games/check-game.py" "$conf" "$work/$lbl.before.mem" "$work/$lbl.n.mem" "$work/$lbl.before.ppm" "$work/$lbl.n.ppm" > "$work/$lbl.check"
		real=$?
		python3 "$root/tests/games/check-game.py" "$conf" "$work/$lbl.before.mem" "$work/$lbl.c.mem" "$work/$lbl.before.ppm" "$work/$lbl.c.ppm" > "$work/$lbl.ctl" 2>&1
		ctl=$?
		n=$(wc -l < "$work/$lbl.n")
		if [ "$n" -eq "$g_frames" ] && cmp -s "$work/$lbl.n" "$work/$lbl.w" && cmp -s "$work/$lbl.n.mem" "$work/$lbl.w.mem" &&
			[ "$real" -eq 0 ] && [ "$ctl" -ne 0 ]; then
			ok "$title: boots into play by frame $g_frames, native and sandbox the same (frames and memory); $(grep -c '^ok' "$work/$lbl.check") screen checks hold, the control without the last keys fails"
		else
			bad "$title: frames $n of $g_frames, check $real, control $ctl (see $work/$lbl.*)"
		fi
		rm -f "${work:?}/$lbl".*.mem "${work:?}/$lbl".*.ppm "${dn:?}/game.$g_ext"	# game content: not kept
	done
fi

echo "gate: $pass passed, $fail failed, $skip skipped"
[ "$fail" -eq 0 ]
