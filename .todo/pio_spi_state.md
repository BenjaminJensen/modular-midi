# PioSpiBus (PIO+DMA SPI transport) debugging state — carried over from branch `pio-spi-bus`

Implements `architecture/RP2350 Quad-Display Concurrent Architecture.md`'s `PioSpiBus` design: a
new `SpiBus` implementation driving the ST7789 over one PIO0 state machine + DMA instead of the
hardware SPI1 peripheral (`SpiDmaBus`). Goal was to swap it in for the single wired display in
`main.cpp`.

**Status: still not working, but root cause is now identified — CS is software-timed and races
the PIO state machine. Not yet fixed.** `main.cpp` currently runs `SpiDmaBus` (hardware SPI), not
`PioSpiBus` — see "Current code state" below. This file was last a flat bug list; it now also
covers a second, sibling debugging project and the CS investigation that followed it.

## Companion project: `pio-spi-bringup`

Debugging `PioSpiBus` inside the full firmware (FreeRTOS + multicore + LVGL) made it hard to get a
clean, single-shot oscilloscope/logic-analyzer capture of one SPI burst. A standalone sibling
project was created at `F:\git\electronics\pio-spi-bringup` (own git repo, **not** nested inside
this one) specifically for that: just `PioSpiBus`/`SpiDmaBus` + `ST7789` + the small HAL concept
headers, copied out of `src/shared`, running from a bare `main()` loop with no scheduler. It points
at this repo's `pico-sdk` submodule by absolute path rather than vendoring a second copy. See its
own `README.md` for build/flash/debug instructions (same J-Link/Ozone workflow, own
`tools/flash.jlink` + `tools/flash_spi_dma.jlink`).

It builds two executables:
- `pio_spi_bringup` (`main.cpp`) — `PioSpiBus`, with a set of hand-toggled test phases
  (`phase1_walking_bits`, `phase1_1_walking_bits`, `phase2_raw_pixels`, `phase2_1_raw_pixels`,
  `phase3_st7789_driver`, `phase4_command_then_pixel_burst`, `phase4_1_..._with_delay`) run one at
  a time from `main()` by commenting/uncommenting.
- `spi_dma_bringup` (`main_spi_dma.cpp`) — the identical phase sequence through `SpiDmaBus`
  (hardware SPI1), as a known-working reference to diff against.

All findings below marked "via `pio-spi-bringup`" came from this harness, not the full firmware.

## What's confirmed working (via `pio-spi-bringup`)

- **Bit order is correct**: phase 1 (walking-bit bytes `0x01`..`0x80` via `write_blocking()`,
  scope-verified) shows clean MSB-first shifting, matching the left-justify (`data[i] << 24`) +
  shift-left OSR config.
- **Byte order/multi-byte framing is correct**: phase 1_1 (`0xAA`,`0x55` in one CS-framed
  transaction) shows correct back-to-back MSB-first bytes with no reordering — this also stands in
  for `set_window()`'s 4-byte CASET/RASET writes, which use the same code path.
- **The two-stage DMA byte-swap+stream pixel path is correct**: phase 2 (single-pixel `0xAA55` and
  the last beat of a 4-pixel burst `{0x0001,0x0002,0x0004,0x0008}`) both reconstruct the original
  pixel value's bit order on the wire — this was the thing explicitly flagged below as never
  independently verified, and it now has scope evidence backing it. Note the other 3 single-pixel
  values and the burst's first 3 beats were never individually re-confirmed (only inferred from
  these two data points + the earlier oscilloscope pass).
- **`SpiDmaBus` control test (in `pio-spi-bringup`)**: `spi_dma_bringup`'s phase 3 (`init()` +
  `update()`, draws a small red block) works — correctly shows the red block *and* random-noise
  GRAM content everywhere else (expected: nothing clears the rest of the screen in this minimal
  harness, so stale/uninitialized controller memory renders as noise). This is a clean control:
  identical driver code, pins, and harness as the failing `PioSpiBus` case, so it rules out wiring,
  `ST7789`, and the harness itself as the bug.
