#!/bin/sh
# build-package.sh - the core package: core.wbx and its declarations, zipped
# as np2kai-<id>.chimeraCore, the way Chimera's other cores name theirs
# (owner, 2026-10-24). The id is the package's own SHA1, 40 hex digits: a
# new build of anything gives a new name, and the same build the same one.
#
# A package is core.wbx (fixed name) + waterbox.config + default_keybinds.json
# + file_slots.json + the licences, loaded through Chimera's one generic
# waterbox adapter.
#
# Usage: build-package.sh [-r <chimera checkout>] [-o <output folder>]
#   The miniBox guest kit is <chimera>/extern/chimera-common-minibox unless
#   MINIBOX_DIR says otherwise. The package goes to <output folder>
#   (default build/package) and, when a Chimera checkout is found, is also
#   installed in <chimera>/build/Cores. Earlier np2kai packages are taken out
#   of both: each was a hand build that no release names.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
chimera_root=""
outdir="$root/build/package"
while getopts "r:o:" opt; do
	case "$opt" in
		r) chimera_root="$OPTARG" ;;
		o) outdir="$OPTARG" ;;
		*) exit 2 ;;
	esac
done
if [ -z "$chimera_root" ] && [ -d "$root/../chimera" ]; then
	chimera_root="$root/../chimera"
fi
[ -n "$chimera_root" ] && chimera_root=$(cd "$chimera_root" && pwd)
mb="${MINIBOX_DIR:-${chimera_root:+$chimera_root/extern/chimera-common-minibox}}"
[ -n "$mb" ] && [ -d "$mb" ] || { echo "miniBox guest kit not found; pass -r <chimera> or set MINIBOX_DIR" >&2; exit 1; }

# the declarations come from the tables the driver is built from
python3 "$root/tools/gen-config.py"
git -C "$root" rev-parse --git-dir >/dev/null 2>&1 && ! git -C "$root" diff --quiet -- waterbox/waterbox.config waterbox/default_keybinds.json && \
	echo "note: gen-config.py changed waterbox.config or default_keybinds.json; commit them with the tables" >&2

# the guest
MINIBOX_DIR="$mb" sh "$here/build-guest.sh" both
wbx="$root/build/wbx-both/core.wbx"
sh "$mb/source/guest/check-wbx.sh" "$wbx"

staging="$root/build/package-staging"
rm -rf "$staging"
mkdir -p "$staging" "$outdir"
cp "$wbx" "$staging/core.wbx"
cp "$here/waterbox.config" "$here/default_keybinds.json" "$here/file_slots.json" "$staging/"
# the controller's picture (tools/draw-keyboard.py), shown beside the bindings
mkdir -p "$staging/pictures"
cp "$here/pictures/keyboard.png" "$staging/pictures/"
# the terms travel with the binary (waterbox/package-licenses.json)
python3 "$mb/source/guest/package-licenses.py" "$root" "$staging" "$here/package-licenses.json"

# ---- version: the commit, and that commit's date in UTC ----
core_version="${CORE_VERSION:-}"
if [ -z "$core_version" ]; then
	if commit=$(git -C "$root" rev-parse --short=12 HEAD 2>/dev/null); then
		git -C "$root" diff --quiet HEAD 2>/dev/null || commit="$commit-dirty"
		core_version="$commit+local"
	else
		core_version="unversioned+local"
	fi
fi
core_version_date=$(TZ=UTC git -C "$root" log -1 --date=format-local:%Y-%m-%dT%H:%M:%SZ --format=%cd HEAD 2>/dev/null || true)
python3 - "$staging/waterbox.config" "$core_version" "$core_version_date" <<'PYVER'
import json, sys
path, version, date = sys.argv[1], sys.argv[2], sys.argv[3]
with open(path, encoding="utf-8") as f:
    cfg = json.load(f)
cfg["version"] = version
if date:
    cfg["versionDate"] = date
with open(path, "w", encoding="utf-8") as f:
    json.dump(cfg, f, indent=2, ensure_ascii=False)
    f.write("\n")
PYVER

# ---- provenance: what built this package (inputs only) ----
python3 - "$staging/build.json" "$root" "$mb" "$core_version" <<'PYPROV'
import json, subprocess, sys
out, root, mb, version = sys.argv[1:5]
def run(*args, cwd=None, default="unknown"):
    try:
        return subprocess.run(list(args), capture_output=True, text=True, check=True, cwd=cwd).stdout.strip()
    except Exception:
        return default
gcc = run("gcc", "-dumpfullversion")
json.dump({
    "version": version,
    "source": {"commit": run("git", "-C", root, "rev-parse", "HEAD"),
               "origin": run("git", "-C", root, "config", "--get", "remote.origin.url", default=""),
               "dirty": "-dirty" in version},
    "toolchain": {"compiler": "gcc " + gcc, "libstdc++": gcc, "target": "x86_64-linux-musl",
                  "binutils": (run("ld", "--version").splitlines() or ["unknown"])[0]},
    "guestKit": {"name": "miniBox", "commit": run("git", "-C", mb, "rev-parse", "--short=12", "HEAD")},
    "upstream": {"name": "NP2kai", "pin": run("git", "-C", root + "/extern/NP2kai", "rev-parse", "HEAD",
                                              default="5939e0c6d598 (plain copy, not yet a submodule)")},
}, open(out, "w"), indent=2, sort_keys=True)
PYPROV

# ---- deterministic zip: the package's SHA1 is the core's identity ----
zip_path="$outdir/np2kai.chimeraCore.part"
python3 - "$staging" "$zip_path" <<'PYZIP'
import hashlib, os, sys, tempfile, zipfile
staging, zip_path = sys.argv[1], sys.argv[2]
def write(path):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for top, dirs, files in os.walk(staging):
            dirs.sort()
            for name in sorted(files):
                full = os.path.join(top, name)
                info = zipfile.ZipInfo(os.path.relpath(full, staging), date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = 0o644 << 16
                with open(full, "rb") as f:
                    z.writestr(info, f.read())
write(zip_path)
with tempfile.NamedTemporaryFile(suffix=".zip") as tmp:
    write(tmp.name)
    again = hashlib.sha1(open(tmp.name, "rb").read()).hexdigest()
first = hashlib.sha1(open(zip_path, "rb").read()).hexdigest()
if first != again:
    sys.exit("packaging is not deterministic: %s then %s" % (first, again))
print("package sha1 " + first)
PYZIP
id=$(python3 -c 'import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],"rb").read()).hexdigest())' "$zip_path")
name="np2kai-$id.chimeraCore"
rm -f "$outdir/np2kai.chimeraCore" "$outdir"/np2kai-*.chimeraCore
mv "$zip_path" "$outdir/$name"
echo "packaged -> $outdir/$name"

if [ -n "$chimera_root" ]; then
	mkdir -p "$chimera_root/build/Cores"
	rm -f "$chimera_root/build/Cores/np2kai.chimeraCore" "$chimera_root"/build/Cores/np2kai-*.chimeraCore
	cp "$outdir/$name" "$chimera_root/build/Cores/$name"
	for cache in "$chimera_root"/build/CoreCache/np2kai-*; do
		[ -d "$cache" ] && rm -rf "$cache" || true
	done
	echo "installed -> $chimera_root/build/Cores/$name"
fi
