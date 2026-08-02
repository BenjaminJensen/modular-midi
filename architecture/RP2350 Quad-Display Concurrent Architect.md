# RP2350 Quad-Display Concurrent Architecture

Target design for driving all 4 `hub-master` displays **concurrently** from a single PIO block,
distilled from a design conversation (Gemini, 2026-07-25) exploring RP2350 PIO/DMA options, and
stress-tested against the current codebase in a follow-up `/grilling` session (2026-07-25).
This is a **design exploration, not yet implemented**, but every open question raised by the
source conversation has now been resolved into an explicit decision — see §8. The only thing
left genuinely open is the physical board pinout (§2), which doesn't affect the design itself.

## 0. Relationship to the existing HAL (`SpiBus` / `ST7789`)

The codebase already has an `SpiBus` concept (`src/shared/hal/spi_bus_concept.h`:
`set_format(bits)`, `write_blocking(bytes, n)`, `write_dma(pixels, len)`, `busy()`) and an
`ST7789<SpiT, DcPinT, RstPinT, DelayT>` driver (`src/shared/hal/drivers/st7789.h`) written purely
against that concept — it implements command-vs-pixel mode switching and a polling
`wait_idle()`/`is_busy()` pattern. **`ST7789` has no CS notion at all** — this is a correction to
this doc's original assumption (see the CS callout below, added after actually building and
debugging `PioSpiBus` on hardware — see `.todo/pio_spi_state.md`).

**Decision: the PIO+DMA transport is a new concrete implementation of `SpiBus` — `PioSpiBus`
— not a parallel/replacement driver.** `ST7789<PioSpiBus, DcPinT, RstPinT, DelayT>` is used
completely unchanged, one instance per display, each backed by a different PIO0 SM:

- `write_blocking()` → CPU writes the command byte(s) directly into that SM's TX FIFO (§4's
  command-mode path).
- `write_dma()` → configures that SM's DMA channel for `DMA_SIZE_16` + `BSWAP` (§4's pixel-mode
  path).
- `set_format(bits)` is close to a no-op on the PIO side — the PIO program always pulls 8 bits
  regardless of whether the CPU or DMA is feeding it (§3) — but is kept so the concept's contract
  stays uniform across `SpiBus` implementations.
- `busy()` — see §5's polling decision.

**CS is owned by the concrete `SpiBus` implementation, not `ST7789` — and for PIO this needed
real design work, contradicting this doc's original "no new design" call.** This section
originally assumed CS would stay a plain `GpioOutputPinConcept` that `ST7789` toggles itself, one
per SM ("CS unique per SM, toggled in software" — the source conversation's requirement, taken at
face value). That's since been superseded on two fronts:

- `ST7789` was refactored to drop its CS template parameter/member entirely — CS isn't its
  concern at any level now, for any `SpiT`.
- Actually debugging `PioSpiBus` on hardware (`F:\git\electronics\pio-spi-bringup`, full findings
  in `.todo/pio_spi_state.md`) found that a CPU-toggled CS GPIO has a genuine race against the PIO
  SM: polling `TXSTALL`/FIFO-empty as a proxy for "the SM actually finished shifting" isn't
  reliable, so software CS deasserts before the last byte finishes clocking out — this was the
  root cause of `PioSpiBus`'s total non-response in that investigation. Unlike the native SPI1
  path (where `SpiDmaBus` now hands CS to the PL022 peripheral itself, GPIO_FUNC_SPI on the CSn
  alt-function pin, for free hardware-driven CS), PIO has no built-in CS concept, so there's
  nothing to hand it off to.
- **Identified fix (not yet implemented — see `.todo/pio_spi_state.md`'s "Next step"):** generate
  CS as a second `side-set` bit driven by the SM itself, matching
  `pico-examples/pio/spi/spi.pio`'s `spi_cpha1_cs` pattern (`pull ifempty side 0x2 [1]` — the
  blocking `pull` that sets `TXSTALL` and the side-set that drives CS high happen on the same PIO
  instruction, so completion-detection and CS's electrical state can't drift apart by
  construction). This means §3's "dumb serializer" program below needs extending to a 2-bit
  side-set (SCL + CS) with manual `pull`/`jmp !osre` control instead of autopull — not yet done in
  either `pio-spi-bringup` or here.

## 1. Core decision: one PIO block, four State Machines

- All 4 displays are driven from **`PIO0`**, one State Machine (SM0–SM3) per display.
- The ST7789 serializer program is loaded into `PIO0`'s shared 32-instruction memory **once**;
  each SM runs it from its own independent Program Counter, so the four displays don't have to be
  in lock-step with each other.
