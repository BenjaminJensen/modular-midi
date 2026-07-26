# LVGL / Render Engine architecture review (carried over from system-service-display-labels)

## What was implemented (committed as `f660c48` on branch `system-service-display-labels`)

- `SystemService` now consumes `Button` events and writes a `"B<id>: <State>"` label (e.g. `"B0: Pressed"`) to display 0 via the Render Engine `Mailbox`, in addition to its existing debug log line. State text uses the enum member name verbatim (`Pressed`/`Released`/`LongPressed`/`DoubleTapped`) via a new `to_label_string()` in `button_payload.h`.
- `main.cpp`'s temporary `render_mailbox.write(0, Label::make("Hello", ...))` seed line is removed; `SystemService` now takes a `Mailbox<DisplayCount>&` constructor param.
- Files touched: `src/shared/services/system_service.h`, `src/shared/services/button_payload.h`, `src/projects/hub-master/main.cpp`, `src/shared/hal/rp2350/display.h`, `src/shared/hal/rp2350/render_engine.h`, plus their tests.

## Bugs found and fixed along the way

1. **White-on-white labels**: `Display::init()` never set an explicit screen background; LVGL's base style (with `LV_USE_THEME_DEFAULT` off) left it white, and `ColorPalette::Default` is also white. Fixed by explicitly setting the screen background to black.
2. **Render deadlock (the big one)**: `flush_cb` was async (start DMA, return immediately, signal completion later via polling from `task()`). LVGL's `lv_refr.c` has a `wait_for_flushing()` that synchronously spins on `disp->flushing` before it can process a second chunk within one redraw pass (both single- and double-buffered modes hit this, just at different points — confirmed by reading LVGL 9.5.0 source directly). That spin can only be resolved by an interrupt preempting it (the standard LVGL assumption) — which this codebase deliberately doesn't use, to avoid a shared-IRQ-line problem across the planned 4 displays. Fix: made `flush()` block until the DMA transfer physically completes and call `lv_display_flush_ready()` itself before returning, so `disp->flushing` is never left set across a `lv_timer_handler()` call. Draw buffer sized at half screen height (~21.6 KiB, single-buffered) — a full redraw is at most 2 chunks.
3. Along the way, discovered and fixed a pre-existing bug: the original draw buffer array was sized for the *full screen* while only declaring 1/10 that height to LVGL — an unexplained, undocumented mismatch (not something either of us could find a rationale for).

## RAM budget findings (RP2350: 512 KiB main SRAM)

- Current single-display, single-buffered, half-height-buffer firmware: ~73.4 KiB `.bss` (~52 KiB fixed baseline + ~21.6 KiB buffer).
- Projected for 4 displays at this design: ~138 KiB total, ~73% SRAM free.
- A full-screen-sized single buffer per display (needed for the round-robin idea below) would be ~168.6 KiB total for 4 displays — still comfortable.

## Open architectural question (why a fresh design session is needed)

The idea: while display 0's DMA transfer is physically in flight, core 1 should move on to start rendering/flushing display 1, 2, 3 — each has its own independent SPI peripheral + DMA channel (per `architecture/RP2350 Quad-Display Concurrent Architect.md`), so this is real, exploitable hardware parallelism. The current blocking `flush()` design wastes this entirely: a blocking wait for display 0 stalls the whole core-1 loop and prevents any progress on the other 3 displays.

Getting that concurrency back requires reverting to async `flush()` — but async only avoids the deadlock if every redraw is guaranteed to be a single `flush_cb` call, which means sizing each display's buffer for the *full screen* (not chunked), plus restructuring the core-1 loop to round-robin `update()` across N independent `RenderEngine` instances instead of spinning on just one.

This surfaced a bigger question worth its own session: **does this project actually need LVGL's value-add** (widget tree, styling, layout, animation), or is the real requirement narrow enough (each display just shows a short status label) that a small custom renderer — no refresh/flush state machine, no single-display-at-a-time assumption baked in — would sidestep this whole class of problem rather than requiring a round-robin scheduler bolted around LVGL's synchronous refresh cycle. Worth deciding before investing in the 4-display concurrency work.

## Loose ends

- LVGL version in use: **9.5.0** (vendored submodule, `src/libs/lvgl`).
- `tools/ozone.jdebug`'s local device-target change (`RP2350_M33_0` → `RP2350_M33_1`, from debugging core 1) was reverted, not committed - Ozone will default back to core 0 next session.