- **`PioSpiBus`'s total non-response is a stronger signal than "just pixels are wrong"**: with
  `PioSpiBus`, the *entire* display stays uniformly white — no noise anywhere, unlike the
  `SpiDmaBus` control. That's more consistent with "no command in the sequence ever takes effect"
  than with "commands work but pixel data is corrupted."
- **Real root cause found via logic analyzer + oscilloscope**: CS deasserts (`cs.write(true)`) far
  too early relative to the SCL burst it's supposed to frame — visually roughly 1/8–1/10th the
  width of the actual clock burst, confirmed on phase 1's individual walking-bit bytes. This is
  the actual explanation for the total non-response: if CS cuts every byte short, the ST7789 never
  receives a single complete, valid command. See "Root cause" section below for the full
  diagnosis and what's been tried.

## Root cause: CS is software-timed and races the SM — not yet fixed

`PioSpiBus::write_blocking()`/`busy()` decide when it's safe to raise CS (a plain CPU-driven
`OutputPin`) by polling `TXSTALL` (`pio->fdebug`), a proxy for "has the state machine actually
finished shifting." This is fundamentally a software-timing bridge between two independently
clocked things (the CPU's poll loop vs. the PIO SM's own, heavily clock-divided cycle), and the
logic analyzer/scope evidence shows it isn't reliably safe — CS goes high while the SM (per the
scope) is still mid-burst.

**Attempted fix (did not work)**: strengthened the completion check to require the TX FIFO
observed empty *in addition to* `TXSTALL` (`pio_sm_is_tx_fifo_empty(m_pio, m_sm)`), on the theory
that a stale `TXSTALL` reading left over from the *previous* call could let the poll loop exit
before the SM had done any real work on the newly-pushed byte. Re-verified on oscilloscope after
this fix (cross-checked against the logic analyzer, which "looked strange") — **CS still shows the
same early-deassert behavior on the last transfer.** This fix is applied in `pio-spi-bringup`'s
`pio_spi_bus.cpp` (`write_blocking()` and `busy()`) but did not resolve the bug; not yet reverted,
not yet ported anywhere else.

**Identified real fix (not yet implemented)**: `pico-examples/pio/spi/spi.pio` handles CS as a
*second side-set bit generated by the state machine itself*, not a CPU-toggled GPIO:

```
public entry_point:                 ; Must set X,Y to n-2 before starting!
    pull ifempty       side 0x2 [1] ; Block with CSn high (minimum 2 cycles)
.wrap                               ; Note ifempty to avoid time-of-check race
```

`pull ifempty` is a blocking pull — stalling on it (no new FIFO data) is exactly what sets
`TXSTALL`, and the side-set on that *same instruction* is what drives CS high. CS-high and
`TXSTALL` become the same hardware event on the same PIO cycle, so there's no longer a software
race to get right — completion detection and CS's actual electrical state can't drift apart by
construction. Adopting this for `PioSpiBus` means: extend `st7789_8bit_program`'s side-set to 2
bits (SCL + CS), and — since the reference's CS variant can't use autopull (refills need to be
explicit so the program can hook custom side-set/CS timing at exactly the right point) — switch
from `sm_config_set_out_shift(..., autopull=true, threshold=8)` to manual `pull`/`jmp !osre`
control, matching the reference's structure. Our version can be simpler than the reference: no
`MISO`/`in pins` needed (TX-only), and possibly no `X--` bit-count-reload complexity if we keep a
fixed 8-bit frame size. **This has not been implemented yet, in either `pio-spi-bringup` or here.**

