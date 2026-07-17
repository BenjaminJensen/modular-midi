# Event System Architecture

The Event System is how independent parts of the firmware — a button press, a MIDI message
arriving, the active program changing — reach whoever needs to react, without producers knowing or
caring who their consumers are, and without a producer's task ever running consumer logic. This
document defines the event representation and the routing model. It follows the same
Dependency-Inversion approach as `architecture/HAL.md` (peripherals) and `architecture/SERVICES.md`
(execution contexts): concepts and templates for static dispatch, zero dynamic allocation,
resolved as much as possible at compile time.

## 1. Core Principles

- **Compile-time topology.** Which consumers receive which `EventType`s is wired explicitly in the
  glue layer (`main.cpp`), the same way `button_service.add_button(&button)` wires buttons to
  `ButtonService` today — not a runtime `subscribe()` registry. C++20 gives enough compile-time
  machinery (templates, fold expressions, `constexpr`) to resolve the entire producer→consumer
  graph at build time, so there's no dynamic subscriber list to size, overflow, or iterate at
  runtime.
- **No dedicated broker task.** "Central broker" describes where the routing table lives (one
  place, one source of truth), not a runtime execution context. Publishing an event is a direct,
  synchronous fan-out from the producer's own task into each subscribed consumer's queue — copying
  a 4-byte value into 0..N queues, not executing any consumer logic. This avoids paying for an
  extra task/context-switch hop on every event, which a "broker task that itself reads an inbound
  queue and re-publishes" design would need.
- **Every event is 4 bytes.** All queues in the system hold the same native, trivially-copyable
  type — `Event` (a `uint32_t`). No per-event-type queue element type, no `std::variant` (a
  variant's discriminant + alignment padding won't reliably stay at 4 bytes across event types of
  different shapes — ruled out for that reason). Type-specific structure is layered on *top* of
  that flat 32 bits via encode/decode helpers, never baked into the queue element itself.
- **Static allocation only**, consistent with the rest of the firmware (see CLAUDE.md's
  Concurrency model): every consumer's queue is a fixed-capacity, compile-time-sized
  `EventQueue<Event, N>` — no heap, no `xQueueCreate` (only the static variant).
- **Out of scope for now: cross-core delivery.** `main.cpp` runs FreeRTOS on core 0 and a bare
  polling loop (`display_task`) on core 1, deliberately — core 1 is being kept free to act as a
  bare-metal GPU for a graphics engine designed later. The event system as defined here only
  routes between FreeRTOS tasks on core 0. Getting data to core 1 is a separate, not-yet-designed
  problem for whenever the graphics engine work starts, not an extension of this routing table.
- **Out of scope for now: ISR producers.** All current and near-term producers (`ButtonService`,
  a future task-based MIDI receive) publish from ordinary task context, so the router only needs
  `xQueueSend`, not the `FromISR` variants. If a producer ever needs to publish from an interrupt
  handler, that's an additive change to the `EventQueue` concept (an `send_from_isr()`-shaped
  operation), not a redesign of the routing model — revisit when a real ISR producer exists, same
  incremental philosophy as the rest of this codebase's architecture docs.

## 2. Event Representation

`src/shared/event/event_common.h` already defines the wire format and stays as the single
queue-element type for the whole system:

```cpp
enum class EventType : uint8_t {
    Button = 0,
    Midi = 1,
    UI = 2,
    CustomStart = 100 // Allows new modules to claim IDs
};

struct Event {
    uint32_t raw;
    [[nodiscard]] constexpr EventType type() const { return static_cast<EventType>(raw >> 24); }
    [[nodiscard]] constexpr uint32_t payload() const { return raw & 0x00FFFFFF; }
};
```

Top byte is the type tag; the remaining 24 bits are opaque payload until interpreted by a
per-type codec.

### Per-type codecs

Each `EventType` gets a small, free-standing encode/decode pair — not a member of `Event` itself,
so `event_common.h` doesn't need to know about every event type that will ever exist. Two
mechanisms are both acceptable, chosen per field granularity:

