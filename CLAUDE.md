# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`modular-midi` is a multi-project embedded firmware repo. The first product, `hub-master`, is an RP2350-based MIDI controller with 4 buttons and 4 displays (one display shows live info for its corresponding button). Future hardware projects will live alongside it and share common infrastructure.

## Git workflow

**Never commit directly to `main`.** All work happens on a topic branch. This is enforced by GitHub branch protection (see Continuous Integration below), not just convention: `main` only accepts changes via a PR with the `build-test-lint` check passing, and direct pushes are rejected even for the repo owner. Merge via GitHub's "Squash and merge" or "Rebase and merge" (plain merge commits are disabled — linear history is required); a bare local `git merge --ff-only && git push` to `main` no longer works, since any direct push to the protected branch is rejected regardless of whether it's a fast-forward. Before committing, confirm the current branch is not `main`.

## Build

Requires `arm-none-eabi-gcc`, `ninja`, and `cmake` on PATH (see `build.ps1` for the exact toolchain paths this repo was set up with, and `setup.md` for the tool list).

```powershell
./build.ps1
```

This configures into `build/` with Ninja (`-DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350`) and builds. Equivalent manual invocation from repo root:

```powershell
cmake -G "Ninja" -B build -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350
cmake --build build
```

Output ELF for the current project: `build/src/projects/hub-master/hub_master.elf` (also produces `.uf2`, `.bin`, `.map` via `pico_add_extra_outputs`).

**`build/`'s `CMAKE_BUILD_TYPE` is `Debug`, but only because that got cached the first time this directory was configured** — neither `build.ps1` nor the command above ever passes `-DCMAKE_BUILD_TYPE`, and pico-sdk's own default (`src/libs/pico-sdk/cmake/pico_pre_load_toolchain.cmake`) is actually `Release`. A cache variable, once set, sticks across reconfigures regardless of that default. If `build/` is ever deleted and recreated, or a new build directory is configured, it will silently come back as `Release` (smaller/differently-optimized binary) unless `-DCMAKE_BUILD_TYPE=Debug` is passed explicitly. Don't assume the two are equivalent — they aren't (confirmed while validating the container below: an initial container build defaulted to `Release` and came out ~15% larger in `.text` than the native `Debug` build).

### Container build (optional, transitional)

A Docker image (`docker/Dockerfile`) mirrors this repo's full tool stack — ARM cross-compiler (pinned to the exact same toolchain release as the native Windows setup, ARM GNU Toolchain 14.3.rel1), plus a native `gcc`/`g++` and `clang-tidy` for the Testing/Linting workflows below — so the same image can eventually serve as both a local build environment and the CI image. **It's currently supplementary, not a replacement**: the native Windows toolchains (MSYS2 for host tests, a standalone LLVM install for clang-tidy) stay the primary documented path until the container's output has been validated for longer. Nothing about flashing/debugging changes or moves into the container — J-Link/Ozone need direct USB access, which containers can't do.

```powershell
./build-docker.ps1
```

Builds the image if it doesn't exist yet, then runs the firmware build inside it with the repo bind-mounted (no source is baked into the image). Output goes to `build-docker/`, **not** `build/` — a Windows-native and a Linux-container CMake configure can't safely share one build directory (`CMakeCache.txt` bakes in absolute compiler paths and ABI). `build-docker.ps1` passes `-DCMAKE_BUILD_TYPE=Debug` explicitly so its output matches `build/`'s current actual behavior (see above) rather than pico-sdk's undeclared `Release` default.

## Linting

[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) is configured via `.clang-tidy` at the repo root, with a deliberately lean check set: `bugprone-*`, `modernize-*`, `performance-*` (minus `modernize-use-trailing-return-type`, a purely stylistic check, and minus `clang-analyzer-cplusplus.NewDeleteLeaks` — see below). `cppcoreguidelines-*` is intentionally **not** enabled yet — it's too opinionated for the pre-refactor HAL code (see HAL section below), which is a known rewrite target anyway; revisit once that refactor lands. `HeaderFilterRegex` scopes diagnostics to `src/shared` and `src/projects`, excluding vendored `src/libs/*`. Existing findings have not been fixed repo-wide — this is tooling setup, not a cleanup pass.

