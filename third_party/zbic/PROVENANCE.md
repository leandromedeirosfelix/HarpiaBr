# zbic

ZBIC is the NSO segment compression of Switch firmware 22.0.0+ (NSO flags bit 7, "UseZbicCompression"): zstd
frames with the magic `ZBIC` (0x4349425A) and entropy tables written with binary interpolative coding.

## What is here

- `zstd.c`, `zstd.h`, `zstd_errors.h`: Atmosphere-NX's single-file zstd with `ZSTD_ZBIC_SUPPORT`
  (`libraries/libstratosphere/source/util/zstd.c` at Atmosphere `082115187a`), as copied into
  <https://github.com/kinnay/zbic> (`src/`), **byte-identical**, unmodified.
- The ZBIC code was written by hexkyz in `hexkyz/zstd`, branch `zbic` (a fork of Meta's zstd), commits `a7cb12b458`
  "implement ZBIC" and `02a8f10c6f` "ZBIC improvements"; Atmosphere's commit `06870a20e7` synchronises from it. Those
  commits add only Meta's dual-licence header, on new files.
- `zbic_api.c` is ours: two exported C functions (`zbic_content_size`, `zbic_decompress`) wrapping zstd's.

## Licence

- Every licence header in `zstd.c` is Meta's (and two are Tino Reichardt's, 2016): **"BSD-style licence (`LICENSE`) and
  the GPLv2 (`COPYING`), at your option"**. Harpia uses the **BSD-3-Clause** option: `LICENSE.zstd` is Meta's zstd
  `LICENSE`, the BSD text, and must travel with any binary built from these files.
- `zstd.c` inlines **divsufsort** (MIT, Copyright (c) 2003-2008 Yuta Mori); its notice is in `zstd.c` itself.
- `LICENSE` here is the GPLv2 text copied with kinnay/zbic: it is the *other* option of the dual licence, kept for
  completeness, not the licence Harpia relies on. Atmosphere's own source files (for example
  `util_compression_zstd_bic.cpp`) are GPL-2.0-only; none of them is in this folder.

## How it is used

It is built into `zbic.dll` by `nx2/zbic.py` on first use, loaded with ctypes by `nx2/unpack.py` and `nx2/nup.py`, and
used only to unpack the dump's modules. It is a separate tool: none of it is linked into the game (`harpia.exe`,
`harpia_vk.dll`). It ships in the Builder package, so its notices ship there too.

Build command (`nx2/zbic.py`): `clang -O2 -shared -DZSTD_ZBIC_SUPPORT=1 -I third_party/zbic -o zbic.dll zstd.c zbic_api.c`
(with the toolchain's native target and link flags). SHA-256 of the `zbic.dll` checked on 2026-11-09 (828,928 bytes, imports
only KERNEL32.dll, exports only `zbic_content_size` and `zbic_decompress`):
`00DF860B0E1606CE5EC7AD531C0ACE34BD1E3B421F5D4B5F96AAD7C174E56EA7`; a different clang gives a different hash.