**Also confirmed working (and now adopted here)**: the *native* SPI1 peripheral doesn't need this
workaround — GPIO9 (our CS pin) is literally SPI1's `CSn` hardware alternate-function pin (RP2350
GPIO function table). With `CPHA=1` (which `SpiDmaBus::set_format()` already sets), the PL022
hardware holds CS low across continuous back-to-back words and only releases it on a genuine gap
— i.e. hardware-driven, race-free CS, for free, on the native-SPI path. This is what's now wired
into `main.cpp` (see below). The `PioSpiBus`/PIO side doesn't get this for free — PIO has no
built-in CS concept — hence the side-set redesign above.

**Loose end**: `pio-spi-bringup`'s phase 4_1 (isolates the `write_blocking()`→`write_dma()`
transition with a manual 1ms gap inserted, to distinguish a CS/panel timing violation from a real
logic bug) was wired up and built, but its actual result on hardware was never confirmed/reported
back before the investigation moved to the logic-analyzer/CS-timing angle. Worth revisiting once
the CS redesign is in, if the picture is still unclear.

## Bugs found and fixed along the way (still valid)

1. **Command byte not left-justified**: `pio_sm_put_blocking()` is a plain 32-bit register store
   (`pio->txf[sm] = data`), not a narrow bus write — no automatic justification. Was pushing raw
   `data[i]` (byte in the low 8 bits); with the SM's shift-left/MSB-first + 8-bit-autopull-threshold
   config, this shifted out 24 garbage zero-bits before ever reaching the real byte. Fixed:
   `pio_sm_put_blocking(m_pio, m_sm, static_cast<uint32_t>(data[i]) << 24)`.
2. **`write_blocking()` returned before the byte physically finished clocking out**:
   `pio_sm_put_blocking()` only blocks until the FIFO has room, not until the SM has actually
   shifted the bits onto SCL — unlike `SpiDmaBus`'s `spi_write_blocking()`, which is genuinely
   hardware-blocking. Fixed (at the time) by spinning on the `TXSTALL` sticky flag before
   returning — **later shown by the logic analyzer to still not be sufficient**; see "Root cause"
   above.
3. **Direct single-DMA pixel path never worked**: original design (matching the architecture doc
   literally) was one `DMA_SIZE_16`+`BSWAP` transfer straight into the FIFO, targeting what I
   theorized was the "upper halfword lane". Confirmed via scope: burst had correct timing but
   **all-zero data** on SDA. Replaced with the current two-stage DMA (RAM→RAM swap, then
   `DMA_SIZE_8` stream) — since independently scope-verified correct, see above.
4. **Stage-1 (RAM→RAM byte-swap) DMA missing `write_increment`**: fixed via
   `channel_config_set_write_increment(&swap_c, true)`.
5. **Stage-2 destination address was wrong**: fixed to a plain base-address narrow (8-bit) write to
   `txf[sm]`, relying on RP2040/2350 IO-fabric narrow-store replication, matching
   `pico-examples/pio/st7789_lcd`'s own technique.
6. **SCL idle-low vs `SpiDmaBus`'s CPOL=1 idle-high — confirmed NOT the bug.** Also directly tested:
   inverted SCL's electrical polarity via `gpio_set_outover(scl_pin, GPIO_OVERRIDE_INVERT)` to try
   CPOL=1 idle-high on the PIO side. **No change** to phase 3's behavior — ruled out experimentally,
   not just by analogy to the reference example as originally noted here.

## Things checked and ruled out (no bug found)

- PIO0 peripheral reset/clock gating, clock-init ordering vs C++ static constructors, RP2350 pad
  isolation, GPIO function-select correctness for PIO0, and `sda_pin`/`scl_pin` parameter-order —
  all previously verified against pico-sdk source, see prior revisions of this file for detail.
