# Credits and thanks

**English** · [Português](CREDITS.md)

Harpia exists only because many people opened their work. **This file says plainly who did what.** Harpia is a version of **Hookshot** (by NotRexed and contributors): it **does not hide** its origin and **keeps every licence and authorship notice**, as the GPL requires. The legal text of each component is in [NOTICE.md](NOTICE.md) and in the `PROVENANCE.md` / `NOTICE.*` files of each folder; this file is the human summary.

If you wrote something here and the credit is wrong, incomplete or missing, [open an issue](../../issues/new/choose): we will fix it right away.

## The base

| Project | Who | What Harpia inherits | Licence |
|---|---|---|---|
| **[Hookshot](https://github.com/NotRexed/Hookshot)** (oot-recomp) | **NotRexed** and contributors | The whole static recompilation, the reimplemented Horizon system (`host/`), the NVN2 graphics layer, the Vulkan renderer and the Builder. **Harpia is a modified version of it.** | GPL-3.0-or-later |
| **a64-kit / Rexcomp / TOTKPort / BlueShell / Skybound** | NotRexed | The AArch64-to-C translation kit and the runtime `host/` descends from (`third_party/a64-kit/`, `host/PROVENANCE.md`) | GPL (via suyu) |

Hookshot contributors who sent fixes (as named by the original project): klimovich008, d3zessete, chrispository, vzeol, monsterlag, leonidasmark, joaoalevato, shayder, lev92, AyyBeeDee20738, among others.

## Graphics and shaders

| Project | Role | Licence |
|---|---|---|
| **[Eden](https://git.eden-emu.dev/eden-emu/eden)** and **Eglantine** | The shader recompiler (`gpu/vk/eden/`) and the work of lowering the Switch 2's Ampere shaders to Maxwell | GPL-3.0-or-later |
| **yuzu / suyu** | Shader recompiler, graphics decoders and the AArch64-to-C translator (`gpu/third_party/suyu/`, `third_party/a64-kit/lift/suyu/`) | GPL-2.0-or-later / GPL-3.0-or-later per file |
| **Sirit** (ReinUsesLisp; Eden's fork) | SPIR-V generator used by the recompiler (`gpu/vk/sirit/`) | BSD-3-Clause (`LICENSE.BSD-3-Clause`; Eden added AGPL/GPL texts, see `PROVENANCE.md`) |
| **Mesa (NAK)** | Reference for the Ampere instruction encoding | reference only |
| **Vulkan Memory Allocator** (AMD) | Video memory allocation; fetched during the build | MIT |
| **AMD FidelityFX Super Resolution 1** | The FSR 1 upscaler (`gpu/vk/shaders/ffx/`) | MIT, Advanced Micro Devices |
| **NVIDIA Image Scaling** | The NIS upscaler (`gpu/vk/shaders/nis/`) | MIT, NVIDIA Corporation |
| **RenderDoc** | Capture API header | MIT |
| **h1k421** and **jhaizhou** (d2r-mobile / MacNativeGames) | NVN API types and the design of the native NVN layer | MIT |
| **fincs** (deko3d) | NVN texture storage layout | zlib |

## Research and fix projects

These open projects were read, measured and used as references; whatever was brought here has its line in [gpu/PROVENANCE.md](gpu/PROVENANCE.md) or [host/PROVENANCE.md](host/PROVENANCE.md), with the source commit.

| Project | Contribution to Harpia |
|---|---|
| **[sostenesfreitas/ocarina-recomp](https://github.com/sostenesfreitas/ocarina-recomp)** | Research on the game (mesh-stage control section, DEPBAR, program capture, colour write masks). The original README layout came from here too. |
| **[zortos293/Lichii](https://github.com/zortos293/Lichii)** | Reference for lowering Ampere shaders and for the game's DLSS path |
| **[seedds/nxnative](https://github.com/seedds/nxnative)** | Documentation of task and mesh shaders (index map in blocks of 32 primitives) and of the shader census |
| **MacNativeGames** (jhaizhou / ly05010419) | The NVN layer's design and the virtual cores |
| **[TotK Recomp](https://github.com/valentderah/totk-pc-recomp)** (valentderah) | Reference for performance and virtual cores (MIT-licensed origin noted in a64-kit's notices) |
| **EvilOOT** | Foliage and lighting fixes (pointer-based buffer access, `OpArrayLength`) brought in by the upstream update |
| **[MythosOfs/Orchid](https://github.com/MythosOfs/Orchid)** | The game's fog and exposure fix |
| **dkb-pc-recomp**, **LASRecomp**, **SavageWindRecomp**, **Ryujinx** | Studied as research references (device-loss diagnostics, descriptors, performance panel, hardware behaviour). No code copied from them is claimed here without a line in the `PROVENANCE.md` files. |

### Code inherited from yuzu / suyu / Eden

The headers of these files name, and we keep: Citra Emulator Project, Dolphin Emulator Project, PPSSPP Project, Gekko Emulator, Tony Wasserka, Jannik Vogel, degasus (WTFPL), the University of North Carolina / FasTC (Apache-2.0), Boost (BSL-1.0) and Austin Appleby (public domain), among others. The licence texts are in the files themselves or in `gpu/third_party/suyu/`.

**mkw-recomp** (by NotRexed, the Mario Kart World port) is the line Hookshot descends from; many paths and comments cite it.

## Libraries

| Library | Use | Licence |
|---|---|---|
| **[SDL2](https://www.libsdl.org)** | Window, audio and controllers (`third_party/sdl2/`) | zlib |
| **[Opus](https://opus-codec.org)** 1.5.2 | Audio | BSD-3-Clause (`third_party/opus/COPYING`) |
| **zbic** (kinnay) / **Atmosphere-NX** | Decompressing the game's modules; a separate tool, not linked into the game | zstd and the ZBIC code (hexkyz): BSD or GPLv2, used under BSD-3-Clause; divsufsort (Yuta Mori): MIT |
| **zstd** | Decoding | BSD or GPLv2 |
| **stb_truetype** (Sean Barrett) | Menu text in the Linux version | public domain or MIT |
| **wgpu-native**, **FFmpeg**, **dav1d**, **Vulkan / SPIR-V Headers** | Fetched or used at build time, with their own licences | MIT/Apache-2.0, LGPL, BSD-2, Apache-2.0 |

## What is Harpia's own

**Leandro Felix**: the name, the visual identity, the tabbed menu in Portuguese and English, the FSR 1 and NIS upscalers in the menu, the aspect, scale, filtering and VSync options, the AMD fixes, the integration of upstream updates, the sturdier shader cache, the dark-frame detector and this documentation.

Development tools: part of the code and documentation was made with the help of AI assistants (Claude, by Anthropic), with changes integrated and tested on the author's PC.

### Art

The emblem and the banner were created with generative AI help and composed for this project. They use no official Nintendo art.

## Licence

GPL-3.0-or-later; see [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

## Licence texts

See [LICENSES/](LICENSES/README.md).
