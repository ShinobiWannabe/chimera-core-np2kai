# Lua memory events

Chimera's Lua scripts can stop part way through a frame at memory the
machine runs, reads or writes: `event.on_bus_exec`, `on_bus_exec_any`,
`on_bus_read` and `on_bus_write` (Chimera issues 496 and 534). This core
offers them when the setting **Lua Memory Events** is on. It is off by
default.

## The setting

Each machine is in the package twice: NP2kai's CPU as it is, and a copy
that checks every instruction and every memory access (np2-entry.c picks
the copy when the machine starts). With the setting off the core runs at
full speed, and Chimera refuses the events with its own message ("This
core cannot call a Lua function at ..."). With it on, the PC9821 machine
is about 4% slower (Popful Mail, sandbox: about 520 frames a second
against 545). The PC98 machine shows no difference beyond noise.

The machine is the same either way. Every frame, the sound, the clock and
the memory are the same, also with every address watched, so a movie plays
back the same with the setting on or off. The gate checks this on both
machines.

## What a script sees

- **One scope, "System Bus"**: physical addresses, numbered as the core's
  System Bus memory domain. An address found with RAM Search can be used
  as it is. With paging on (Windows-era software on the PC9821), the
  address is the physical one the page maps to, not the linear one.
- **Instructions** (`on_bus_exec`): called before the instruction runs, at
  the physical address of its first byte, with the registers as they are
  then.
- **Reads and writes** (`on_bus_read`, `on_bus_write`): one call per access,
  at its first byte, with its size (1, 2 or 4 bytes) and its value. A
  read is reported once the value is fetched, and a write before memory
  changes. The script may give back another value, and the machine reads
  or writes that instead.
- Watches stay through state loads. No savestate carries them.

## Not covered

- On the PC98 (286) machine, instruction bytes are fetched as ordinary
  reads, so a read watch on code also fires when that code runs. The
  PC9821 machine does not report its instruction fetches.
- The CPU's own page-table reads and writes (paging on).
- A 64-bit or 80-bit access (MMX, FPU) is reported as two or three
  accesses of 4 or 2 bytes. An access that crosses a page with paging on
  is reported as its bytes on each page.
- Interrupt vector fetches on the 286 machine.
- Memory that the CPU does not touch itself: DMA (the floppy and the sound
  boards), the BIOS calls NP2kai carries out in C (disk reads through
  INT 1Bh and the like), and the memory tools' own pokes.
- Block moves NP2kai makes in one call (descriptor tables, the FPU state)
  are reported a byte at a time.

## How it is built

- `waterbox/port/memwatch.c`: what is watched (up to 1024 addresses per
  kind, or every address), and the call into Chimera. It is all
  `ECL_INVISIBLE`.
- `waterbox/port/trap/steptrap.h`: the watch copies are built with
  `ENABLE_TRAP`, so both CPU loops call `steptrap()` before each
  instruction. Here that call is one flag test.
- `waterbox/port/cpumem-watch.c`: NP2kai's `i386c/cpumem.c` or
  `i286c/cpumem.c`, unchanged, with the CPU's memory functions renamed and
  wrapped.
- `waterbox/port/paging-watch.c`: NP2kai's IA-32 paging. A watched page
  gets no direct host pointer in the TLB, so its accesses reach the
  wrappers.
- `waterbox/port/cpu286-*.c`: the 286 CPU's byte read-modify-write
  instructions take NP2kai's own slow path while something is watched.
- `extern/NP2kai` is not changed. Test harnesses: `NP2_WATCH` in
  `run-native` and `run-wbx` (`waterbox/watch-script.h`), and
  `tests/gen-testdisk.py --paging`.