- `PIO1`/`PIO2` are left completely free for other future work (audio, rotary encoders, etc.).

## 2. Pin assignment strategy

| Signal | Scope | Rule |
|---|---|---|
| SCL (clock) | Unique per SM | Driven via that SM's `side-set` pin. **Never shared** — concurrent SMs toggling a shared clock line would collide/garble. |
| SDA (data) | Unique per SM | Driven via that SM's `out` pin. |
| CS (chip select) | Unique per SM | Driven via that SM's second `side-set` bit, not a CPU-toggled GPIO — see §0's CS callout for why a software-toggled pin doesn't work here. |
| DC (data/command) | Unique per SM | |
| RESET | **Shared** across all 4 displays | One GPIO, toggled once at boot. |
| Backlight PWM | **Shared** across all 4 displays | One PWM channel/GPIO, tied like RESET — no per-display brightness requirement exists. |

The RP2350's per-SM IO mapping guarantees no electrical crosstalk between SMs even though they
share a PIO block and instruction memory: each SM only ever drives the GPIOs it was configured
with. Pin numbers used in the source conversation (e.g. SM0 SDA=GPIO2/SCL=GPIO3, SM1=4/5,
SM2=6/7, SM3=8/9) are **illustrative only**. Actual board pinout is the one remaining open item
(§8) — not a design decision, just unassigned hardware wiring — with the constraint that each
display needs its own SDA/SCL/CS/DC group (ideally SDA/SCL kept numerically close together per
the PIO mapping note above), plus one shared RESET GPIO and one shared backlight PWM GPIO.

## 3. PIO program: "dumb serializer"

The PIO program is deliberately minimal — it doesn't know or care whether it's shifting out a
command byte or pixel data, and doesn't need separate code paths for 8-bit vs. 16-bit transfers:

```
.program st7789_8bit
.side_set 1 opt                ; SCL is the side-set pin

public entry_point:
    out pins, 1    side 0 [1]  ; shift 1 bit to SDA, set SCL low, wait 1 cycle
    jmp entry_point side 1 [1] ; set SCL high, wait 1 cycle, loop back
```

Only 2 of the 32 available instructions. SM configuration (identical program, per-SM pin config):

- **Autopull enabled, 8-bit threshold** — the SM always pulls/shifts 8 bits at a time, regardless
  of whether the CPU or DMA put those bits there.
- **Shift direction: left (MSB first)** — required by ST7789.
- **FIFO join: `PIO_FIFO_JOIN_TX`** — merges the unused RX FIFO into TX, giving each SM an
  8-level TX FIFO instead of 4, for more DMA burst headroom.
- **Clock divider** set so the loop (2 cycles/bit) yields a 25–30 MHz SCL.

Initialization pattern: `pio_add_program()` once for all of `PIO0`, then a loop over SM index
0–3 calling `pio_sm_config`/`sm_config_set_out_pins`/`sm_config_set_sideset_pins`/
`sm_config_set_out_shift(..., false, true, 8)`/`pio_sm_init`/`pio_sm_set_enabled` with each SM's
own pin pair.

## 4. Command vs. pixel data flow

Two distinct paths feed the same PIO FIFO / same program:

- **Command mode (CPU-driven):** CPU manually toggles CS and DC, then writes 8-bit command bytes
  directly to the PIO TX FIFO. The SM pulls 8 bits and clocks them out. Fine to keep on the CPU
  since commands are small and infrequent (address-set sequences).
