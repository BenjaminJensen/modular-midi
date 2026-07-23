# modular-midi

Firmware for `hub-master`, an RP2350-based MIDI controller with 4 buttons and 4 displays.

## Language

**Render Engine**:
The bare-metal subsystem running on core 1 (the `render_task` loop) that owns the LVGL widget tree for every physical display and drives the actual pixel output. Has no FreeRTOS dependency — it is the "GPU" side of the system referenced in `architecture/EVENT_SYSTEM.md`.
_Avoid_: GPU (used informally in prose, but the formal subsystem name is Render Engine), UI Engine.

**RenderService**:
The core-0 FreeRTOS Service (in the SERVICES.md sense) that is the sole producer writing into the Mailbox. Consumes State events via the existing EventQueue/EventRouter and translates each into a Slot update — never touches raw input events (e.g. Button) directly, and never called into synchronously by other core-0 code. Pure presentation logic: it knows how a state change should look, not what caused it.
_Avoid_: UiService, DisplayService.

**Slot**:
A named, fixed UI element within a display's pre-built layout. v1 has exactly one Slot per display — the Label — so a Slot is addressed by `display_id` alone (no separate `slot_id`; add one only if/when a second Slot kind is actually needed). Slot content:
- `text`: up to 24 characters, silently truncated by RenderService if longer.
- `color`: a `ColorPalette` value — `Default` (white) or `Active` (green). Not raw RGB; meaning is centralized in the Render Engine, not chosen per-call.
- `font`: a `FontSize` value — `Small` (Montserrat 20px) or `Large` (Montserrat 42px, the current `LV_FONT_DEFAULT`). Not a raw pixel size — LVGL needs specific compiled-in font objects, and only these two are compiled in.

**Mailbox**:
The cross-core shared structure holding the latest pending Slot value per `display_id` (4 entries, one per display). Not a queue — writing a Slot overwrites any not-yet-consumed previous value (latest-value-wins), so it has no depth/overflow policy. Each entry is double-buffered with an atomically-swapped active-buffer index, so RenderService (core 0, writer) and Render Engine (core 1, reader) never tear a read. Each entry also carries an atomic dirty flag (set on write, cleared after Render Engine applies it) so `render_task`'s unblocked polling loop only touches LVGL when something actually changed. No SIO FIFO/doorbell involved — `render_task` already spins continuously, so it polls dirty flags directly rather than being woken.

**State event** (`EventType::State`):
An `Event` representing an application/system-level state transition (program loaded, mode changed, looper started/stopped, ...) — distinct from raw input events like `EventType::Button`. Named for what it represents, not for who consumes it: RenderService is *a* consumer of State events, not their reason for existing. Payload codec (how the 24-bit payload distinguishes which state changed) is not yet designed — deferred until the first state-producing service is built, matching this codebase's incremental per-type-codec approach (see `architecture/EVENT_SYSTEM.md`).
_Avoid_: EventType::UI (reserved in `event_common.h` but deliberately left unused/unassigned by this decision — see `docs/adr/` once written).