- **Byte-aligned fields** (a button ID, a button state enum, a program-slot index): a small
  struct, `static_assert(sizeof(T) == 3)` (it has to fit the 24-bit payload, not the full 4 bytes
  — the top byte is already spoken for by `type()`), converted to/from the payload bits with
  `std::bit_cast`-style helpers. Compiler-checked size, no hand-computed shifts.
- **Sub-byte fields** (MIDI's 4-bit status nibble + 4-bit channel nibble sharing a byte): explicit
  mask/shift constants, the same style `Event::type()`/`payload()` already use. Bitfield structs
  are deliberately avoided here — cross-compiler bitfield layout is implementation-defined, and
  this project only needs to target GCC/Clang on ARM EABI in practice, but explicit masks remove
  the question entirely rather than relying on that happening to be consistent.

Example shapes (illustrative — finalize when each producer is actually built):

```cpp
// Button: id (8 bits) + state (8 bits), byte-aligned within the 24-bit payload
namespace button_event {
    enum class State : uint8_t { Pressed, Released, LongPressed, DoubleTapped };
    constexpr Event encode(uint8_t id, State state) {
        return Event{(static_cast<uint32_t>(EventType::Button) << 24)
                    | (static_cast<uint32_t>(id) << 8)
                    | static_cast<uint32_t>(state)};
    }
    constexpr uint8_t id(Event e)   { return static_cast<uint8_t>((e.payload() >> 8) & 0xFF); }
    constexpr State state(Event e)  { return static_cast<State>(e.payload() & 0xFF); }
}

// MIDI: status nibble + channel nibble + 2 data bytes — every MIDI channel message minus
// sysex fits this shape (Note On/Off, CC, Program Change, Pitch Bend, etc.)
namespace midi_event {
    constexpr Event encode(uint8_t status_nibble, uint8_t channel, uint8_t data1, uint8_t data2) {
        return Event{(static_cast<uint32_t>(EventType::Midi) << 24)
                    | (static_cast<uint32_t>(status_nibble & 0x0F) << 20)
                    | (static_cast<uint32_t>(channel & 0x0F) << 16)
                    | (static_cast<uint32_t>(data1) << 8)
                    | static_cast<uint32_t>(data2)};
    }
}
```

`event_engine.h` and every consumer only ever see `Event` (a `uint32_t`); the per-type namespaces
above are the only place that knows how to pack or unpack one.

## 3. Concept Vocabulary

| Concept | Status | Concept file | rp2350 implementation | Host test double |
|---|---|---|---|---|
| `EventQueue<T, N>` | Target design, not implemented | `src/shared/event/event_queue_concept.h` (proposed) | FreeRTOS static queue (`xQueueCreateStatic`) | fake ring buffer, mirrors `FakeTaskRunner` |
| `EventRouter<Bindings...>` | Target design, not implemented | `src/shared/event/event_router.h` (proposed) | header-only, no concrete/host split needed — it's pure compile-time fan-out logic | tested directly on host (no RTOS dependency) |

`EventQueue<T, N>` was already flagged in `architecture/SERVICES.md` (there as `Queue<T, N>`,
renamed here to leave room for other queue shapes elsewhere in the system) as the target-design
primitive `event_engine.h` would need; this document is that "once that lands" moment. Its shape
mirrors `TaskRunner`: minimal, blocking-with-timeout, no dynamic allocation.

```cpp
template<typename Q, typename T>
concept EventQueue = requires(Q& q, const T& item, T& out, uint32_t timeout_ms) {
    { q.send(item) } -> std::same_as<bool>;              // false if full (see overflow policy)
    { q.receive(out, timeout_ms) } -> std::same_as<bool>; // false on timeout, true if out was filled
};
```

`receive()`'s block-with-timeout *is* the notify/wake mechanism — no separate semaphore or task
notification is layered on top. A consumer's loop looks like:

```cpp
for (;;) {
    Event e;
    if (queue.receive(e, poll_timeout_ms)) {
        handle(e);
    }
    // any other periodic work the service needs still runs here on timeout,
    // same shape ButtonService's update-loop already has today
}
```

This gives immediate wake-up when an event arrives, and still lets a service that also needs
periodic polling (like `ButtonService` debouncing pins) use the same loop shape it already has —
just replacing a plain `delay_ms` with `queue.receive(..., timeout)`.

### `EventRouter<Bindings...>`

The "central broker, resolved at compile time" piece. A `Binding` pairs an `EventType` with a
reference to a subscriber's queue; `EventRouter` is templated on a pack of these and fans out via
a fold expression — the set of possible destinations for a `publish()` call is fixed at compile
time and inlined, even though *which* of them actually matches a given event is necessarily a
runtime check against `event.type()`.

```cpp
template<EventType Type, EventQueue<Event> Q>
struct Binding {
    static constexpr EventType type = Type;
    Q& queue;
};

template<typename... Bindings>
class EventRouter {
public:
    explicit constexpr EventRouter(Bindings&... bindings) : m_bindings(bindings...) {}

    void publish(Event e) const {
        std::apply([&](auto&... b) { (dispatch(b, e), ...); }, m_bindings);
    }

private:
    template<typename B>
    static void dispatch(B& binding, Event e) {
        if (e.type() == B::type) {
            if (!binding.queue.send(e)) {
                g_log.warn() << "Event dropped, queue full for type "
                              << static_cast<uint32_t>(B::type);
            }
        }
    }
    std::tuple<Bindings&...> m_bindings;
};
```

A producer holds a `const EventRouter<...>&` and calls `publish()` — it never touches a
consumer's queue directly or knows how many subscribers exist, but no runtime registry or
`std::function` indirection is involved either.

## 4. Overflow Policy

**Drop the newest event and log it** when a consumer's queue is full (`send()` returns `false`,
`EventRouter::dispatch` logs via `g_log.warn()`). Rationale: the producer must never block on a
slow consumer — that would reintroduce exactly the coupling this whole design exists to avoid —
and for the event types currently in scope (discrete button transitions, MIDI messages, state
changes), losing the newest of a burst is preferable to silently reordering by evicting an older,
possibly already-relevant event. Revisit per-type if a future event type turns out to only care
about "latest value wins" (e.g. a rapidly-updating continuous value) — nothing in the design above
prevents a specific consumer from choosing a different local policy, since `send()`'s
full/not-full outcome is visible at the call site.