Locally, `clang-tidy` comes from a standalone [LLVM release](https://github.com/llvm/llvm-project/releases) (`LLVM-*-win64.exe` on Windows, installed to its default location) — **not** MSYS2's `clang-tools-extra` package. MSYS2 UCRT64 is still required, just for the host test toolchain (see Testing below), not for clang-tidy itself.

**`clang-analyzer-*` (the Clang Static Analyzer) isn't in the `Checks:` glob above, but don't assume it's off.** CI's container runs clang-tidy from LLVM 18, and that version enables a default subset of `clang-analyzer-*` checks regardless of whether `Checks:` matches them — a version-specific quirk later clang-tidy releases fixed by actually respecting the glob (confirmed locally: the standalone LLVM 22 install correctly skips them). So `clang-analyzer-*` findings can show up in CI without a matching local repro unless your local clang-tidy happens to be old enough to share the bug — check the CI log directly rather than trusting a clean local run for these. Most are worth fixing (one caught a real uninitialized-field bug in `Button`); `clang-analyzer-cplusplus.NewDeleteLeaks` specifically is excluded because it false-positives inside vendored `src/libs/doctest/doctest.h`'s internal `String` class whenever a *new* test file's `CHECK`/`REQUIRE` macros get analyzed for the first time (doctest.h isn't covered by `HeaderFilterRegex`, but that doesn't suppress `clang-analyzer-*` diagnostics on LLVM 18 either — same underlying quirk).

Run it via `tools/clang-tidy.py`, not `clang-tidy` directly — it picks the right compile database (`build` vs `build-tests`, depending on which one actually compiles the file in question) and, for firmware code, the ARM cross-compile flags clang needs, automatically:

```powershell
python3 tools/clang-tidy.py tests/shared/button_test.cpp
python3 tools/clang-tidy.py src/shared/hal/rp2350/spi_dma_bus.cpp
```

clang-tidy parses with **clang's** frontend regardless of which compiler produced the compile command, and clang can't auto-detect the ARM GNU Toolchain's multilib header layout the way GCC can — without the extra target/sysroot/include flags the script adds automatically for firmware code, it fails outright (`unknown target CPU 'armv8-m.main+fp+dsp'`, then `'cstdint' file not found`) rather than just misbehaving. Requires `build/` and/or `build-tests/` already configured (`build.ps1`/`test.ps1`, or the Docker equivalents — the same script also works unmodified inside the container, via `--src-build-dir build-docker --tests-build-dir build-tests-docker`). Machine-specific paths (the ARM sysroot, clang-tidy's own Windows fallback location) live in `tools/paths.json`, not hardcoded in the script.

The script only takes a single file — a `.c`/`.cpp` translation unit, not a header or a directory (directory/recursive scanning was deliberately deferred; invoke it once per file if you need several, e.g. across a diff). Output is a single JSON document on stdout (`{"summary": {...}, "findings": [...]}`, or `{"summary": {"status": "error", ...}, "error": "..."}` for a setup failure like a missing binary or a file not in either compile database) — progress/diagnostic text goes to stderr instead, so stdout stays parseable. Exit code is 0 only when clean.

To lint everything relevant to the current branch instead of one file at a time, `tools/changed_files.py [--base main] [--ext .cpp,.h]` lists changed files as the union of two git-native sources — every file touched by a commit on the branch since it diverged from `--base` (what CI sees: a clean checkout), and every file with an uncommitted change right now, staged or not, including untracked files (what a developer or agent mid-edit sees). Deleted files are dropped by checking the path still exists, not by interpreting git's status letters. This replaces the old `tools/lint-diff.sh` (`clang-tidy-diff.py` parsing a unified diff, which silently reported "clean" on a malformed or empty diff) with a plain file list, so whole files go through `tools/clang-tidy.py` rather than diff hunks. `tools/lint_changed.py [--base main]` composes both — gets the changed-file list, runs `tools/clang-tidy.py` on each, and aggregates the per-file JSON results into one payload (`files_checked`, a merged `findings` list, and `file_errors` for any file that failed to lint) — and is what backs the CI step below.