- **Pixel mode (DMA-driven):** CPU sends the "write memory" command (8-bit, CPU path above), sets
  DC high, then triggers a DMA transfer:
  - `DMA_SIZE_16` (16-bit transfers, matching LVGL's RGB565 framebuffer).
  - **`BSWAP` (byte swap) enabled** — the DMA engine swaps the high/low byte of each 16-bit pixel
    in-flight, in its own pipeline, at full bus speed and zero CPU cost. This is what resolves the
    RGB565-endianness-vs-byte-oriented-SPI mismatch without a software swap loop or a second PIO
    program.
  - DREQ tied to that display's specific SM TX FIFO (`DREQ_PIO0_TX<n>`).
  - The PIO SM keeps pulling 8 bits at a time regardless — it just sees two 8-bit chunks per
    swapped 16-bit word and has no idea it's "pixel data."

## 5. Synchronization rules (correctness-critical)

- **Drain before mode switch:** before flipping DC to switch from command mode to pixel mode (or
  back), the FIFO must be fully empty. A command byte still sitting in the FIFO when DC flips
  gets sent as pixel data.
- **DMA-done ≠ transfer-done:** the DMA "transfer complete" signal only means the data left RAM
  and entered the PIO FIFO — not that it has been clocked out to the display yet. Before pulling
  CS high at the end of a pixel blast, the driver must confirm both the PIO **FIFO is empty**
  (`FSTAT`/`FEMPTY`) and the **SM is idle** (not mid-shift).
- **No manual OSR-guarding needed:** with autopull enabled, the hardware handles FIFO→OSR
  handoff atomically — if the OSR and FIFO are both empty, the SM simply stalls on the next `out`
  instruction until new data arrives; there is no half-written/corrupted shift-register state to
  guard against in software.
- **Completion is polled, never interrupt-driven.** `PioSpiBus::busy()` (§0) checks
  `dma_channel_is_busy()` OR'd with the PIO FIFO-empty/SM-idle state, and is called from the same
  `wait_idle()`/`Display::task()` polling loops `ST7789`/`Display<DriverT>` already use today —
  no `DMA_IRQ_0` or PIO IRQ handler anywhere. This was an explicit decision (§8.1): it matches
  `SpiBus::busy()`'s existing synchronous-poll contract exactly (no separate completion
  callback), and keeps `architecture/HAL.md`'s current no-IRQ Display design valid even at 4
  concurrent DMA channels instead of reopening the shared-IRQ-line problem it was written to
  avoid.

## 6. DMA channel budget

4 channels (one per display, pixel-mode blasts) + 1 for the external flash/storage SPI = 5 of the
RP2350's 12–16 available DMA channels. Leaves the remainder free for other background transfers
(UI animation buffers, sensor polling, etc.).

## 7. Optional / aspirational, not committed

- **Memory bank striping:** placing framebuffers in different RAM banks (e.g. one display's
  buffer in Bank 0, another's in Bank 1) so concurrent DMA reads don't contend for the same RAM
  bus cycles. Raised as a stretch optimization ("if you're feeling particularly adventurous"),
  not a firm design requirement.

## 8. Throughput sanity check (derived, not stated)

The source conversation asserted "25–30 MHz SCL" and "30–60 FPS" as goals without deriving them
from this panel's actual dimensions. Using the panel size already in code
(`SCREEN_WIDTH`/`SCREEN_HEIGHT` = 284×76, `src/shared/hal/rp2350/display.h`):

- A full-frame update is 284 × 76 × 16 bits ≈ 345,344 bits.
- At 25–30 MHz SCL, that's ≈ 11.5–13.8 ms per full-frame blast — a ceiling of roughly 72–87 FPS
  **per display**.
- Because all 4 SMs blast independently and concurrently, that ceiling holds simultaneously
  across all 4 displays, not just one at a time — well above the original 30–60 FPS goal.
- **Caveat:** this is a ceiling on full-frame blasts, not the FPS LVGL will actually achieve.
  `Display::init()` allocates `m_draw_buf` at only 1/10 of screen height, so LVGL flushes dirty
  partial areas, not full frames, on most ticks — real achieved FPS depends on redraw-area size
  and is expected to be well within this ceiling rather than equal to it.

## 9. Resolved decisions log

Every question the source conversation raised and left unanswered was put to the user directly
in a follow-up `/grilling` session (2026-07-25) and resolved:

1. **Poll vs. interrupt for completion → poll, no IRQ.** The source conversation twice asked
   about using a DMA IRQ for CS cleanup and got no answer, which would have conflicted with the
   currently-documented no-`DMA_IRQ_0` Display design (CLAUDE.md's Display section — deliberately
   avoided ahead of a multi-display future, i.e. now). Resolved: `PioSpiBus::busy()` stays a pure
   poll (§0, §5) — no IRQ anywhere, keeping the existing no-IRQ rationale intact at 4 displays.
2. **Relationship to `SpiBus` → `PioSpiBus` implements it, `ST7789` unchanged.** Resolved in §0.
   The CS half of this (originally "no new design needed") was later revised after hardware
   debugging found a real software/SM race — see §0's CS callout and §2's CS row.
3. **Final GPIO pin assignments → left as an explicit TBD**, not a design gap (§2): a board-
   wiring decision, not something that changes the PIO/DMA architecture.
4. **Backlight PWM sharing → confirmed shared**, one PWM line tied like RESET (§2).
5. **Target clock/frame rate → derived from the actual panel size** rather than left as a stated
   goal (§8).
