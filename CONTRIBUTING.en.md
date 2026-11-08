# How to contribute

**English** · [Português](CONTRIBUTING.md)

Thanks for wanting to help! This guide covers building, branches and commits, the GPU test rules and how to send a PR. By taking part you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).

## Rules that always apply

1. **No game data, anywhere.** No `romfs`, `exefs`, `image.bin`, `.nso` modules, caches (`*_shaders.bin`, `*_programs.bin`, `*_pipelines.bin`, `*_prepared.bin`), dumps, saves, keys, screenshots that show the game in full, or logs with personal paths. `.gitignore` blocks most of it; check `git status` before every commit.
2. **No piracy.** Do not ask for or offer game files, keys or instructions to obtain them.
3. **Keep licence and authorship notices.** `(hookshot)` comments, `Hookshot issue/PR N` references, `SPDX-License-Identifier` headers and `LICENSE`/`PROVENANCE` files are not removed. Our changes to inherited code carry the `(local)` marker.
4. **Only GPL-3.0-or-later-compatible code goes in.** When bringing something from another project, add a line to `gpu/PROVENANCE.md` or `host/PROVENANCE.md` with the project, the source commit and the licence, and credit it in [CREDITS.en.md](CREDITS.en.md).
5. **Do not distribute proprietary binaries** (for example NVIDIA's DLSS DLLs).

## Building

Requirements and commands are in the [README](README.en.md#requirements) and in [docs/development.md](docs/development.md). In short:

```powershell
python -I nx2/make_game.py --nup "C:\path\your-game.nup" --out "C:\Games\Harpia" --language en-US
```

For development use a separate work folder (`OOT_WORK`) and **never** the game folder you play from. Tests that do not need the game:

```powershell
python -I -B nx2/test_nup_paths.py
python -I -B tests/test_make_game_cache.py
python -I -B tests/test_translator_stamp.py
```

(these are examples; the list CI runs is in `.github/workflows/source-regressions.yml`).

A note on the shader cache: changes to the translator code (`gpu/vk/eden`, `gpu/vk/sirit`, `gpu/vk/compat`, `vk_shader.cpp`, `vk_state.h`, `vk_api.h`) change the *stamp* and recompile everyone's caches. Comments and renames do not. Avoid touching them without need.

## Branches and commits

- `main` is the stable branch: only what builds and was tested goes in.
- Work on your own short-named branch: `feat/<topic>`, `fix/<topic>`, `docs/<topic>`.
- Commits in the form **`type: description`**, in Portuguese or English, **one topic per commit**:

  `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`

  Example: `fix: decommit freed guest pages under the import lock`.
- Explain the **why** in the commit body when it is not obvious.

## GPU test rules

The game uses the real GPU, and the development machine is also the one people play on. Therefore:

1. **One GPU test at a time.** **Never two games on the GPU at once**: in testing, contention has caused *device lost*. `scripts/run-vk.ps1` refuses to run if another `harpia.exe` is open (`-Force` skips the guards; read the top of the script, it has the author's personal guards).
2. **Only the real game.** Never dispatch synthetic shaders on the GPU (it has caused device loss).
3. **Before a run that changes the stamp**, back up `harpia_programs.bin`, `harpia_pipelines.bin`, `harpia_vk_pipelines.bin` and `harpia_prepared.bin` (or use a copy of the test folder), otherwise the "warm" baseline turns "cold".
4. Any list in `RX_VK_SKIP_PROG` must contain `53509044401d78d2`.
5. Windowless runs (`HOS_HEADLESS`) do not exercise frame generation, NIS, the "Fit" bars or presentation: those need a window.
6. Delete screenshots (`work\runs\<name>\shots`) after looking: they reach gigabytes. Check whether Windows logged `nvlddmkm` events (driver faults) at the end of the run.
7. Code-only changes (no GPU) can be reviewed without a GPU test, but say so in the PR.

## Sending a Pull Request

1. Fork, create the branch and keep the PR **small and about one topic**.
2. Fill in the PR template: what changes, why, how it was tested (GPU, driver, Windows) and what was **not** tested.
3. Confirm the licence checklist (above) and that no game data got in.
4. Be ready for adjustments: review looks for logic errors, licence/credit problems and GPU risks.

## Reporting a bug

Open an [issue](../../issues/new/choose) with the **Bug** template. It asks for:

- graphics card, **driver version**, Windows and Harpia versions;
- the contents of `harpia_options.ini`;
- an **excerpt** of `harpia_log.txt` (the last ~100 lines and any lines with `error`/`device lost`), **not the whole file**;
- steps to reproduce and, if it helps, a screenshot or video.

Before attaching, **read the log**: it may contain folder names from your PC. **Never attach** caches (`*.bin`), `image.bin`, saves or game files.

## Asking for help or suggesting something

Usage questions: [SUPPORT.en.md](SUPPORT.en.md). Ideas: the **Feature request** issue template. Vulnerabilities: [SECURITY.md](SECURITY.md).
