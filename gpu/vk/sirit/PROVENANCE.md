# sirit

Sirit is a SPIR-V generator. This copy is **eden-emulator/sirit v1.0.5** (`b870b06299`, 2026-03-26), through Hookshot
(`gpu/vk/VULKAN.md`: "v1.0.5 plus Eden's mesh shader ops patch").

## Origin and licence

- Upstream: **ReinUsesLisp/sirit**. It is BSD-3-Clause since `60a856d266`; every source file here says
  "Copyright (c) 2019 sirit ... 3-Clause BSD License".
- `LICENSE.BSD-3-Clause` is a verbatim copy of ReinUsesLisp/sirit's `LICENSE.txt`: **the licence the file headers state**.
- `LICENSE` (AGPL-3.0) and `LICENSE.txt` (GPL-3.0) come from Eden's commit `495d1cfcad` (crueter, 2025-09-25, "[cmake]
  install target, static/shared split, agplv3"), which added the AGPL file, replaced the BSD text in `LICENSE.txt` with
  the GPL-3.0 text and changed CMake files, **without touching the per-file BSD headers**. Both files are kept as shipped.
- All the C++ in v1.0.5 was committed while `LICENSE.txt` was still the BSD-3 text, and between that commit and v1.0.5
  the only change Eden made under `src/` or `include/` is `src/CMakeLists.txt`. So the BSD-3-Clause headers describe the
  code; the AGPL/GPL label can only cover Eden's build files and what was added later.
- Harpia as a whole is GPL-3.0-or-later; BSD-3-Clause code is GPL-compatible if its notice, conditions and disclaimer
  are kept, and they are (the headers and `LICENSE.BSD-3-Clause`).

## Changes after v1.0.5

Not Harpia's own work: they come with Hookshot, and `include/sirit/sirit.h`, `src/sirit.cpp` and `src/stream.h` are
byte-identical to NotRexed/Hookshot's `dev` branch.

- Hookshot's additions (52 lines in `sirit.h`, `arithmetic.cpp`, `memory.cpp`, `misc.cpp`, `type.cpp`):
  Eden's mesh-shader ops (`OpEmitMeshTasksEXT`, `OpSetMeshOutputsEXT`; where that patch comes from is not documented, so
  its licence is not stated here), `TypeCooperativeMatrixKHR` (marked "(hookshot)") and `OpArrayLength` (marked
  "(EvilOOT)").
- klimovich008, 2026-11-07, merged through Harpia's upstream updates: `621102b` "fix(spirv): preserve physical
  addressing with Vulkan memory model" (the `SetMemoryModel(spv::MemoryModel)` overload) and `28ed17e` "Keep independently
  decorated SPIR-V structures distinct" (the `OpTypeStruct` dedupe bypass and `#include <cstdlib>` in `src/stream.h`).
- Harpia's own changes to this folder: none so far (anything added is to be tagged "(harpia)").
