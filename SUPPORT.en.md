# Help

**English** · [Português](SUPPORT.md)

Before opening an issue, check whether the answer is here.

> **We do not give support** for obtaining the game, decrypting it, keys or pirated copies. See the [legal notice](README.en.md).

## Installation problems (Harpia Builder / `make_game.py`)

| Message or symptom | What to do |
|---|---|
| *"This is build XXXXXXXX. Harpia supports build e0f3ccfa."* | Your game is not the supported version (program `0400486024DEC000`, update `v131072`). Use a dump of that version. |
| *"The file ... in this dump is not a game module"* / *"is not a game executable"* | The dump is incomplete, corrupt or still encrypted. Redo the dump from your console. |
| *"The package did not contain a romfs folder."* | The `.nup` has no `romfs`. Use another `.nup` or the `romfs` and `exefs` folders. |
| *"Visual Studio's C++ tools were not found"*, or a clang/EMSDK-not-found error | In the Visual Studio Installer choose **Modify** and tick **Desktop development with C++**. |
| *"The Windows SDK was not found"* | On the same screen, tick a **Windows SDK** (10 or 11). |
| *"Your Visual Studio is too old for this game's renderer"* | Open the Visual Studio Installer and update. |
| *"The builder's own Python ... is missing"* | Your antivirus probably removed the package's Python. Restore it from quarantine or unzip into a folder the antivirus allows. |
| The build sat at almost 0% CPU | Windows may have throttled the background window. Keep the Builder window visible. |
| Out of space | The build uses about 60 GB of temporary space. Free space on the work folder's drive (`%LOCALAPPDATA%\HarpiaBuilder`). |

## Problems while playing

- **Black screen or closes on start:** update the graphics driver (Vulkan 1.3 with mesh shaders is required). Start the game from the icon and look at `harpia_log.txt`.
- **Stutter the first time in each area:** that is shader compilation; it improves from the second run. Do not delete the `harpia_*.bin` files needlessly.
- **Occasional dark frames:** a [known issue](README.en.md#known-issues), under investigation.
- **I changed an option and nothing happened:** some apply only after a restart (the menu says so).
- **I want everything back to default:** close the game and delete `harpia_options.ini`.
- **I lost my saves:** they live in `user\save` next to the game. Back that folder up before rebuilding the game.

## I still need help

1. Search the [issues](../../issues?q=is%3Aissue) (closed ones too).
2. If you find nothing, open one with the **Bug** template, filling in graphics card, driver, Windows, `harpia_options.ini` and an excerpt of `harpia_log.txt` (see [how to report](CONTRIBUTING.en.md#reporting-a-bug)).
3. **Never** post game files, `*.bin` caches or saves.

We reply when we can: the project is maintained by volunteers.
