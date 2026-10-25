#!/bin/sh
# build-guest.sh - the core itself: NP2kai and the driver built for the
# miniBox guest (musl, -mcmodel=large, fixed base), as core.wbx.
#
# The same sources and the same np2-flags.sh as build-native.sh, so the two
# builds differ only in the C library and the code model.
#
# Usage: build-guest.sh [np21|np2|both]     (default np21, the IA-32 machine)
# Output: build/wbx-<flavour>/core.wbx
#
# "both" is the package's core (docs/PLAN.md section 10): the np21 and np2
# builds each linked into one relocatable half, with their driver's exports
# renamed np21_* and np2_* (np2-half.h) and every hidden symbol made local,
# then np2-entry.c on top to pick the half the machine setting names.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
FLAVOUR=${1:-np21}
HALF=${NP2_HALF_BUILD:-}
NP2=$root/extern/NP2kai
PORT=$here/port
MB=${MINIBOX_DIR:-$root/../chimera/extern/chimera-common-minibox}
MBB=${MINIBOX_BUILD:-$MB/build/meson-cpp}
SR=$MBB/guest-sysroot
[ -f "$SR/lib/musl-gcc.specs" ] || { echo "no miniBox C++ guest sysroot at $SR (meson -Dguest_cpp=true, then ninja musl-gcc libstdcxx-installed.stamp)" >&2; exit 1; }
. "$here/np2-flags.sh"
out=${NP2_OUT:-$root/build/wbx-$FLAVOUR}
obj=$out/obj
mkdir -p "$obj"

# The guest flags, as miniBox's own guest build and the PCem core have them.
# -mno-red-zone: see miniBox docs/RED-ZONE.md. -fno-gnu-unique makes C++'s
# one-per-program statics weak, which a half can make local (see "both"). The thread_local defines make
# the few thread-local variables plain ones: the guest has one thread.
GCCVER=$(gcc -dumpfullversion)
GUEST="-specs $SR/lib/musl-gcc.specs -fvisibility=hidden -mcmodel=large -mno-red-zone \
 -mstack-protector-guard=global -fno-stack-protector -fno-pic -fno-pie \
 -fcf-protection=none -DCHIMERA_GUEST -Dthread_local= -D_Thread_local= -D__thread= \
 -I$MB/extern/emulibc -I$MB/source/guest/include -I$MB/extern/jsmn"
GUESTCXX="$GUEST -fexceptions -fno-gnu-unique -I$SR/include/c++/$GCCVER -I$SR/include/c++/$GCCVER/x86_64-linux-musl"

if [ "$FLAVOUR" = both ]; then
  out=${NP2_OUT:-$root/build/wbx-both}
  mkdir -p "$out/obj"
  for f in np21 np2; do
    NP2_HALF_BUILD=1 NP2_OUT= sh "$0" "$f" || exit 1
  done
  gcc $GUEST -O2 $NP2_ZEROINIT -c "$here/np2-entry.c" -o "$out/obj/np2-entry.o" || exit 1
  # A name both halves still define would be one function for two machines:
  # only the renamed exports may be global, and those differ by prefix.
  dup=$(for f in np21 np2; do nm -g --defined-only "$root/build/wbx-$f/half.o" | awk '{print $3}' | grep -v "^${f}_"; done | sort | uniq -d)
  [ -z "$dup" ] || { echo "both halves define:"; echo "$dup"; exit 1; }
  g++ $GUESTCXX -o "$out/core.wbx" "$out/obj/np2-entry.o" "$root/build/wbx-np21/half.o" "$root/build/wbx-np2/half.o" \
     -static -no-pie -Wl,--eh-frame-hdr,-O2,--no-relax \
     -T "$MB/source/guest/linkscript.T" \
     -Wl,-u,pthread_once -Wl,-u,pthread_cond_wait \
     -Wl,-u,pthread_cond_broadcast -Wl,-u,pthread_key_create \
     "$MBB/source/guest/cxxglue.c.o" "$MBB/source/guest/emulibc.c.o" \
     -L"$SR/lib" -lstdc++ -lgcc -lgcc_eh -lc 2>"$out/errors.log" || { echo "link failed; see $out/errors.log"; exit 1; }
  echo "OK: $out/core.wbx"
  sh "$MB/source/guest/check-wbx.sh" "$out/core.wbx" || { echo "check-wbx.sh reported problems"; exit 1; }
  exit 0
