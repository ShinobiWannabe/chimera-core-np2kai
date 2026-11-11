#!/bin/bash
# run-frontend.sh - the frontend half of the gate: the NP2kai package through
# Chimera's own engine (chimera-run) and through Chimera's window (run
# headless), each held to the native reference: the Main Memory domain after
# the same frames with nothing pressed, byte for byte.
#
# A game runs only in a project (Chimera, 2026-10-26): every run here opens
# a .chimeraProject made by chimera-run --new-project beside a copy of its
# disk, as the window's New Project makes one. A project is a zip of
# project.json and input.input (Chimera, 2026-11-02).
#
# Legs:
#   engine:file        a disk made into a project with nothing but the
#                      package's defaults boots the IA-32 machine
#   engine:machine     the machine setting PC98 boots the 286 half, which is
#                      the 286 reference and not the IA-32 one
#   engine:project     the disk named in the floppy slot, the firmware pinned
#                      by hash, reaches the same machine
#   engine:savedata    the sector the test disk writes leaves as save data;
#                      the control disk, which writes nothing, leaves none
#   frontend:boot      Chimera's window runs the package to the same machine
#   frontend:read      a Lua script reads a known address of Main Memory and
#                      gets what the native run left there
#   frontend:keybinds  the package's bindings are the window's when nobody
#                      changed them
#   lua:<check>        frontend-tools.lua on the counter disk: the memory
#                      domains, their views of Main Memory, the System Bus,
#                      a search for the frame counter, pokes the program
#                      sees, registers, keys from joypad.set, memory states
#                      and frame events (what RAM Search, RAM Watch and
#                      scripts stand on); lua:callbacks is said as INFO
#   mem21:<check>, mem2:<check>  frontend-memory.lua on the counter disk with
#                      the setting Lua Memory Events, on each machine: the
#                      scope, event.on_bus_write / on_bus_read / on_bus_exec /
#                      on_bus_exec_any at the frame counter, the registers
#                      inside a call, a write replaced by the script
#   memoff:refused     the same disk without the setting: no scopes, and
#                      on_bus_write is refused
#   movie:record:<g>   a named game (tests/games/<g>.json, found by sha1 in
#                      NP2_GAMES) recorded through chimera-run from its key
#                      presses reaches the native run's memory
#   movie:replay:<g>   that recorded movie, replayed in a new process, reaches
#                      the same memory, and the game's screen checks hold
#
# Chimera is a bundle (a nightly's download, or tools/build-bundle.sh's
# output: chimera[.exe] and bin/chimera-run[.exe]) or a Linux checkout built
# with meson (build/meson-linux). Runs on Windows (Git Bash) and Linux.
#
# The native reference is made by run-native (Linux), with --make-reference,
# into build/frontend-ref; a Windows machine is given that folder. It holds
# the SHA-256 of each machine's Main Memory and that memory's first 768 KB
# (for saying where a run went wrong), never the whole domain: from C0000h
# up it holds the BIOS and the other ROMs, and its second MB the font, all
# the user's firmware.
#
# Usage: run-frontend.sh [--chimera <bundle or checkout>] [--frames N] [--make-reference]
#        (--chimera-root is the same as --chimera)
# Environment:
#   CHIMERA         the bundle or checkout, when --chimera is not given
#                   (else ../chimera-main or ../chimera beside this repository)
#   NP2_FIRMWARE    a folder holding the required firmware, the user's own,
#                   never committed: FONT.ROM, BIOS.ROM, SOUND.ROM,
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
chimera_root="${CHIMERA:-}"
make_ref=0
while [ $# -gt 0 ]; do
	case "$1" in
		--chimera|--chimera-root) chimera_root="$2"; shift ;;
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
FIRMWARE_IDS="bios.rom sound.rom ide.rom 2608_bd.wav 2608_sd.wav 2608_top.wav 2608_hh.wav 2608_tom.wav 2608_rim.wav"
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
	cp "$font" "$1/font.rom"
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
		[ -d "$c" ] && { chimera_root="$c"; break; }
	done
fi
[ -n "$chimera_root" ] && [ -d "$chimera_root" ] || { echo "Chimera not found; pass --chimera <bundle or checkout>" >&2; exit 1; }
chimera_root="$(cd "$chimera_root" && pwd)"
x=""; [ "$windows" = 1 ] && x=".exe"
if [ -f "$chimera_root/bin/chimera-run$x" ]; then	# a bundle
	crun="$chimera_root/bin/chimera-run$x"; emu="$chimera_root/chimera$x"
else	# a checkout built with meson
	crun="$chimera_root/build/meson-linux/chimera-run"; emu="$chimera_root/build/meson-linux/source/ui/chimera"
