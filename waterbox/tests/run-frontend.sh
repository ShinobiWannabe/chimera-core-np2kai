#!/bin/bash
# run-frontend.sh - the frontend half of the gate: the NP2kai package through
# Chimera's own engine (chimera-run) and through the real frontend
# (Chimera.exe, headless), each held to the native reference: the Main Memory
# domain after the same frames with nothing pressed, byte for byte.
#
# Legs:
#   engine:file        a disk opened with no project (mounted as "rom" and
#                      under its own name) boots the IA-32 machine
#   engine:machine     the machine setting PC98 boots the 286 half, which is
#                      the 286 reference and not the IA-32 one
#   engine:project     a .chimeraProject with the disk in the floppy slot and
#                      the font pinned by hash reaches the same machine
#   engine:savedata    the sector the test disk writes leaves as save data;
#                      the control disk, which writes nothing, leaves none
#   frontend:boot      Chimera.exe runs the package to the same machine
#   frontend:read      a Lua script reads a known address of Main Memory and
#                      gets what the native run left there
#   frontend:keybinds  the package's bindings become the frontend's defaults
#   lua:<check>        frontend-tools.lua on the counter disk: the memory
#                      domains, their views of Main Memory, a search for the
#                      frame counter, pokes the program sees, registers,
#                      keys from joypad.set, memory states and frame events
#                      (what RAM Search, RAM Watch and scripts stand on);
#                      lua:callbacks is said as INFO, not required
#   movie:record:<g>   a named game (tests/games/<g>.json, found by sha1 in
#                      NP2_GAMES) recorded through chimera-run from its key
#                      presses reaches the native run's memory
#   movie:replay:<g>   that recorded movie, replayed in a new process, reaches
#                      the same memory, and the game's screen checks hold
#
# Runs on Windows (Git Bash, Chimera.exe and build/dll/chimera-run.exe) and
# on Linux (Mono on a private Xvfb display, build/meson-linux/chimera-run).
#
# The native reference is made by run-native (Linux), with --make-reference,
# into build/frontend-ref; a Windows machine is given that folder. It holds
# the SHA-256 of each machine's Main Memory and that memory's first 768 KB
# (for saying where a run went wrong), never the whole domain: from C0000h
# up it holds the BIOS and the other ROMs, and its second MB the font, all
# the user's firmware.
#
# Usage: run-frontend.sh [--chimera-root <path>] [--frames N] [--make-reference]
# Environment:
#   NP2_FIRMWARE    a folder holding the required firmware, the user's own,
#                   never committed: FONT.ROM, BIOS.ROM, ITF.ROM, SOUND.ROM,
#                   IDE.ROM (or IDEBIOS.ROM) and the six 2608_*.WAV, any case
#   NP2_FONT        FONT.ROM to use instead of the folder's (optional)
#   NP2_REFERENCE   the reference folder (default build/frontend-ref)
#   NP2_GAMES       a folder the named games are looked for in, by sha1
#                   (local content, never committed); without it the movie
#                   legs SKIP
# Exit status: 0 when every leg passed.
set -u

here="$(cd "$(dirname "$0")" && pwd)"
wb="$(cd "$here/.." && pwd)"
root="$(cd "$wb/.." && pwd)"
frames=600
chimera_root=""
make_ref=0
while [ $# -gt 0 ]; do
	case "$1" in
		--chimera-root) chimera_root="$2"; shift ;;
		--frames) frames="$2"; shift ;;
		--make-reference) make_ref=1 ;;
		*) echo "unknown argument: $1" >&2; exit 2 ;;
	esac
	shift
done

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) windows=1 ;;
	*) windows=0 ;;
esac
# a path as the programs see it: Windows programs take Windows paths
native_path() { if [ "$windows" = 1 ] && command -v cygpath >/dev/null; then cygpath -w "$1"; else printf '%s' "$1"; fi; }
PY=""
for p in python3 python py; do
	if command -v "$p" >/dev/null && "$p" -c 'import sys; sys.exit(sys.version_info < (3, 6))' 2>/dev/null; then PY=$p; break; fi
done
[ -n "$PY" ] || { echo "python 3 not found" >&2; exit 1; }

