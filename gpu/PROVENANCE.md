# gpu/: the standalone build's renderer

`render/`, `compat/` and `third_party/suyu/` are TOTKPort's renderer (`a64-kit/gpu/`, commit `e88744a`,
2026-10-31): the native NVN layer's replay on WebGPU (wgpu-native on the PC), its texture, buffer, pipeline and
present code, and suyu's shader recompiler with TOTKPort's WGSL backend. `host/nvn_native.c` and `host/display.c`
are the API side and the window, from the same commit (see `host/PROVENANCE.md`).

Licensing as in TOTKPort's `NOTICE.md`: suyu's parts are GPL-2.0-or-later / GPL-3.0-or-later per file (texts in
`third_party/suyu/`); `render/nvn_api.h` (h1k421, as adapted by jhaizhou), the NVN layer's design (jhaizhou's
d2r-mobile / MacNativeGames) and the FSR 1 port in `render/rx_present_fx.cpp` are MIT; `render/renderdoc_app.h` is
RenderDoc's MIT header. wgpu-native (MIT or Apache-2.0) is not vendored: `nx2/build.py` takes TOTKPort's build of it
(`WGPU_NATIVE` for another).

mkw-recomp's changes, for the Switch 2:
- **Ampere shaders.** `third_party/suyu/shader_recompiler/frontend/ampere/` is Eglantine's Ampere-to-Maxwell
  lowering (header-only; Eden Emulator Project, GPL-3.0-or-later, from the `work/eg` clone). `environment.h` gains
  `TranslateProgramAddress` and the control flow passes indirect branch targets through it, as in Eglantine.
- **NVN2 shader packages** (`render/rx_shader.cpp` `read_nvn_code`): the control section's layout (magic
  `0x98761234`, code length `+0x38`, c1 constants `+0x40..+0x44`, stage `+0x56`, workgroup `+0x58`), the 128-byte
  graphics program header, and the lowering before the Maxwell frontend.
- **The Switch 2's GOB swizzle** (the Tegra X1's with offset bits 5 and 6 swapped, from Eglantine's native tiling):
  `third_party/suyu/video_core/textures/decoders.h` (the CPU swizzle masks), the two WGSL unswizzles in
  `render/rx_texture_gpu.cpp`, and `bl_offset` in `render/rx_nvn.cpp`.
- **The 64-bit guest window**: per-page tables in `render/rx_buffer.cpp` are committed untouched (`PageTable`).
- **The window crop** (`rx_present_crop`): MKW renders at a dynamic size into a 3840x2160 window texture and crops
  it; the crop is copied out and presented.

`mkw_gpu.cpp` / `mkw_gpu.h` / `build-gpu.bat` are the earlier Vulkan stand-in (a window and a swapchain; no longer
loaded). `shadertool.cpp` runs dumped NVN2 programs through Eglantine's recompiler offline (SPIR-V; it prints what
each program reads from the constant banks).

Harpia `gpu/vk` ports in the black-flash / translator round (branch `p3-flashes`):
- **Mesh index map by whole 32-primitive blocks** (`spirv_emit_context.cpp` `mesh_map_bytes`): the idea from
  nxnative's `docs/reference/shaders.md` ("Mesh stage memory reserves the primitive index bytes for every primitive
  block", PR 1575; MIT). Our own code.
- **Program capture** (`frontend/ampere/program_capture.h`: CALL.NOINC as a call, EXIT past the last callee, BRA's
  .DIV/.CONV bits) and **DEPBAR with any scoreboard** (`lower_to_maxwell.h` 0x91a): from ocarina-recomp's Eglantine
  fork (`program_capture.h:40-71`, commit 01e4fa9; `lower_to_maxwell.h:2437-2452`) as also carried by Lichii;
  GPL-3.0-or-later (Eden Emulator Project), the same licence as the file.
- **Colour write masks narrowed on NVIDIA** (`vk_draw.cpp` `target_write_mask`, `RX_VK_NARROW_COMPONENTS`): the
  finding from ocarina-recomp commit 0cf12aa (GPL-3.0) that NVIDIA writes zeros to channels a fragment shader leaves
  unwritten; none of its code is copied.

`gpu/vk/host_shaders/` holds three compute shaders and the SPIR-V arrays generated from them
(`*_comp_spv.h`, glslang 16.6.0 = `// 1116.6.0`, `glslang -V --target-env vulkan1.1 --vn <NAME>`). The `.comp` files are
**Eglantine's** (Eglantine-Community/Eglantine, branch `nx2`, GPL-3.0-or-later, Eden Emulator Project headers kept
untouched) and are the preferred form of the arrays; compiling them reproduces the shipped words exactly (checked word
for word, and by `tests/test_host_shader_sources.py` when glslang is available):

| File | Eglantine `nx2` commit | Why that commit |
|---|---|---|
| `astc_decoder.comp` | `c556de5c9ac5385faff6da8dd1694a93a77c35e4` (2026-10-28) | the Switch 2 GOB layout |
| `bc_encoder.comp` | `0d5534a2883329183ae8aa89b3fe1c7b67291ece` (2026-10-29) | "ASTC: decode and re-compress to BC1/BC3 on the GPU" |
| `block_linear_unswizzle_2d.comp` | `51207c73c889dbdc4a586a5da23314f6d33e4eb9` (2026-10-28) | the Switch 2 GOB layout |

The files are identical at those commits and at `nx2` `78743f30` (2026-11-08). The 3D unswizzle array, which nothing
included, was deleted (upstream Hookshot did the same in `f871bfd`).
