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
so `event_common.h` doesn't need to know about every event type that will ever exist. In practice
both fields implemented so far are byte-aligned, and `ButtonPayload` below settled on explicit
shift/mask constants (the same style `Event::type()`/`payload()` already use) rather than a
`std::bit_cast`-based struct — simpler once every field just happens to be a whole byte, and it
sidesteps `std::bit_cast`'s requirement that the source and destination be the same size (the
payload being cast from is a 24-bit-meaningful `uint32_t`, not a byte-for-byte match for a 3-byte
struct). A `bit_cast`-based struct remains an option for a future event type if its fields turn out
not to need shift/mask logic anyway; bitfield structs are deliberately avoided regardless of
mechanism — cross-compiler bitfield layout is implementation-defined, and this project only needs
to target GCC/Clang on ARM EABI in practice, but explicit masks remove the question entirely rather
than relying on that happening to be consistent.

**Button (implemented)** — `src/shared/services/button_payload.h`. Three byte-aligned fields
within the 24-bit payload (`id`, `state`, a currently-unused `reserved` byte), as a small struct
rather than a namespace of free functions — `pack()`/`unpack()` are static members instead of the
`encode()`/`id()`/`state()` free-function sketch this section originally had:

```cpp
enum class ButtonEventState : uint8_t { Pressed, Released, LongPressed, DoubleTapped };

struct ButtonPayload {
    uint8_t id;
    ButtonEventState state;
    uint8_t reserved;

    static constexpr ButtonPayload unpack(uint32_t payload) {
        return {
            static_cast<uint8_t>(payload >> 16),
            static_cast<ButtonEventState>((payload >> 8) & 0xFF),
            static_cast<uint8_t>(payload & 0xFF)
        };
    }
    static constexpr Event pack(uint8_t id, ButtonEventState state) {
        uint32_t data = (static_cast<uint32_t>(id) << 16) | (static_cast<uint32_t>(state) << 8);
        return { (static_cast<uint32_t>(EventType::Button) << 24) | data };
    }
};
```

Also has `to_string(ButtonEventState)`, matching the wording `Button` itself already logs (e.g.
"pressed", "long press"), so a button's own debug lines and any consumer's event-driven log lines
read consistently.

**MIDI (illustrative — not yet built)**: status nibble + channel nibble + 2 data bytes — every
MIDI channel message minus sysex fits this shape (Note On/Off, CC, Program Change, Pitch Bend,
etc.). Not implemented until a MIDI-receiving producer actually exists:

```cpp
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

Every producer/consumer only ever sees `Event` (a `uint32_t`) on the wire — `ButtonPayload` (and
whatever MIDI's eventual equivalent is) are the only places that know how to pack or unpack one.

## 3. Concept Vocabulary

| Concept | Status | Concept file | rp2350 implementation | Host test double |
|---|---|---|---|---|
| `EventQueue<T, N>` | **Implemented** | `src/shared/event/event_queue_concept.h` | `src/shared/hal/rp2350/freertos_event_queue.h` (`FreeRTOSEventQueue<Capacity>`, `Event` fixed as the element type rather than a second template param) | `tests/mocks/fake_event_queue.h` (`FakeEventQueue`, an unbounded `std::vector`-backed FIFO rather than literally a ring buffer, but fulfills the same concept) |
| `EventRouter<Bindings...>` | Target design, not implemented | `src/shared/event/event_router.h` (proposed) | header-only, no concrete/host split needed — it's pure compile-time fan-out logic | tested directly on host (no RTOS dependency) |

`EventQueue<T, N>` was already flagged in `architecture/SERVICES.md` (there as `Queue<T, N>`,
renamed here to leave room for other queue shapes elsewhere in the system) as the target-design
primitive this document's routing would need. Its shape mirrors `TaskRunner`: minimal,
blocking-with-timeout, no dynamic allocation.

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

## 5. Worked Example

`ButtonService`/`SystemService` are the reference producer/consumer pair, the way `pin_concept.h`/
`Pin`/`Button` is HAL.md's and `TaskRunner`/`ButtonService` is SERVICES.md's. There's no
`EventRouter` yet (only one consumer exists so far), so `main.cpp` wires both services directly to
the same queue instance — this direct-sharing is exactly the seam `EventRouter` will slot into once
a second consumer needs the same event stream:

```cpp
// main.cpp — compile-time wiring, no runtime subscribe() calls
static FreeRTOSEventQueue<8> button_events;

static FreeRTOSTaskRunner<512> button_runner("ButtonService", 1);
static ButtonService<Pin, RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>>
    button_service(button_runner, button_events);

static FreeRTOSTaskRunner<512> system_runner("SystemService", 1);
static SystemService<RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>>
    system_service(g_log, system_runner, button_events);
```

- Producer: `src/shared/services/button_service.h` — `ButtonService::update()` calls each
  `Button`'s `update()`, which returns a `ButtonTransition` bitmask (see `architecture/SERVICES.md`
  for why `Button`'s API is edge-triggered rather than the level/latched state it started as), and
  publishes one `ButtonPayload::pack(id, state)` `Event` per set bit via `m_queue.send(...)`.
- Consumer: `src/shared/services/system_service.h` — `SystemService::run()` blocks on
  `queue.receive(event, timeout)` (the notify mechanism described in §3) and currently just logs
  `EventType::Button` events (`"event: button <id> <state>"`); other `EventType`s fall through a
  no-op `default:` case, the natural place to grow as more producers exist.
- Host test wrappers: `tests/shared/services/button_service_test.cpp`,
  `tests/shared/services/system_service_test.cpp`, `tests/shared/services/button_payload_test.cpp`
  — all real doctest coverage via `FakeEventQueue`, no RTOS involved.
- Glue/injection: `src/projects/hub-master/main.cpp` (shown above).

`ButtonService`'s `send()` calls don't check the returned `bool` yet (see §4's overflow policy) —
there's no `Logger` injected into `ButtonService` to log a drop through, and with only one consumer
today there's limited value in it. Revisit once `EventRouter` centralizes this.

## 6. Open Items

- `EventRouter`: not yet written; the shape in §3 is a starting point, not a locked API. Still
  only one consumer (`SystemService`) exists, so there's nothing to fan out to yet — build this
  when a second consumer needs the same event stream.
- Per-type codecs: `ButtonPayload` (§2) is real and in use. `midi_event` and a
  `program_loaded_event` for system state changes are still illustrative-only sketches — write them
  alongside the Service that first needs them, same incremental approach as the rest of this
  codebase.
- Drop-and-log overflow handling (§4) isn't wired up anywhere yet — `FreeRTOSEventQueue::send()`
  already implements the *drop* half (non-blocking `xQueueSend`), but nothing logs it. Natural to
  land together with `EventRouter`, whose `dispatch()` is where §4 always intended that log line to
  live.
- Queue depth (`N`) is a per-consumer tuning parameter, not fixed here — `main.cpp` currently uses
  8 for `button_events`; revisit once real button-press burst behavior is observed on target.