# the required firmware (waterbox.config "firmware"), each under its id;
# IDE.ROM is only needed with a hard disk, and is given to every run
FIRMWARE_IDS="bios.rom itf.rom sound.rom ide.rom 2608_BD.WAV 2608_SD.WAV 2608_TOP.WAV 2608_HH.WAV 2608_TOM.WAV 2608_RIM.WAV"
fw_file() {	# fw_file <id>: the file in NP2_FIRMWARE, any case (IDE.ROM may be IDEBIOS.ROM)
	local n f
	for n in "$1" $( [ "$1" = ide.rom ] && echo idebios.rom ); do
		f=$(find "${NP2_FIRMWARE:-/nonexistent}" -maxdepth 1 -type f -iname "$n" 2>/dev/null | head -1)
		[ -n "$f" ] && { echo "$f"; return 0; }
	done
	return 1
}
font=${NP2_FONT:-$(fw_file FONT.ROM)}
[ -n "$font" ] && [ -f "$font" ] || { echo "NP2_FIRMWARE must hold FONT.ROM (required firmware, never committed)" >&2; exit 1; }
for id in $FIRMWARE_IDS; do
	fw_file "$id" > /dev/null || { echo "NP2_FIRMWARE has no $id (required firmware, never committed)" >&2; exit 1; }
done
fw_copy() {	# fw_copy <dir>: the firmware into a native run's folder
	local id
	cp "$font" "$1/FONT.ROM"
	for id in $FIRMWARE_IDS; do cp "$(fw_file "$id")" "$1/$id"; done
}
ref=${NP2_REFERENCE:-$root/build/frontend-ref}
work="$root/build/frontend"
mkdir -p "$work"

# ---- the test disks: generated, free content ------------------------------
# test.hdm writes its own boot sector to sector 2 (tests/gen-testdisk.py);
# nowrite.hdm is the same disk with the write taken out, the control;
# counter.hdm counts frames and keys, for the tools legs.
"$PY" "$root/tests/gen-testdisk.py" "$work/test.hdm" > /dev/null || { echo "could not make the test disk" >&2; exit 1; }
"$PY" "$root/tests/gen-testdisk.py" --no-write "$work/nowrite.hdm" > /dev/null || { echo "could not make the control disk" >&2; exit 1; }
"$PY" "$root/tests/gen-testdisk.py" --counter "$work/counter.hdm" > /dev/null || { echo "could not make the counter disk" >&2; exit 1; }

