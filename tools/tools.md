# Tools

## Segger JLink & VS Code
Ensure the JLink root folder is in the path variable:
1. In VS Code, go to File > Preferences > Settings.
2. Click the Open Settings (JSON) icon at the top right.
3. Add (or update) the terminal.integrated.env.windows section like this:

'''
"terminal.integrated.env.windows": {
    "PATH": "C:\\Program Files\\SEGGER\\JLink_V924a;${env:PATH}"
}
'''

## Segger flash task
1. ctrl+shift+p
2. Type in "task run"
3. Choose "J-Link: Flash RP2350"

## Segger Ozone (debugging)
Debugging is done through SEGGER Ozone rather than VS Code's built-in debugger.

1. Open `tools/ozone.jdebug` in Ozone.
2. It's pre-configured for the RP2350 (Cortex-M33 core 0) over J-Link/SWD, with the FreeRTOS-aware plugin enabled, and loads `build/src/projects/hub-master/hub_master.elf`.
3. RTT output (logging) appears automatically in Ozone's Terminal window once the target is running — see [rtt.md](../rtt.md).

## MSYS2 (host toolchain: tests)

The `arm-none-eabi-gcc` toolchain only produces ARM binaries — it can't build or run the host-side unit tests under `tests/`, which need a real native compiler environment instead. That's **MSYS2's UCRT64 environment**.

### Install

1. `winget install -e --id MSYS2.MSYS2` (or download from [msys2.org](https://www.msys2.org/)). Default install location is `C:\msys64`.
2. Open the **MSYS2** shell (plain, not UCRT64) from the Start Menu and update the core packages first:
   ```
   pacman -Syu
   ```
   This upgrades `pacman`/`bash`/`msys2-runtime` itself and will terminate the terminal partway through — that's expected. Reopen the MSYS2 shell and run `pacman -Syu` again until it reports nothing to do.
3. Open the **MSYS2 UCRT64** shell specifically (not MSYS2, not MINGW64 — UCRT64 is the environment this repo standardizes on) and install the toolchain and CMake:
   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake
   ```
   `mingw-w64-ucrt-x86_64-gcc` provides `g++`/`gcc`/`gdb`/`make`.
4. Add `C:\msys64\ucrt64\bin` to `PATH` (the `ucrt64\bin` subdirectory specifically — not the MSYS2 root, and not `mingw64\bin`, which is the older environment). Open a new terminal afterward so the change takes effect.
5. Verify: `g++ --version`, `gdb --version` should both resolve.

### Keeping it up to date

Periodically (and definitely if tool versions seem stale) run `pacman -Syu` from the plain MSYS2 shell — same two-pass procedure as step 2 above. Package updates (e.g. `mingw-w64-ucrt-x86_64-gcc`) flow straight through once the core system is current.

See `CLAUDE.md`'s Testing section for what this toolchain is actually used for and the exact build/run commands.

## LLVM (clang-tidy)

`clang-tidy` comes from a standalone LLVM install, not MSYS2's `clang-tools-extra` package (that was the previous setup; this repo has since moved off it).

### Install

1. Download the latest `LLVM-*-win64.exe` installer from the [LLVM releases page](https://github.com/llvm/llvm-project/releases).
2. Install to the default location (`C:\Program Files\LLVM`).
3. Verify: `clang-tidy --version` should resolve if `C:\Program Files\LLVM\bin` is on `PATH`; if it isn't, `tools/clang-tidy.py` falls back to that default install path automatically (see `tools/paths.json`).

See `CLAUDE.md`'s Linting section for how `tools/clang-tidy.py` uses this and the exact commands to run it.