fi

SOURCES=$(python3 "$root/tools/sources.py" "$NP2" "$FLAVOUR" | sed "s|^|$NP2/|")
SOURCES="$SOURCES $(ls "$PORT"/*.c "$PORT"/*.cpp 2>/dev/null) $here/np2-driver.c"

COMMON="-O2 -fno-strict-aliasing $NP2_DEFINES $NP2_INCLUDES"
fail=0; objs=""
: > "$out/errors.log"
for s in $SOURCES; do
  [ -f "$s" ] || continue
  rel=${s#$root/}
  o=$obj/$(printf '%s' "$rel" | tr '/' '_' | sed 's/\.[^.]*$/.o/')
  extra=""
  # in a half the driver's exports carry the half's prefix (np2-half.h)
  if [ -n "$HALF" ] && [ "$s" = "$here/np2-driver.c" ]; then
    o=${o%.o}-half.o; extra="-DNP2_HALF=${FLAVOUR}_ -include $here/np2-half.h"
  fi
  objs="$objs $o"
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ "$here/np2-flags.sh" -nt "$o" ] || [ "$PORT/compiler.h" -nt "$o" ] || { { [ "${s%/*}" = "$here" ] || [ "${s%/*}" = "$PORT" ]; } && [ -n "$(find "$here" "$PORT" -maxdepth 1 -name '*.h' -newer "$o" -print -quit)" ]; } || { [ -n "$extra" ] && [ "$here/np2-half.h" -nt "$o" ]; }; then
    case "$s" in
      *.cpp) g++ $GUESTCXX $COMMON $NP2_CXXFLAGS $(np2_file_flags "$s") -w -c "$s" -o "$o" 2>>"$out/errors.log" || { echo "FAILED: $rel"; fail=1; } ;;
      *)     gcc $GUEST $COMMON $NP2_CFLAGS $(np2_file_flags "$s") $extra -w -c "$s" -o "$o" 2>>"$out/errors.log" || { echo "FAILED: $rel"; fail=1; } ;;
    esac
  fi
done
[ $fail -eq 0 ] || { echo "compile failed; see $out/errors.log"; exit 1; }

if [ -n "$HALF" ]; then
  # One relocatable object for the half, with every name it defines made
  # local but the renamed exports, so the other half's copies of them
  # cannot be bound to it. C++ inline code and templates come in COMDAT
  # groups, which the final link keeps once by name for the whole program;
  # removing the groups keeps each half's own copy.
  ld -r -o "$out/half-raw.o" $objs 2>>"$out/errors.log" || { echo "ld -r failed; see $out/errors.log"; exit 1; }
  objcopy -w --keep-global-symbol="${FLAVOUR}_*" -R .group "$out/half-raw.o" "$out/half.o" || exit 1
  echo "OK: $out/half.o"
  exit 0
fi

# miniBox's C++ guest link recipe (source/guest/meson.build). The order is
# load-bearing: -lstdc++ -lgcc -lgcc_eh -lc, libc last; --no-relax for the
# large code model; the -u pthread_* because libgcc_eh refers to them weakly.
g++ $GUESTCXX -o "$out/core.wbx" $objs \
   -static -no-pie -Wl,--eh-frame-hdr,-O2,--no-relax \
   -T "$MB/source/guest/linkscript.T" \
   -Wl,-u,pthread_once -Wl,-u,pthread_cond_wait \
   -Wl,-u,pthread_cond_broadcast -Wl,-u,pthread_key_create \
   "$MBB/source/guest/cxxglue.c.o" "$MBB/source/guest/emulibc.c.o" \
   -L"$SR/lib" -lstdc++ -lgcc -lgcc_eh -lc 2>>"$out/errors.log" || { echo "link failed; see $out/errors.log"; exit 1; }
echo "OK: $out/core.wbx"
sh "$MB/source/guest/check-wbx.sh" "$out/core.wbx" || { echo "check-wbx.sh reported problems"; exit 1; }