fi
package=$(ls -t "$root"/build/package/np2kai-*.chimeraCore 2>/dev/null | head -1)
[ -f "$package" ] || { echo "no package in $root/build/package (waterbox/build-package.sh)" >&2; exit 1; }

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

# ---- projects ---------------------------------------------------------------
# Every run gives the firmware (a project pins it by hash and never holds
# it). A project pins what its disk needs: IDE.ROM only with a hard disk.
FWARGS=(--firmware "font.rom=$(native_path "$font")")
PINS=(--firmware "font.rom=$(native_path "$font")")
for id in $FIRMWARE_IDS; do
	FWARGS+=(--firmware "$id=$(native_path "$(fw_file "$id")")")
	[ "$id" = ide.rom ] || PINS+=(--firmware "$id=$(native_path "$(fw_file "$id")")")
done
# project <name> <disk> [chimera-run --new-project options...]: a project
# made beside a copy of the disk, in build/frontend/projects/<name>; prints
# its path, or nothing when chimera-run refused (its words in make.log there)
project() {
	local name=$1 disk=$2 d="$work/projects/$1"; shift 2
	rm -rf "$d"; mkdir -p "$d"; cp "$disk" "$d/"
	"$crun" --new-project "$(native_path "$d/$name.chimeraProject")" "$(native_path "$package")" \
		"$(native_path "$d/$(basename "$disk")")" "$@" > "$d/make.log" 2>&1 && printf '%s' "$d/$name.chimeraProject"
}
made() { echo "no project: $(tail -1 "$work/projects/$1/make.log" 2>/dev/null)"; }

# ---- the engine: chimera-run ----------------------------------------------
# A project with no input, run with --record for a number of frames: that
# many frames with nothing pressed (chimera-run's own advice for a
# fixed-length idle run).
engine() {	# engine <tag> <project> [chimera-run flags...]
	local tag=$1 proj=$2; shift 2
	rm -f "$work/$tag.mem.bin"
	[ -n "$proj" ] || return 1
	"$crun" --project "$(native_path "$proj")" "$(native_path "$package")" \
		--record "$(native_path "$work/$tag.rec.txt")" --frames "$frames" \
		"${FWARGS[@]}" --dump "Main Memory=$(native_path "$work/$tag.mem.bin")" "$@" \
		> "$work/$tag.log" 2>&1
}
if [ ! -f "$crun" ]; then
	for leg in engine:file engine:machine engine:project engine:savedata; do
		report "$leg" FAIL "chimera-run not found: $crun"
	done