## 5. Worked Example (glue layer, illustrative)

```cpp
// main.cpp — compile-time wiring, no runtime subscribe() calls
static FreeRTOSEventQueue<Event, 8>  display_inbox;
static FreeRTOSEventQueue<Event, 8>  midi_inbox;

static Binding<EventType::Button, FreeRTOSEventQueue<Event, 8>> button_to_display{display_inbox};
static Binding<EventType::Midi,   FreeRTOSEventQueue<Event, 8>> midi_to_display{display_inbox};
static Binding<EventType::UI,     FreeRTOSEventQueue<Event, 8>> ui_to_midi{midi_inbox};

static EventRouter router(button_to_display, midi_to_display, ui_to_midi);

// ButtonService is constructed with a reference to `router` and calls
// router.publish(button_event::encode(id, state)) instead of logging directly.
```

## 6. Open Items

- `EventQueue<T, N>` concept + rp2350/host implementations: not yet written.
- `EventRouter`: not yet written; the shape above is a starting point, not a locked API.
- Per-type codec namespaces (`button_event`, `midi_event`, a `program_loaded_event` for system
  state changes) exist today only as the illustrative sketches in §2 — write them alongside the
  Service that first needs them, same incremental approach as the rest of this codebase.
- Following CLAUDE.md's TDD rule: `EventRouter` and the codec namespaces have no pico-sdk/FreeRTOS
  dependency (`EventQueue` is a concept, satisfiable by a host fake same as `TaskRunner`), so they
  belong in `tests/` with real doctest coverage from the start — don't let this become a
  header-only file with no compiling TU, per `architecture/SERVICES.md` §2's rule.
- Queue depth (`N`) is a per-consumer tuning parameter, not fixed here — size it to the burst
  behavior of whatever's producing into it once real producers exist.