# ---- the native reference --------------------------------------------------
# The disk as a file opened with no project: the folder holds it under its
# own name with that name in rom.name, and no slots file, as the engine
# mounts it.
reference() {	# reference <flavour> <settings json>
	local f=$1 d="$work/ref-$1"
	local rn="$root/build/native-$f/run-native"
	rm -rf "$d"; mkdir -p "$d"
	fw_copy "$d"; cp "$work/test.hdm" "$d/test.hdm"; cp "$work/test.hdm" "$d/rom"
	printf '/test.hdm' > "$d/rom.name"; printf '%s' "$2" > "$d/settings"
	"$rn" "$d" "$frames" /dev/null "$work/ref-$f.mem.bin" > /dev/null 2>"$work/ref-$f.err" || return 1
	"$PY" - "$work/ref-$f.mem.bin" "$ref/$f" <<'PY'
import hashlib, sys
data = open(sys.argv[1], "rb").read()
open(sys.argv[2] + ".sha256", "w").write(hashlib.sha256(data).hexdigest().upper() + "\n")
open(sys.argv[2] + ".low.bin", "wb").write(data[:0xC0000])	# below the ROMs
PY
}
# a named game's file under NP2_GAMES, by its sha1 (empty when absent)
game_file() {	# game_file <game.json>
	[ -n "${NP2_GAMES:-}" ] && [ -d "${NP2_GAMES:-}" ] || return 0
	"$PY" - "$1" "$NP2_GAMES" <<'PY'
import hashlib, json, os, sys
c = json.load(open(sys.argv[1])); ext = "." + c["file"].rsplit(".", 1)[-1].lower()
for d, _, names in os.walk(sys.argv[2], followlinks=True):
    for n in sorted(names):
        p = os.path.join(d, n)
        try:
            if n.lower().endswith(ext) and hashlib.sha1(open(p, "rb").read()).hexdigest() == c["sha1"]:
                sys.exit(print(p))
        except OSError:
            pass  # a file another program holds open: not this game
PY
}
conf_get() { "$PY" -c 'import json,sys; print(json.load(open(sys.argv[1]))[sys.argv[2]])' "$1" "$2"; }
# the native run of a named game: its key presses and frames, and the hash
# of Main Memory after them (only the hash: the memory holds game and font)
game_reference() {	# game_reference <game.json> <file>
	local g d ext
	g=$(basename "$1" .json); d="${work:?}/ref-game-$g"; ext=$(conf_get "$1" file | sed 's/.*\.//')
	rm -rf "$d"; mkdir -p "$d"
	fw_copy "$d"; cp "$2" "$d/game.$ext"
	printf '{"%s":["game.%s"]}' "$(conf_get "$1" slot)" "$ext" > "$d/slots"
	NP2_PRESS=$(conf_get "$1" press) "$root/build/native-np21/run-native" "$d" "$(conf_get "$1" frames)" /dev/null "$d/mem.bin" > /dev/null 2>&1 || return 1
	"$PY" -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest().upper())' "$d/mem.bin" > "$ref/game-$g.sha256"
	rm -rf "$d"
	echo "game $g: reference written"
}
if [ "$make_ref" = 1 ] || { [ ! -f "$ref/info.txt" ] && [ -x "$root/build/native-np21/run-native" ]; }; then
	[ -x "$root/build/native-np21/run-native" ] && [ -x "$root/build/native-np2/run-native" ] ||
		{ echo "the native builds are needed for the reference (build-native.sh np21, np2)" >&2; exit 1; }
	mkdir -p "$ref"
	reference np21 '{}' && reference np2 '{"machine":"PC98"}' || { echo "the native reference failed" >&2; exit 1; }
	{
		echo "frames=$frames"
		echo "testdisk=$("$PY" -c 'import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],"rb").read()).hexdigest())' "$work/test.hdm")"
		echo "font=$("$PY" -c 'import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],"rb").read()).hexdigest())' "$font")"
		for id in $FIRMWARE_IDS; do
			echo "$id=$("$PY" -c 'import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],"rb").read()).hexdigest())' "$(fw_file "$id")")"
		done
	} > "$ref/info.txt"
	echo "reference written to $ref ($frames frames)"
	for c in "$root"/tests/games/*.json; do
		f=$(game_file "$c")
		if [ -n "$f" ]; then game_reference "$c" "$f" || { echo "the native reference for $c failed" >&2; exit 1; }
		else echo "game $(basename "$c" .json): not in NP2_GAMES, no reference"; fi
	done
	[ "$make_ref" = 1 ] && exit 0
fi
[ -f "$ref/info.txt" ] || { echo "no reference at $ref (make it with --make-reference where the native builds are)" >&2; exit 1; }
rframes=$(sed -n 's/^frames=//p' "$ref/info.txt" | tr -d '\r')
[ "$rframes" = "$frames" ] || { echo "the reference is for $rframes frames; run with --frames $rframes" >&2; exit 1; }
sha1() { "$PY" -c 'import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],"rb").read()).hexdigest())' "$1"; }
[ "$(sed -n 's/^testdisk=//p' "$ref/info.txt" | tr -d '\r')" = "$(sha1 "$work/test.hdm")" ] ||
	{ echo "the reference was made from another test disk" >&2; exit 1; }

# ---- Chimera ----------------------------------------------------------------
if [ -z "$chimera_root" ]; then
	for c in "$root/../chimera-main" "$root/../chimera"; do
		[ -d "$c/source/gui" ] && { chimera_root="$c"; break; }
	done
fi
[ -n "$chimera_root" ] && [ -d "$chimera_root" ] || { echo "Chimera checkout not found; pass --chimera-root" >&2; exit 1; }
chimera_root="$(cd "$chimera_root" && pwd)"
package=$(ls "$root"/build/package/np2kai-*.chimeraCore 2>/dev/null | head -1)
[ -f "$package" ] || { echo "no package in $root/build/package (waterbox/build-package.sh)" >&2; exit 1; }
if [ "$windows" = 1 ]; then
	crun="$chimera_root/build/dll/chimera-run.exe"
	emu="$chimera_root/build/Chimera.exe"
else
	crun="$chimera_root/build/meson-linux/chimera-run"
	[ -x "$crun" ] || crun="$chimera_root/build/dll/chimera-run"
	emu="$chimera_root/build/Chimera.exe"
	export LD_LIBRARY_PATH="$chimera_root/build/dll:$chimera_root/build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

ok=0; failed=0; skipped=0
report() {
	printf "%-20s %-5s %s\n" "$1" "$2" "$3"
	case "$2" in PASS) ok=$((ok + 1)) ;; SKIP) skipped=$((skipped + 1)) ;; *) failed=$((failed + 1)) ;; esac
}
printf "%-20s %-5s %s\n" "Leg" "Result" "Detail"
sha256() { "$PY" -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest().upper())' "$1"; }
refhash() { tr -d '\r\n' < "$ref/$1.sha256"; }
same_as() {	# same_as <dump> <flavour>: the dump is the reference's bytes
	[ -s "$1" ] && [ "$(sha256 "$1")" = "$(refhash "$2")" ]
}
where() {	# where <dump> <flavour>: the first byte below the ROMs that differs
	"$PY" - "$1" "$ref/$2.low.bin" <<'PY'
import sys
try:
    a = open(sys.argv[1], "rb").read()[:0xC0000]
except OSError:
    sys.exit(print("no dump"))
b = open(sys.argv[2], "rb").read()
d = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), None)
print("first difference at 0x%X" % d if d is not None else "the memory below C0000h is the same, so the difference is above it")
PY
}

# ---- the engine: chimera-run ----------------------------------------------
# An empty movie and --record: the run is that many frames with nothing
# pressed (chimera-run's own advice for a fixed-length idle run).
printf '[Input]\n[/Input]\n' > "$work/empty.txt"
FONTW=$(native_path "$font")
FWARGS=(--firmware "FONT.ROM=$FONTW")
for id in $FIRMWARE_IDS; do FWARGS+=(--firmware "$id=$(native_path "$(fw_file "$id")")"); done
engine() {	# engine <tag> <disk> [chimera-run flags...]
	local tag=$1 disk=$2; shift 2
	rm -f "$work/$tag.mem.bin"
	"$crun" "$(native_path "$package")" "$(native_path "$disk")" "$(native_path "$work/empty.txt")" \
		--record "$(native_path "$work/$tag.rec.txt")" --frames "$frames" \
		"${FWARGS[@]}" --dump "Main Memory=$(native_path "$work/$tag.mem.bin")" "$@" \
		> "$work/$tag.log" 2>&1
}
if [ ! -x "$crun" ] && [ ! -f "$crun" ]; then
	for leg in engine:file engine:machine engine:project engine:savedata; do
		report "$leg" FAIL "chimera-run not built: $crun"
	done
else
	engine file "$work/test.hdm"
	if same_as "$work/file.mem.bin" np21; then report engine:file PASS "$frames frames, Main Memory the same as the native reference"
	else report engine:file FAIL "Main Memory differs, $(where "$work/file.mem.bin" np21) (see build/frontend/file.log)"; fi

	engine machine "$work/test.hdm" --settings '{"machine":"PC98"}'
	if same_as "$work/machine.mem.bin" np2 && ! cmp -s "$work/machine.mem.bin" "$work/file.mem.bin"; then
		report engine:machine PASS "machine PC98 is the 286 reference, not the IA-32 run"
	else report engine:machine FAIL "the PC98 run is not the 286 reference, $(where "$work/machine.mem.bin" np2) (see build/frontend/machine.log)"; fi

	# a project: the disk in the floppy slot, the firmware pinned by its hashes
	pdir="$work/project"; rm -rf "$pdir"; mkdir -p "$pdir"; cp "$work/test.hdm" "$pdir/"
	fwlist="$work/project-firmware.txt"; : > "$fwlist"
	for id in $FIRMWARE_IDS; do [ "$id" = ide.rom ] || printf '%s\t%s\n' "$id" "$(native_path "$(fw_file "$id")")" >> "$fwlist"; done
	"$PY" - "$pdir/test.hdm" "$font" "$pdir/p.chimeraProject" "$fwlist" <<'PY'
import hashlib, json, sys
sha1 = lambda p: hashlib.sha1(open(p, "rb").read()).hexdigest().upper()
# every firmware the floppy project needs (IDE.ROM only goes with a hard disk)
more = [{"id": i, "sha1": sha1(p)} for i, p in (l.rstrip("\n").split("\t", 1) for l in open(sys.argv[4]) if l.strip())]
json.dump({
    "title": "np2kai frontend gate",
    "core": {"name": "NP2kai", "version": "", "sha1": ""},
    "headers": {"MovieVersion": "Chimera Project File v1.1", "Platform": "PC9821"},
    "files": [{"name": "test.hdm", "sha1": sha1(sys.argv[1]), "slot": "floppy"}],
    "firmware": [{"id": "FONT.ROM", "sha1": sha1(sys.argv[2])}] + more,
    "settings": {},
    "input": "[Input]\n[/Input]\n",
}, open(sys.argv[3], "w"))
PY
	rm -f "$work/project.mem.bin"
	"$crun" --project "$(native_path "$pdir/p.chimeraProject")" "$(native_path "$package")" --allow-core-mismatch \
		--record "$(native_path "$work/project.rec.txt")" --frames "$frames" \
		"${FWARGS[@]}" --dump "Main Memory=$(native_path "$work/project.mem.bin")" \
		> "$work/project.log" 2>&1
	if same_as "$work/project.mem.bin" np21; then report engine:project PASS "the project's floppy slot reaches the reference machine"
	else report engine:project FAIL "Main Memory differs, $(where "$work/project.mem.bin" np21) (see build/frontend/project.log)"; fi

	rm -rf "$work/sd" "$work/sdn"; mkdir -p "$work/sd" "$work/sdn"
	engine savedata "$work/test.hdm" --export-savedata "$(native_path "$work/sd")"
	engine savenone "$work/nowrite.hdm" --export-savedata "$(native_path "$work/sdn")"
	if "$PY" - "$work/test.hdm" "$work/sd/test.hdm" <<'PY'
import sys
a = open(sys.argv[1], "rb").read(); b = open(sys.argv[2], "rb").read()
changed = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
sys.exit(0 if len(a) == len(b) and b[1024:2048] == a[0:1024] and changed and changed[0] >= 1024 and changed[-1] < 2048 else 1)
PY
	then
		if [ -z "$(ls -A "$work/sdn")" ]; then report engine:savedata PASS "the written sector left as save data; the control disk left none"
		else report engine:savedata FAIL "the control disk, which writes nothing, was exported"; fi
	else report engine:savedata FAIL "the exported disk is not what the machine wrote (see build/frontend/savedata.log)"; fi
fi

# ---- movies: a named game recorded from its key presses, then replayed ----
gm() {	# gm <tag> <disk> <movie> [flags...]: one chimera-run process
	local tag=$1 disk=$2 movie=$3; shift 3
	rm -f "${work:?}/$tag.mem.bin"
	"$crun" "$(native_path "$package")" "$(native_path "$disk")" "$(native_path "$movie")" \
		"${FWARGS[@]}" --dump "Main Memory=$(native_path "$work/$tag.mem.bin")" "$@" > "$work/$tag.log" 2>&1
}
for c in "$root"/tests/games/*.json; do
	g=$(basename "$c" .json)
	f=$(game_file "$c")
	if [ -z "$f" ]; then
		report "movie:record:$g" SKIP "not found in NP2_GAMES by sha1 (local content, never committed)"
		report "movie:replay:$g" SKIP "not found"; continue
	fi
	if [ ! -f "$ref/game-$g.sha256" ]; then
		report "movie:record:$g" SKIP "no native reference (--make-reference with NP2_GAMES where the native builds are)"
		report "movie:replay:$g" SKIP "no native reference"; continue
	fi
	if [ ! -x "$crun" ] && [ ! -f "$crun" ]; then
		report "movie:record:$g" FAIL "chimera-run not built"; report "movie:replay:$g" FAIL "chimera-run not built"; continue
	fi
	gframes=$(conf_get "$c" frames); before=$(conf_get "$c" before)
	"$PY" "$root/tests/games/game-movie.py" "$c" "$work/$g.input.txt"
	want=$(refhash "game-$g")
	rm -f "${work:?}/$g.rec.txt"
	gm "$g-rec" "$f" "$work/$g.input.txt" --record "$(native_path "$work/$g.rec.txt")" --frames "$gframes"
	entries=$(grep -c '^|' "$work/$g.rec.txt" 2>/dev/null)
	if [ -s "$work/$g-rec.mem.bin" ] && [ "$(sha256 "$work/$g-rec.mem.bin")" = "$want" ] && [ "${entries:-0}" -eq "$gframes" ]; then
		report "movie:record:$g" PASS "$gframes frames recorded from its key presses, Main Memory the same as the native run"
	else
		report "movie:record:$g" FAIL "memory or movie not as the native run (${entries:-0} entries; see build/frontend/$g-rec.log)"
	fi
	# a new process for each: the movie alone takes the machine there
	gm "$g-play" "$f" "$work/$g.rec.txt" --final-screenshot "$(native_path "$work/$g-play.tga")"
	gm "$g-before" "$f" "$work/$g.rec.txt" --frames "$before" --final-screenshot "$(native_path "$work/$g-before.tga")"
	if [ -s "$work/$g-play.mem.bin" ] && [ "$(sha256 "$work/$g-play.mem.bin")" = "$want" ] &&
		"$PY" "$root/tests/games/check-game.py" "$c" "$work/$g-before.mem.bin" "$work/$g-play.mem.bin" "$work/$g-before.tga" "$work/$g-play.tga" > "$work/$g.check.txt" 2>&1; then
		report "movie:replay:$g" PASS "the recorded movie replayed in a new process: the same memory, $(grep -c '^ok' "$work/$g.check.txt") screen checks hold"
	else
		report "movie:replay:$g" FAIL "$(grep -v '^ok' "$work/$g.check.txt" 2>/dev/null | head -1) (see build/frontend/$g-play.log)"
	fi
	rm -f "${work:?}/$g-rec.mem.bin" "${work:?}/$g-play.mem.bin" "${work:?}/$g-before.mem.bin" "${work:?}/$g-play.tga" "${work:?}/$g-before.tga"	# the game's memory and pictures: not kept
done

# ---- the frontend: Chimera.exe, headless ------------------------------------
xvfb_pid=""
cleanup() { [ -n "$xvfb_pid" ] && kill "$xvfb_pid" 2>/dev/null; }
trap cleanup EXIT
chimera() {	# chimera <log> <args...>
	local log=$1; shift
	if [ "$windows" = 1 ]; then
		( cd "$chimera_root/build" && timeout 900 ./Chimera.exe "$@" ) > "$log" 2>&1
	else
		( cd "$chimera_root" && MONO_CRASH_NOFILE=1 MONO_WINFORMS_XIM_STYLE=disabled ALSOFT_DRIVERS=null \
			timeout 900 mono "$emu" "$@" ) > "$log" 2>&1
	fi
}
if [ ! -f "$emu" ]; then
	report frontend:boot FAIL "Chimera not built: $emu (dotnet build source/gui/Chimera.sln -c Release)"
	report frontend:read FAIL "Chimera not built"
	report frontend:keybinds FAIL "Chimera not built"
	report lua:script FAIL "Chimera not built"
else
	if [ "$windows" = 0 ] && [ -z "${DISPLAY:-}" ]; then
		for n in 90 91 92 93 94 95 96; do
			if [ ! -e "/tmp/.X11-unix/X$n" ]; then
				Xvfb ":$n" -screen 0 640x480x24 -nolisten tcp & xvfb_pid=$!
				export DISPLAY=":$n"; break
			fi
		done
		sleep 1
	fi
	config="$work/config.ini"
	if [ ! -f "$config" ]; then
		chimera "$work/bootstrap.log" --headless "--config=$(native_path "$config")" "--lua=$(native_path "$here/exit.lua")"
		[ -f "$config" ] || echo "the config bootstrap wrote nothing (see build/frontend/bootstrap.log)"
	fi
	run_frontend() {	# run_frontend <tag> <config> <frames>
		local tag=$1 cfg=$2 n=$3 job="$work/job.$1.txt"
		printf 'frames=%s\nmeta=%s\nshot=%s\n' "$n" "$(native_path "$work/$tag.meta.txt")" "$(native_path "$work/$tag.png")" > "$job"
		rm -f "$work/$tag.meta.txt"
		NP2_JOB="$(native_path "$job")" chimera "$work/$tag.log" --headless "--config=$(native_path "$cfg")" \
			"--core=$(native_path "$package")" "${FWARGS[@]}" \
			"--lua=$(native_path "$here/frontend-ram.lua")" "$(native_path "$work/test.hdm")"
		[ -f "$work/$tag.meta.txt" ] && grep -q '^status=OK' "$work/$tag.meta.txt"
	}
	want=$(refhash np21)
	if [ ! -f "$config" ]; then
		report frontend:boot FAIL "no config (see build/frontend/bootstrap.log)"
		report frontend:read FAIL "no config"
		report frontend:keybinds FAIL "no config"
		report lua:script FAIL "no config"
	else
		"$PY" "$here/config-edit.py" "$config" "$work/config.boot.ini"
		if run_frontend boot "$work/config.boot.ini" "$frames"; then
			got=$(sed -n 's/^ramhash=//p' "$work/boot.meta.txt" | tr -d '\r' | tr a-f A-F)
			if [ "$got" = "$want" ]; then report frontend:boot PASS "$frames frames in Chimera, Main Memory the same as the native reference"
			else report frontend:boot FAIL "Main Memory differs (got $got, want $want)"; fi
		else
			report frontend:boot FAIL "no OK result: $(sed -n 's/^detail=//p' "$work/boot.meta.txt" 2>/dev/null) (see build/frontend/boot.log)"
		fi
		# what the test disk's program left at 1FC1Dh: the BIOS's answer, 0, then
		# its own name, as the native run left it
		probe=$(sed -n 's/^probe=//p' "$work/boot.meta.txt" 2>/dev/null | tr -d '\r')
		native=$("$PY" -c 'import sys; print(open(sys.argv[1], "rb").read()[0x1FC1D:0x1FC2D].hex().upper())' "$ref/np21.low.bin")
		if [ -n "$probe" ] && [ "$probe" = "$native" ] && [ "${native#00434849}" != "$native" ]; then
			report frontend:read PASS "memory.readbyte at 1FC1Dh: BIOS answer 0 and the disk's name, as the native run left them"
		else report frontend:read FAIL "read '$probe', native '$native'"; fi
		"$PY" "$here/config-edit.py" "$config" "$work/config.keys.ini" --forget "PC-98 Controller"
		if run_frontend keys "$work/config.keys.ini" 1; then
			if "$PY" "$here/check-keybinds.py" "$work/config.keys.ini" "$wb/default_keybinds.json" "PC-98 Controller" > "$work/keys.txt" 2>&1
			then report frontend:keybinds PASS "$(cat "$work/keys.txt")"
			else report frontend:keybinds FAIL "$(head -1 "$work/keys.txt")"; fi
		else
			report frontend:keybinds FAIL "no OK result (see build/frontend/keys.log)"
		fi
		# the tools: one Lua run on the counter disk, a leg for each check it makes
		job="$work/job.tools.txt"; rm -f "$work/tools.meta.txt"
		printf 'meta=%s\n' "$(native_path "$work/tools.meta.txt")" > "$job"
		NP2_JOB="$(native_path "$job")" chimera "$work/tools.log" --headless "--config=$(native_path "$work/config.boot.ini")" \
			"--core=$(native_path "$package")" "${FWARGS[@]}" \
			"--lua=$(native_path "$here/frontend-tools.lua")" "$(native_path "$work/counter.hdm")"
		if [ -f "$work/tools.meta.txt" ]; then
			while IFS= read -r line; do
				line=${line%$'\r'}
				name=${line%%=*}; rest=${line#*=}; word=${rest%% *}; detail=${rest#* }
				if [ "$name" = status ]; then
					[ "$word" = OK ] || report lua:script FAIL "stopped: $detail (see build/frontend/tools.log)"
				elif [ "$word" = INFO ]; then printf "%-20s %-5s %s\n" "lua:$name" INFO "$detail"
				else report "lua:$name" "$word" "$detail"; fi
			done < "$work/tools.meta.txt"
			grep -q '^status=' "$work/tools.meta.txt" || report lua:script FAIL "the script did not finish (see build/frontend/tools.log)"
		else
			report lua:script FAIL "no result (see build/frontend/tools.log)"
		fi
	fi
fi

echo
echo "$ok passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