- **CPOL/idle-polarity** — directly tested via `gpio_set_outover`, no effect (see bug #6 above).
- A software race in `write_blocking()`'s `TXSTALL` check specifically around FIFO-empty timing —
  the FIFO-empty-guard fix didn't change the observed CS behavior, so if there is a race, it isn't
  (only) that one. Doesn't rule out the general "CS is software-timed" diagnosis — see above.

## Current code state

- **`src/projects/hub-master/main.cpp`**: reverted from `PioSpiBus` to `SpiDmaBus`
  (`spi1, sck=10, tx=11, cs=9, 10 MHz`) — **confirmed working on hardware** (full firmware,
  button/label flow, back at the original 10 MHz). `PioSpiBus` is not currently wired into the
  real firmware.
- **`src/shared/hal/rp2350/spi_dma_bus.h`/`.cpp`**: `SpiDmaBus` constructor now takes a `cs_pin`
  and configures it `GPIO_FUNC_SPI` (hardware-driven CS) instead of leaving CS to the caller.
  Requires `CPHA=1` (already set in `set_format()`) — see "Root cause" above for why.
- **`src/shared/hal/rp2350/null_output_pin.h`**: removed as post-debugging cleanup. It was a
  `GpioOutputPin` stub whose `write()` was a no-op, passed as `ST7789`'s CS template parameter
  while CS was still nominally part of `ST7789`'s interface. Once CS became fully hardware-owned
  by `SpiDmaBus` (see above), `ST7789`'s `CsPinT` template parameter and `m_cs` member were dead
  weight — every `m_cs.write(...)` call site was already commented out — so both `ST7789`'s CS
  plumbing and this stub were deleted outright rather than left wired in. `ST7789` is now
  templated only on `SpiT`/`DcPinT`/`RstPinT`/`DelayT`.
- **`src/shared/hal/rp2350/pio_spi_bus.cpp`/`.h`**: unchanged from the two-stage-DMA design
  described above; still has the (insufficient) FIFO-empty+TXSTALL guard from the attempted fix.
  Not currently wired into `main.cpp`.
- **`src/shared/hal/rp2350/st7789_spi.pio`**: unchanged 2-instruction "dumb serializer" — does
  *not* yet have the CS-side-set redesign.
- **`F:\git\electronics\pio-spi-bringup`** (sibling repo): has all the test-phase infrastructure
  described above, plus the FIFO-empty guard fix in its own copy of `pio_spi_bus.cpp`. Its
  `st7789_spi.pio` currently still has the `gpio_set_outover` CPOL-inversion experiment applied
  (confirmed no-effect, harmless, not reverted).

## Next step

Implement the CS-in-PIO-side-set redesign (see "Root cause" above) in `pio-spi-bringup` first —
extend `st7789_8bit_program` to 2-bit side-set (SCL + CS), switch off autopull in favor of manual
`pull`/`jmp !osre` control, drop `PioSpiBus`'s software CS entirely. Re-run phases 1 through 4 there
to confirm CS now tracks the SM's actual completion with no gap, then re-attempt phase 3/4 (the
real `ST7789` sequence) to see if the panel finally responds. Only port back into this repo's
`pio_spi_bus.cpp`/`st7789_spi.pio` once confirmed there.

## Environment/tooling gotchas (unrelated to the bug itself, worth knowing)

- `build.ps1`/`cmake --build` alone can't build any `.pio`-touching change without a native C/C++
  compiler on `PATH` (in addition to `arm-none-eabi-gcc`): `pico_generate_pio_header` builds
  `pioasm` as a *native host* tool. Prepend a native compiler's `bin` dir (e.g. LLVM's) to `PATH`
  before building. Same requirement applies in `pio-spi-bringup`.
- `.clang-tidy`'s `HeaderFilterRegex` (`'.*/src/(shared|projects)/.*'`) substring-matches the
  pioasm-generated header path, surfacing warnings on pioasm's own boilerplate for any `.pio` file
  in this repo. Not fixed — flagged as a discovered gap, no decision made yet on whether to tighten
  the regex.

## Inspiration and references

[st7789 driver implementation with PIO example](https://github.com/raspberrypi/pico-examples/tree/master/pio/st7789_lcd)
[PIO SPI implementation, including the CS-via-side-set programs (`spi_cpha0_cs`/`spi_cpha1_cs`) that are the basis for the identified next fix](https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/spi.pio)
