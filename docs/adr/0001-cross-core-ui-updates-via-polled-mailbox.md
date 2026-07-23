# Cross-core UI updates via a polled Mailbox, not a command queue

Core 1 (the Render Engine) already runs a bare, continuously-spinning loop with no
blocking/sleep between iterations, driving LVGL displays. Core 0 needs to push label
updates to it without dynamic allocation, without either core blocking on the other, and
without torn reads. We considered the RP2350's hardware SIO FIFO (32-bit words, 8 deep —
too small to carry a text payload), a shared-memory ring buffer of commands with the FIFO
as a wake-up doorbell, and a hardware spinlock around shared state.

We decided on a **Mailbox**: a fixed-size struct with one entry per display, each
double-buffered with an atomically-swapped active-buffer index and a dirty flag. A single
dedicated RenderService (enforcing single-producer) writes the inactive copy, then flips
the index, then sets dirty; the Render Engine polls dirty flags every loop iteration and
applies changes to the corresponding LVGL widget, no signaling mechanism involved. This
is a mailbox (latest-value-wins), not a queue — it has no depth and needs no overflow
policy, because UI label state is inherently "only the latest value matters," and the
Render Engine never needs waking since it already spins continuously. The double-buffer +
atomic-index pattern avoids torn reads without either core ever blocking.

## Consequences

- No SIO FIFO usage anywhere in this design; it remains free for a future genuinely
  blocking/interrupt-driven use.
- This only works because there is exactly one producer (RenderService) per Mailbox
  entry — if a second core-0 writer to the same display's Slot is ever needed, this
  design needs revisiting (e.g. a lock on the write side).
- Adding a second Slot per display (e.g. an icon) means growing the Mailbox's addressing
  from `display_id` alone to `(display_id, slot_id)` — deliberately not built until a
  real second Slot type exists.