else
	# the disk alone: its kind of file picks the slot and the machine, every
	# setting at the package's default
	p=$(project file "$work/test.hdm" "${PINS[@]}")
	if [ -z "$p" ]; then report engine:file FAIL "$(made file)"
	else
		engine file "$p"
		if same_as "$work/file.mem.bin" np21; then report engine:file PASS "$frames frames, Main Memory the same as the native reference"
		else report engine:file FAIL "Main Memory differs, $(where "$work/file.mem.bin" np21) (see build/frontend/file.log)"; fi
	fi

	# chimera-run --new-project takes each setting's first declaration's
	# default, whatever the machine; the window's New Project takes the one
	# whose "when" names the machine. So the PC98 multiplier's own default, 4,
	# is given as the window would give it.
	p=$(project machine "$work/test.hdm" "${PINS[@]}" --settings '{"machine":"PC98","clockMultiplier":"4"}')
	if [ -z "$p" ]; then report engine:machine FAIL "$(made machine)"
	else
		engine machine "$p"
		if same_as "$work/machine.mem.bin" np2 && ! cmp -s "$work/machine.mem.bin" "$work/file.mem.bin"; then
			report engine:machine PASS "machine PC98 is the 286 reference, not the IA-32 run"
		else report engine:machine FAIL "the PC98 run is not the 286 reference, $(where "$work/machine.mem.bin" np2) (see build/frontend/machine.log)"; fi
	fi

	# the disk named in the floppy slot, the firmware pinned by its hashes
	rm -rf "$work/projects/project"; mkdir -p "$work/projects/project"; cp "$work/test.hdm" "$work/projects/project/"
	p="$work/projects/project/p.chimeraProject"
	if "$crun" --new-project "$(native_path "$p")" "$(native_path "$package")" \
		--file "floppy=$(native_path "$work/projects/project/test.hdm")" "${PINS[@]}" --title "np2kai frontend gate" \
		> "$work/projects/project/make.log" 2>&1; then
		engine project "$p"
		if same_as "$work/project.mem.bin" np21; then report engine:project PASS "the project's floppy slot reaches the reference machine"
		else report engine:project FAIL "Main Memory differs, $(where "$work/project.mem.bin" np21) (see build/frontend/project.log)"; fi
	else report engine:project FAIL "$(made project)"; fi

	rm -rf "$work/sd" "$work/sdn"; mkdir -p "$work/sd" "$work/sdn"
	engine savedata "$(project savedata "$work/test.hdm" "${PINS[@]}")" --export-savedata "$(native_path "$work/sd")"
	engine savenone "$(project savenone "$work/nowrite.hdm" "${PINS[@]}")" --export-savedata "$(native_path "$work/sdn")"
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
# The game stays where it is: its project is made in build/frontend and finds
# it with --files, so no copy of a game is ever made.
gproject() {	# gproject <name> <game> <movie> <slot> [more pins...]: prints its path
	local name=$1 game=$2 movie=$3 slot=$4 d="$work/projects/$1"; shift 4
	rm -rf "$d"; mkdir -p "$d"
	"$crun" --new-project "$(native_path "$d/$name.chimeraProject")" "$(native_path "$package")" \
		--file "$slot=$(native_path "$game")" --movie "$(native_path "$movie")" "${PINS[@]}" "$@" \
		> "$d/make.log" 2>&1 && printf '%s' "$d/$name.chimeraProject"
}
gm() {	# gm <tag> <project> <game> [flags...]: one chimera-run process
	local tag=$1 proj=$2 game=$3; shift 3
	rm -f "${work:?}/$tag.mem.bin"
	[ -n "$proj" ] || return 1
	"$crun" --project "$(native_path "$proj")" "$(native_path "$package")" --files "$(native_path "$(dirname "$game")")" \
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
	if [ ! -f "$crun" ]; then
		report "movie:record:$g" FAIL "chimera-run not found"; report "movie:replay:$g" FAIL "chimera-run not found"; continue
	fi
	gframes=$(conf_get "$c" frames); before=$(conf_get "$c" before); slot=$(conf_get "$c" slot)
	gpins=(); [ "$slot" = hdd ] && gpins=(--firmware "ide.rom=$(native_path "$(fw_file ide.rom)")")
	"$PY" "$root/tests/games/game-movie.py" "$c" "$work/$g.input.txt"
	want=$(refhash "game-$g")
	rm -f "${work:?}/$g.rec.txt"
	p=$(gproject "$g-rec" "$f" "$work/$g.input.txt" "$slot" "${gpins[@]}")
	gm "$g-rec" "$p" "$f" --record "$(native_path "$work/$g.rec.txt")" --frames "$gframes"
	entries=$(grep -c '^|' "$work/$g.rec.txt" 2>/dev/null)
	if [ -s "$work/$g-rec.mem.bin" ] && [ "$(sha256 "$work/$g-rec.mem.bin")" = "$want" ] && [ "${entries:-0}" -eq "$gframes" ]; then
		report "movie:record:$g" PASS "$gframes frames recorded from its key presses, Main Memory the same as the native run"
	else
		[ -n "$p" ] || echo "$(made "$g-rec")" >> "$work/$g-rec.log"
		report "movie:record:$g" FAIL "memory or movie not as the native run (${entries:-0} entries; see build/frontend/$g-rec.log)"
	fi
	# a new process for each, from a project of the recorded movie: the movie
	# alone takes the machine there
	p=$(gproject "$g-play" "$f" "$work/$g.rec.txt" "$slot" "${gpins[@]}")
	gm "$g-play" "$p" "$f" --final-screenshot "$(native_path "$work/$g-play.tga")"
	gm "$g-before" "$p" "$f" --frames "$before" --final-screenshot "$(native_path "$work/$g-before.tga")"
	if [ -s "$work/$g-play.mem.bin" ] && [ "$(sha256 "$work/$g-play.mem.bin")" = "$want" ] &&
		"$PY" "$root/tests/games/check-game.py" "$c" "$work/$g-before.mem.bin" "$work/$g-play.mem.bin" "$work/$g-before.tga" "$work/$g-play.tga" > "$work/$g.check.txt" 2>&1; then
		report "movie:replay:$g" PASS "the recorded movie replayed in a new process: the same memory, $(grep -c '^ok' "$work/$g.check.txt") screen checks hold"
	else
		report "movie:replay:$g" FAIL "$(grep -v '^ok' "$work/$g.check.txt" 2>/dev/null | head -1) (see build/frontend/$g-play.log)"
	fi
	rm -f "${work:?}/$g-rec.mem.bin" "${work:?}/$g-play.mem.bin" "${work:?}/$g-before.mem.bin" "${work:?}/$g-play.tga" "${work:?}/$g-before.tga"	# the game's memory and pictures: not kept
done

# ---- the window: Chimera, headless --------------------------------------------
# Its settings, cores folder, cache and recovery files are the gate's own:
# nothing is read from or left in a Chimera installed on this machine. Each
# run starts from empty settings ({}), which is a fresh install: the
# package's own bindings are the machine's.
export ALSOFT_DRIVERS=null CHIMERA_CORES_DIR="$work/cores" CHIMERA_QT_CACHE="$work/qt-cache" \
	CHIMERA_QT_RECOVERY="$work/qt-recovery" CHIMERA_QT_HASHES="$work/qt-file-hashes.txt" \
	XDG_CACHE_HOME="$work/cache-home" XDG_DATA_HOME="$work/data-home"
mkdir -p "$work/cores"
window() {	# window <tag> <project> <script> <job lines...>: one headless run of the window
	local tag=$1 proj=$2 script=$3 job="$work/job.$1.txt"; shift 3
	printf '%s\n' "$@" > "$job"
	printf '{}\n' > "$work/settings.$tag.json"
	[ -n "$proj" ] || return 1
	( cd "$(dirname "$emu")" && NP2_JOB="$(native_path "$job")" timeout 900 "$emu" --headless \
		"--config=$(native_path "$work/settings.$tag.json")" "--core=$(native_path "$package")" "${FWARGS[@]}" \
		"--project=$(native_path "$proj")" "--lua=$(native_path "$here/$script")" ) > "$work/$tag.log" 2>&1
}
if [ ! -f "$emu" ]; then
	for leg in frontend:boot frontend:read frontend:keybinds lua:script; do report "$leg" FAIL "Chimera's window not found: $emu"; done
else
	want=$(refhash np21)
	rm -f "$work/boot.meta.txt"
	window boot "$(project boot "$work/test.hdm" "${PINS[@]}")" frontend-ram.lua \
		"frames=$frames" "meta=$(native_path "$work/boot.meta.txt")" "shot=$(native_path "$work/boot.png")"
	if [ -f "$work/boot.meta.txt" ] && grep -q '^status=OK' "$work/boot.meta.txt"; then
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

	rm -f "$work/keys.meta.txt" "$work/keys.json"
	window keys "$(project keys "$work/test.hdm" "${PINS[@]}")" frontend-keys.lua \
		"out=$(native_path "$work/keys.json")" "meta=$(native_path "$work/keys.meta.txt")"
	if [ -f "$work/keys.meta.txt" ] && grep -q '^status=OK' "$work/keys.meta.txt"; then
		if "$PY" "$here/check-keybinds.py" "$work/keys.json" "$wb/default_keybinds.json" "PC-98 Controller" > "$work/keys.txt" 2>&1
		then report frontend:keybinds PASS "$(cat "$work/keys.txt")"
		else report frontend:keybinds FAIL "$(head -1 "$work/keys.txt")"; fi
	else
		report frontend:keybinds FAIL "no OK result (see build/frontend/keys.log)"
	fi

	# the tools: one Lua run on the counter disk, a leg for each check it makes
	rm -f "$work/tools.meta.txt"
	window tools "$(project tools "$work/counter.hdm" "${PINS[@]}")" frontend-tools.lua "meta=$(native_path "$work/tools.meta.txt")"
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

# ---- the window: Lua memory events (Chimera issues 496 and 534) -----------
memleg() {	# memleg <tag> <expect on|off> [chimera-run --new-project options...]
	local tag=$1 expect=$2 p; shift 2
	rm -f "$work/$tag.meta.txt"
	p=$(project "$tag" "$work/counter.hdm" "${PINS[@]}" "$@")
	[ -n "$p" ] || { report "$tag:script" FAIL "$(made "$tag")"; return; }
	window "$tag" "$p" frontend-memory.lua "meta=$(native_path "$work/$tag.meta.txt")" "expect=$expect" "tag=$tag"
	if [ -f "$work/$tag.meta.txt" ]; then
		while IFS= read -r line; do
			line=${line%$'\r'}
			name=${line%%=*}; rest=${line#*=}; word=${rest%% *}; detail=${rest#* }
			if [ "$name" = status ]; then
				[ "$word" = OK ] || report "$tag:script" FAIL "stopped: $detail (see build/frontend/$tag.log)"
			elif [ "$word" = INFO ]; then printf "%-20s %-5s %s\n" "$name" INFO "$detail"
			else report "$name" "$word" "$detail"; fi
		done < "$work/$tag.meta.txt"
		grep -q '^status=' "$work/$tag.meta.txt" || report "$tag:script" FAIL "the script did not finish (see build/frontend/$tag.log)"
	else
		report "$tag:script" FAIL "no result (see build/frontend/$tag.log)"
	fi
}
if [ -f "$emu" ]; then
	memleg mem21 on --settings '{"luaMemoryEvents":true}'
	memleg mem2 on --settings '{"machine":"PC98","clockMultiplier":"4","luaMemoryEvents":true}'
	memleg memoff off
fi

echo
echo "$ok passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
