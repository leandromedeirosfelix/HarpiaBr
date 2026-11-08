# Harpia: design overview

A native x86-64 Windows build of the Switch 2 *The Legend of Zelda: Ocarina of Time*: the game's AArch64 code is
translated to C ahead of time, **on the user's PC, from the user's own decrypted dump**, and compiled with clang; the
Horizon OS and NVN2 are implemented natively; the GPU work is replayed on Vulkan. There is no CPU or GPU emulation at
any point.

Harpia is a version of **Hookshot** (oot-recomp, by NotRexed and contributors), itself descended from the
mkw-recomp / a64-kit line of static recompilations. See [CREDITS.en.md](../CREDITS.en.md) and
[NOTICE.md](../NOTICE.md). This page describes how the pieces fit; it deliberately holds no data read from the game.

## Pipeline

```
 exefs/ modules + main.npdm + romfs/            (the user's dump, read in place, never copied into the repository)
        │  nx2/nup.py (a .nup package) or the folders as they are; nx2/unpack.py makes ZBIC modules plain
        ▼
 modules: segments, dynamic section, symbols, relocations (RELA + RELR)
        │  nx2/image.py: one image at a fixed guest address, imports bound across modules, the HLE set hooked
        ▼
 image
        │  a64-kit lifter + nx2/insn.py + nx2/lift_fixes.py + nx2/poll.py + nx2/chunks.py
        ▼
 one C function per guest function (generated locally, large, never committed)
        │  clang -O1 and lld (nx2/build.py)
        ▼
 harpia.exe     = the lifted game + host/ (the Horizon system: threads, SVCs, memory, fs, hid, audio, vi, NVN2)
 harpia_vk.dll  = the Vulkan back end (gpu/vk), loaded by the exe
```

`nx2/make_game.py` runs all of this and is what the Harpia Builder (`builder/`) drives.

## Design decisions

- **Standalone.** One native process; no emulator beside it.
- **The SDK is replaced at the `nn::` boundary.** `HLE(<mangled name>)` functions in `host/` stand in for the SDK's
  OS-facing functions; the rest of the SDK runs as lifted code. Unimplemented imports get generated stubs that log
  their first call.
- **Memory.** A large reserved guest window with the image at a fixed address; memory is committed as it is handed out
  or on first touch (`host/hos.h`).
- **GPU.** `host/nvn_native.c` implements the NVN2 procs over guest memory in place; command buffers record an op
  stream and queue submits hand it to the back end. `gpu/vk` replays it on Vulkan 1.3: pools imported as host memory,
  Ampere shader programs lowered to Maxwell and translated to SPIR-V by Eden's recompiler, mesh and task programs with
  `VK_EXT_mesh_shader`.

## Rules that keep it correct

1. Cross-queue event waits are honoured (`vk_replay.cpp`); replaying in arrival order on one queue flashed.
2. Nothing the replay reads comes from live guest memory: reads come from submit-time captures.
3. Texture and sampler registrations are queued in order with the submits.
4. Clears after draws, and draws that sample their own target, get passes of their own.
5. The CPU saturates first: thread counts are measured, never raised by default.

## Testing

Game-free tests live in `tests/` and `nx2/test_*.py` (they run in CI). GPU runs use the real game only, one at a time;
see [CONTRIBUTING.en.md](../CONTRIBUTING.en.md#gpu-test-rules).

## Rules

- No game file, and nothing derived from the game (dumps, modules, lifted C, caches, shader dumps, saves), is ever
  committed. `.gitignore` blocks the usual paths.
- Licence and authorship notices are kept as they are (GPL-3.0-or-later; third-party parts keep theirs).