## Testing

Hardware-independent logic (currently just `Button`) has host-side unit tests under `tests/`, built with a native compiler — **not** `arm-none-eabi-gcc` — since it's a separate CMake project from the firmware build (the root `CMakeLists.txt` unconditionally pulls in pico-sdk before `project()`, so it can't produce a host binary). Tests use [doctest](https://github.com/doctest/doctest), vendored as a single header at `src/libs/doctest/doctest.h` (pinned to v2.4.11, not a submodule).

```powershell
./test.ps1
```

Puts MSYS2 UCRT64 (`C:\msys64\ucrt64\bin`) first on `PATH` — required, since without it `arm-none-eabi-gcc` or MSVC's `cl` (also on `PATH` on this machine) can get picked up by CMake instead of a native compiler, and the failure mode is a confusing runtime error (e.g. `ctest` failing with a missing-DLL exit code), not a clear build error — then configures into `build-tests/`, builds, and runs the suite. Equivalent manual invocation from repo root once MSYS2 is already first on `PATH`:

```powershell
cmake -G "Ninja" -S tests -B build-tests -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The same commands also run inside the container from the Build section above (`docker run --rm -v ${PWD}:/workspace -w /workspace modular-midi-build bash -c "cmake -G Ninja -S tests -B build-tests-docker -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build-tests-docker && ctest --test-dir build-tests-docker --output-on-failure"`) — verified to pass identically. Still supplementary; MSYS2 stays primary for now.

Only code with no pico-sdk/FreeRTOS dependency belongs in `tests/` — e.g. `Button` is deliberately kept hardware-free (see Services below) so it can be tested this way. `ButtonService` now qualifies too: it depends on FreeRTOS only through the injected `TaskRunner` concept (see `architecture/SERVICES.md`), so a host build satisfies that concept with a fake (`tests/mocks/fake_task_runner.h`) instead of the real RTOS. `ST7789` (`src/shared/hal/drivers/st7789.h`) qualifies the same way, one level down the stack: it depends on hardware only through the `SpiBus`/`GpioOutputPin`/`Delay` concepts, satisfied on the host by `tests/mocks/fake_spi_bus.h`/`fake_output_pin.h`/`fake_delay.h` (`tests/shared/hal/drivers/st7789_test.cpp`) instead of `SpiDmaBus`/`OutputPin`/`PicoDelay`. The concrete rp2350 implementations of these concepts — `Pin`, `OutputPin`, `SpiDmaBus`, `PicoDelay`, `FreeRTOSTaskRunner`, `RttSink` — still touch pico-sdk/FreeRTOS directly and are not host-testable; only the logic layered on top of them is.

### TDD is required for hardware-independent logic

For any code that can go through the host build above (no pico-sdk/FreeRTOS dependency — the `Button`/`ButtonService` category), development is test-first:

1. Write a failing test in `tests/` for the behavior you're about to add or change.
2. Implement (or edit) until it passes.
3. Don't add untested logic to this category of code, and don't leave a red test in the tree.

Before treating any change to testable code as done, rebuild and rerun the full suite (`./test.ps1`, or `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure` if `build-tests/` is already configured) — a change isn't finished if this fails, or if new/changed behavior in `tests/`-covered code has no corresponding test.

Hardware-coupled code — the concrete concept implementations themselves (`Pin`, `FreeRTOSTaskRunner`, SPI/GPIO drivers, everything under `src/shared/hal/rp2350` today) — has no host-test harness, so TDD isn't mechanically enforceable there; verify those on target as usual. As more of the HAL and Services layer moves to the concepts-based design in `architecture/HAL.md`/`architecture/SERVICES.md`, extend `tests/` coverage to match rather than leaving new hardware-independent logic untested.

## Continuous Integration

GitHub Actions (`.github/workflows/ci.yml`) runs on every PR into `main` and every push to `main`: firmware build, host tests, and changed-file clang-tidy, inside the same `docker/Dockerfile` image used locally (see Build section above). A native Linux runner has no WSL2 boundary, so the local Docker slowdown noted there shouldn't apply here.

- **Firmware build & host tests**: the same commands documented in Build/Testing above, just run via `docker run` against a fresh checkout instead of `build-docker.ps1`/manual invocation.
- **Lint changed files**: `tools/lint_changed.py --base origin/main` runs `tools/changed_files.py` (see Linting above) to find every `.c`/`.cpp` file changed relative to `origin/main`, then lints each one with `tools/clang-tidy.py`, aggregating the per-file JSON results into one payload. This is deliberately scoped to changed files, not the whole repo: the pre-refactor HAL code (see Architecture below) already has a large volume of findings, so a repo-wide gate would force either a big upfront cleanup or a pile of `NOLINT` suppressions. New/changed code must be clean; untouched legacy code isn't retroactively flagged, and coverage grows organically as code actually gets touched. Requires the checkout to have `fetch-depth: 0` and `origin/main` fetched (both already set up in the workflow) so `tools/changed_files.py` can compute a merge base.

**Branch protection is enabled on `main`**: the `build-test-lint` check must pass, a PR is required (direct pushes are rejected, `enforce_admins` is on so this applies to the repo owner too — verified by attempting a direct push and confirming GitHub rejects it), and linear history is required (GitHub's plain "Merge commit" button is disabled; use "Squash and merge" or "Rebase and merge"). This is what actually enforces the Git workflow rule above, not just convention.

## Flash & debug

Flashing and debugging are done through SEGGER tools, not VS Code's built-in debugger:

- **Flash**: VS Code task `J-Link: Flash RP2350` (runs `JLink.exe` with `tools/flash.jlink`). Requires the J-Link install dir on `PATH` — see `tools/tools.md`.
- **Debug**: Open `tools/ozone.jdebug` in SEGGER Ozone. It's pre-configured for the RP2350 (Cortex-M33 core 0) over J-Link/SWD with the FreeRTOS-aware plugin, and loads `build/src/projects/hub-master/hub_master.elf`.
- **Logging (RTT)**: `printf`-style output goes over SEGGER RTT and appears automatically in Ozone's Terminal window during a debug session — see `rtt.md`. Use the global `g_log` instance (`src/shared/hal/rp2350/logger_instance.h`), e.g. `g_log.debug() << "value: " << x;` — a hardware-independent `Logger<SinkT>`/`LogStream<SinkT>` (`src/shared/logger.h`, stream-style `operator<<`, no heap) writing through the RP2350 `RttSink` (`src/shared/hal/rp2350/rtt_sink.h/.cpp`) — not `printf`/stdio (USB and UART stdio are explicitly disabled in `src/projects/hub-master/CMakeLists.txt`).

## Coding standard

Target modern C++, C++20 and onwards (concepts, `constexpr`/`consteval`, structured bindings, ranges where applicable) rather than older C-with-classes style. Enforced by `set(CMAKE_CXX_STANDARD 20)` in the root `CMakeLists.txt` (applies to `shared` and every `src/projects/<name>` target; the vendored `FreeRTOS-Kernel` submodule pins its own C++17 independently in its own `CMakeLists.txt`, so it's unaffected).

## Architecture

### Directory layout

```
src/
├── libs/       # third-party deps as git submodules: pico-sdk, FreeRTOS-Kernel, lvgl, RTT
├── shared/     # code shared across projects: HAL, event system, services
└── projects/   # individual firmware targets, e.g. hub-master
```

The CMake build is deliberately distributed: `src/`, `src/shared/`, `src/shared/hal/`, `src/shared/hal/drivers/`, `src/shared/hal/rp2350/`, `src/shared/services/`, and each `src/projects/<name>/` each have their own `CMakeLists.txt`, rather than one monolithic build file. `src/shared` builds into a single static `shared` library that each project links against.

Multi-platform selection for the HAL is intended to happen at the `src/shared/hal/CMakeLists.txt` level: `add_subdirectory(rp2350)` is the concrete platform implementation (currently the only one; a future `native_sim` or similar would be added as a sibling and selected via CMake cache variable — see `architecture/HAL.md`). `add_subdirectory(drivers)` is unconditional rather than platform-switched — `src/shared/hal/drivers/` holds peripheral drivers written entirely against HAL concepts (`SpiBus`, `GpioOutputPin`, `Delay`, ...), with no dependency, direct or transitive, on any specific platform's SDK, so the same driver code works under any platform implementation without modification.

### HAL — target design vs. current state

`architecture/HAL.md` documents the **intended** HAL architecture: zero-overhead (no virtual functions/vtables — static dispatch via C++20 concepts and templates), zero dynamic allocation, with peripheral identity (pin numbers, SPI instance) passed as runtime constructor parameters so there's one class per peripheral type rather than one per instance.

`src/shared/hal/pin_concept.h` (`GpioPin`), `src/shared/hal/gpio_output_pin_concept.h` (`GpioOutputPin`), `src/shared/hal/spi_bus_concept.h` (`SpiBus`), `src/shared/hal/delay_concept.h` (`Delay`), `src/shared/hal/display_driver_concept.h` (`DisplayDriver`), `src/shared/hal/log_sink_concept.h` (`LogSink`), `src/shared/hal/task_runner_concept.h` (`TaskRunner` — a scheduling/execution-context concept rather than a peripheral one; see `architecture/SERVICES.md`), their rp2350 implementations (`rp2350/pin.h`/`Pin`, `rp2350/output_pin.h`/`OutputPin`, `rp2350/spi_dma_bus.h/.cpp`/`SpiDmaBus`, `rp2350/pico_delay.h`/`PicoDelay`, `rp2350/rtt_sink.h/.cpp`/`RttSink`), and `src/shared/hal/drivers/st7789.h` (`ST7789<SpiT, DcPinT, RstPinT, DelayT>`) all now follow that design. `ST7789` has no CS template parameter — CS is owned by whichever concrete `SpiT` is in use (see `rp2350/spi_dma_bus.h`'s `SpiDmaBus`, which hands CS to the PL022 peripheral itself rather than a software-toggled pin). `src/shared/button.h`/`src/shared/services/button_service.h`/`src/shared/logger.h` (`Button<PinT, SinkT>`/`ButtonService<PinT, SinkT, RunnerT, QueueT>`/`Logger<SinkT>`) do too.

`src/shared/hal/rp2350/display.h` (`Display<DriverT>`) is templated the same way and consumes `ST7789` purely through the `DisplayDriver` concept — but isn't fully decoupled yet: it still calls pico-sdk's `add_repeating_timer_ms` directly for the LVGL tick, so it stays under `rp2350/` rather than `hal/drivers/` until that's pulled behind its own concept (a `PeriodicTimer`-shaped one, not `Delay` — registering a periodic callback is a different capability than blocking for a duration). When adding new HAL code, follow `architecture/HAL.md`'s concepts-based pattern — `pin_concept.h`/`Pin`/`Button` and `hal/drivers/st7789.h` are working examples of it.

### Concurrency model

FreeRTOS runs with **static allocation only** (`configSUPPORT_STATIC_ALLOCATION=1`, `configSUPPORT_DYNAMIC_ALLOCATION=0` in `src/projects/hub-master/FreeRTOSConfig.h`) — tasks are created with `xTaskCreateStatic` using explicitly declared `StackType_t`/`StaticTask_t` buffers, never `xTaskCreate`. No heap (`new`/`malloc`) is used anywhere in the firmware.

The RP2350 is dual-core (via `pico_multicore`): `main()` starts the FreeRTOS scheduler (blink/MIDI-priority tasks) on core 0, and launches a separate non-FreeRTOS bare loop (`display_task`) on core 1 for display/LVGL updates via `multicore_launch_core1`.

### Event System — target design vs. current state

`architecture/EVENT_SYSTEM.md` documents the **intended** event system architecture: producers and
consumers wired together at compile time (no runtime `subscribe()` registry), a single flat
`Event` (`src/shared/event/event_common.h`, a `struct` wrapping one `uint32_t` — top byte
`EventType` [`Button`, `Midi`, `UI`, `CustomStart = 100` reserved for new modules], remaining 24
bits an opaque payload interpreted by a per-type codec) as the only thing that ever sits in a
queue, and eventually an `EventRouter` that fans a single `publish()` out to every subscribed
consumer's queue without running any consumer logic on the producer's task.

`EventQueue<T, N>` (`src/shared/event/event_queue_concept.h`) is implemented, with `FreeRTOSEventQueue<Capacity>`
(`src/shared/hal/rp2350/freertos_event_queue.h`) as the rp2350 backing and `FakeEventQueue`
(`tests/mocks/fake_event_queue.h`) as the host double. `ButtonService` is the reference producer
(see Services below) and `SystemService<SinkT, RunnerT, QueueT>`
(`src/shared/services/system_service.h`) is the reference consumer — it logs each `EventType::Button`
event it receives (`"event: button <id> <state>"`), with other event types falling through a no-op
default case for now. `EventRouter` itself isn't built yet: with only one consumer so far, `main.cpp`
wires `ButtonService` and `SystemService` directly to the same `FreeRTOSEventQueue` instance rather
than through a router — see `architecture/EVENT_SYSTEM.md`'s Open Items for what's still deferred
(the router itself, MIDI/program-state codecs, drop-and-log overflow logging).

### Services

`architecture/SERVICES.md` documents the Services architecture: a Service wraps a hardware-independent unit of logic in its own execution context, injected via a `TaskRunner` concept (mirroring HAL.md's DIP approach for peripherals) rather than depending on FreeRTOS directly — which is also what makes it possible to give a Service a host-side test wrapper under `tests/` for real doctest + clang-tidy coverage. `ButtonService<PinT, SinkT, RunnerT, QueueT>` is the reference implementation: templated on the pin type (`GpioPin`), log sink type (`LogSink`), execution context (`TaskRunner`), and event queue (`EventQueue<Event>`, see Event System above), it owns up to `MAX_BUTTONS` (4) `Button<PinT, SinkT>*` instances and polls them from a dedicated statically-allocated task (`start()` → the injected `RunnerT`'s `run()` loop calling `Button::update(delta_time)` on each, then publishing one event per bit set in the returned transition mask into the injected `QueueT`). `Button` itself is HAL-agnostic — it's templated on any type satisfying `GpioPin` and handles debounce (4-sample shift register), long-press, and double-tap detection in software; `update()` returns a `ButtonTransition` bitmask of whatever happened during that call (`Pressed`/`Released`/`LongPressed`/`DoubleTapped` — a double tap resolves on the same tick as the release edge that completes it, so more than one bit can be set at once) rather than exposing latched getters, since a poll-every-tick producer needs to know what happened *this* tick, not level/sticky state. `is_pressed()` (level state) and `id()` remain as plain queries. `Button` deliberately has no pico-sdk dependency itself — it logs only through the injected `Logger<SinkT>` (see HAL section above) — so it stays host-testable (see Testing above); don't reintroduce a hardware-coupled dependency directly into `Button`/`ButtonService` without giving it a host-safe seam the way `LogSink`/`TaskRunner`/`EventQueue` do.

### Display

`Display<DriverT>` (`src/shared/hal/rp2350/display.h`, header-only) drives LVGL's flush callback from `display_task` on core 1 against any driver satisfying `DisplayDriver` — `src/shared/hal/drivers/st7789.h`'s `ST7789` today. Completion of the driver's async pixel-burst DMA transfer is polled (`driver.is_busy()`), not interrupt-driven: no `DMA_IRQ_0` handler is registered anywhere, which sidesteps the multi-instance-vs-shared-IRQ-line problem entirely once more than one display exists (4 are planned — see Project section above). LVGL is configured to build with no demos/examples/ThorVG and no default widget set (`src/CMakeLists.txt`), with the real config at `src/projects/hub-master/lv_conf.h`.
