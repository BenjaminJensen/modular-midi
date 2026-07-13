# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`modular-midi` is a multi-project embedded firmware repo. The first product, `hub-master`, is an RP2350-based MIDI controller with 4 buttons and 4 displays (one display shows live info for its corresponding button). Future hardware projects will live alongside it and share common infrastructure.

## Git workflow

**Never commit directly to `main`.** All work happens on a topic branch; `main` is only updated via fast-forward merge or PR. Before committing, confirm the current branch is not `main`.

## Build

Requires `arm-none-eabi-gcc`, `ninja`, and `cmake` on PATH (see `build.ps1` for the exact toolchain paths this repo was set up with, and `setup.md` for the tool list). There is no build container yet.

```powershell
./build.ps1
```

This configures into `build/` with Ninja (`-DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350`) and builds. Equivalent manual invocation from repo root:

```powershell
cmake -G "Ninja" -B build -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 -DPICO_PLATFORM=rp2350
cmake --build build
```

Output ELF for the current project: `build/src/projects/hub-master/hub_master.elf` (also produces `.uf2`, `.bin`, `.map` via `pico_add_extra_outputs`).

There is no linter configured in this repo currently.

## Testing

Hardware-independent logic (currently just `Button`) has host-side unit tests under `tests/`, built with a native compiler — **not** `arm-none-eabi-gcc` — since it's a separate CMake project from the firmware build (the root `CMakeLists.txt` unconditionally pulls in pico-sdk before `project()`, so it can't produce a host binary). Tests use [doctest](https://github.com/doctest/doctest), vendored as a single header at `src/libs/doctest/doctest.h` (pinned to v2.4.11, not a submodule).

```powershell
cmake -G "Ninja" -S tests -B build-tests -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The explicit compiler flags avoid ambiguity if `arm-none-eabi-gcc` or MSVC's `cl` are also on `PATH`. This machine's native toolchain is MSYS2 UCRT64 (`C:\msys64\ucrt64\bin`).

Only code with no pico-sdk/FreeRTOS dependency belongs in `tests/` — e.g. `Button` is deliberately kept hardware-free (see Services below) so it can be tested this way; `ButtonService` and other FreeRTOS-task wrappers are not host-testable and aren't covered here.

### TDD is required for hardware-independent logic

For any code that can go through the host build above (no pico-sdk/FreeRTOS dependency — the `Button` category), development is test-first:

1. Write a failing test in `tests/` for the behavior you're about to add or change.
2. Implement (or edit) until it passes.
3. Don't add untested logic to this category of code, and don't leave a red test in the tree.

Before treating any change to testable code as done, rebuild and rerun the full suite — a change isn't finished if this fails, or if new/changed behavior in `tests/`-covered code has no corresponding test:

```powershell
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Hardware-coupled code (SPI/GPIO drivers, FreeRTOS task wrappers, anything under `src/shared/hal/rp2350` today) has no host-test harness yet, so TDD isn't yet mechanically enforceable there — verify those on target as usual. As more of the HAL moves to the concepts-based design in `architecture/HAL.md`, extend `tests/` coverage to match rather than leaving new hardware-independent logic untested.

## Flash & debug

Flashing and debugging are done through SEGGER tools, not VS Code's built-in debugger:

- **Flash**: VS Code task `J-Link: Flash RP2350` (runs `JLink.exe` with `tools/flash.jlink`). Requires the J-Link install dir on `PATH` — see `tools/tools.md`.
- **Debug**: Open `tools/ozone.jdebug` in SEGGER Ozone. It's pre-configured for the RP2350 (Cortex-M33 core 0) over J-Link/SWD with the FreeRTOS-aware plugin, and loads `build/src/projects/hub-master/hub_master.elf`.
- **Logging (RTT)**: `printf`-style output goes over SEGGER RTT and appears automatically in Ozone's Terminal window during a debug session — see `rtt.md`. Use `Logger::log(...)` / the `LOG_DEBUG(...)` macro (`src/shared/hal/rp2350/async_logger.h`), not `printf`/stdio (USB and UART stdio are explicitly disabled in `src/projects/hub-master/CMakeLists.txt`).

## Coding standard

Target modern C++, C++20 and onwards (concepts, `constexpr`/`consteval`, structured bindings, ranges where applicable) rather than older C-with-classes style.

## Architecture

### Directory layout

```
src/
├── libs/       # third-party deps as git submodules: pico-sdk, FreeRTOS-Kernel, lvgl, RTT
├── shared/     # code shared across projects: HAL, event system, services
└── projects/   # individual firmware targets, e.g. hub-master
```

The CMake build is deliberately distributed: `src/`, `src/shared/`, `src/shared/hal/`, `src/shared/hal/rp2350/`, `src/shared/services/`, and each `src/projects/<name>/` each have their own `CMakeLists.txt`, rather than one monolithic build file. `src/shared` builds into a single static `shared` library that each project links against.

Multi-platform selection for the HAL is intended to happen at the `src/shared/hal/CMakeLists.txt` level (currently only `rp2350` exists; a future `native_sim` or similar would be added as a sibling and selected via CMake cache variable — see `architecture/HAL.md`).

### HAL — target design vs. current state

`architecture/HAL.md` documents the **intended** HAL architecture: zero-overhead (no virtual functions/vtables — static dispatch via C++20 concepts and templates), zero dynamic allocation, with peripheral identity (pin numbers, SPI instance) passed as runtime constructor parameters so there's one class per peripheral type rather than one per instance.

The current code in `src/shared/hal/rp2350/`, `src/shared/i_pin.h`, and `src/shared/button.h` predates that design and uses classic runtime polymorphism instead (`IPin` is an abstract base class with a `virtual bool read()`, implemented by `Pin`). It compiles and runs correctly on target — pico-sdk, FreeRTOS, and lvgl are all in a working integrated configuration — but is a refactor target, not a reference implementation to copy from. When adding new HAL code, follow `architecture/HAL.md`'s concepts-based pattern rather than mirroring `IPin`/`Pin`.

### Concurrency model

FreeRTOS runs with **static allocation only** (`configSUPPORT_STATIC_ALLOCATION=1`, `configSUPPORT_DYNAMIC_ALLOCATION=0` in `src/projects/hub-master/FreeRTOSConfig.h`) — tasks are created with `xTaskCreateStatic` using explicitly declared `StackType_t`/`StaticTask_t` buffers, never `xTaskCreate`. No heap (`new`/`malloc`) is used anywhere in the firmware.

The RP2350 is dual-core (via `pico_multicore`): `main()` starts the FreeRTOS scheduler (blink/MIDI-priority tasks) on core 0, and launches a separate non-FreeRTOS bare loop (`display_task`) on core 1 for display/LVGL updates via `multicore_launch_core1`.

### Event system

`src/shared/event/event_common.h` defines a shared, allocation-free event representation: a `struct Event` wrapping a single `uint32_t` — top byte is `EventType` (`Button`, `Midi`, `UI`, with `CustomStart = 100` reserved for new modules to claim their own IDs), remaining 24 bits are an opaque payload. `src/shared/event/event_engine.h` (the dispatch/routing engine consuming these events) is a work in progress.

### Services

Services under `src/shared/services/` wrap a HAL primitive in its own static FreeRTOS task. Example: `ButtonService` owns up to `MAX_BUTTONS` (4) `Button*` instances and polls them from a dedicated statically-allocated task (`start(priority)` → `xTaskCreateStatic`-backed `run()` loop calling `Button::update(delta_time)` on each). `Button` itself is HAL-agnostic — it takes an `IPin*` and handles debounce (4-sample shift register), long-press, and double-tap detection in software, exposing the result via `is_pressed()`/`was_long_pressed()`/`was_double_tapped()`. It deliberately has no logging or other pico-sdk dependency so it stays host-testable (see Testing above) — don't reintroduce a hardware-coupled include like the old `async_logger.h`/`LOG_DEBUG` without giving it a host-safe seam.

### Display

`Display` (`src/shared/hal/rp2350/display.h/.cpp`) wraps an ST7789 driver (`st7789.h/.cpp`) and drives LVGL's flush callback from `display_task` on core 1. LVGL is configured to build with no demos/examples/ThorVG and no default widget set (`src/CMakeLists.txt`), with the real config at `src/projects/hub-master/lv_conf.h`.
