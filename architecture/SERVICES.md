# Services Architecture

A **Service** wraps a hardware-independent unit of logic (e.g. `Button`) in its own execution
context — usually a dedicated RTOS task that polls or reacts to it on a schedule. This document
defines how Services stay decoupled from the specific RTOS/scheduler underneath, mirroring how
`architecture/HAL.md` decouples application logic from specific peripherals.

## 1. Core Principle

A Service depends on scheduling and synchronization primitives only through C++20 concepts —
never directly on `FreeRTOS.h`/`task.h` or pico-sdk sync primitives. The same Dependency Inversion
approach HAL.md applies to `GpioPin`/`LogSink` applies here: static dispatch via concepts and
templates, zero dynamic allocation, RTOS identity (stack size, priority) resolved at the glue
layer (`main.cpp`), not hardcoded into the Service itself.

This buys the same thing DIP buys on the HAL side, plus one more: a Service written against these
concepts can be instantiated against a **host-side fake** instead of the real RTOS implementation,
which is what makes it possible to unit-test and lint an otherwise RTOS-coupled service at all
(see next section).

## 2. Why header-only Services need a test wrapper

clang-tidy only meaningfully analyzes a file through an actual translation unit that compiles it —
a bare header sitting in `src/shared` with no `.cpp` anywhere that includes it effectively gets no
reliable coverage. `button.h` avoids this because `tests/shared/button_test.cpp` compiles it on the
host as a real doctest binary; that TU is what both `tools/clang-tidy.py` and `ctest` exercise.

Before this document, `button_service.h` had no such TU: it depended directly on
`xTaskCreateStatic`/`vTaskDelay`/`UBaseType_t`/`StackType_t`/`StaticTask_t`, none of which exist in
a host build. **The rule going forward: every header-only file under `src/shared` needs a
translation unit that compiles it.** For hardware/RTOS-independent logic, that TU is a host test
wrapper under `tests/`, mirroring the `src/` path (`src/shared/services/button_service.h` →
`tests/shared/services/button_service_test.cpp`). For a Service that touches FreeRTOS, getting that
TU to exist on the host at all requires pulling the RTOS dependency out from behind a concept first
— that's the point of the vocabulary below.

Pre-existing hardware-coupled headers that predate this rule (`display.h`, `st7789.h` — see
HAL.md's "target design vs. current state" section) are not retroactively required to gain
wrappers; this is a rule for new/refactored Service code, same incremental philosophy as the rest
of the linting setup (see CLAUDE.md's Linting section).

## 3. Concept Vocabulary

| Concept | Status | Concept file | rp2350 implementation | Host test double |
|---|---|---|---|---|
| `TaskRunner` | **Implemented** | `src/shared/hal/task_runner_concept.h` | `src/shared/hal/rp2350/freertos_task_runner.h` | `tests/mocks/fake_task_runner.h` |
| `EventQueue<T, N>` | **Implemented** — see `architecture/EVENT_SYSTEM.md` | `src/shared/event/event_queue_concept.h` | `src/shared/hal/rp2350/freertos_event_queue.h` | `tests/mocks/fake_event_queue.h` |
| `SpinLock` / `SpinLockGuard` | Target design, not implemented | — | — | — |
| `Mutex` | Target design, not implemented | — | — | — |

`TaskRunner` and `EventQueue` both have real consumers today (`ButtonService`, and
`ButtonService`/`SystemService` respectively); `SpinLock`/`Mutex` are documented so the next Service
that needs them follows the same shape, not a one-off. This mirrors how HAL.md documents an
`spi_concept` that isn't implemented yet — write down the target, build it when something needs it.

### `TaskRunner` (implemented)

The minimal primitive pair a periodically-polled Service needs: create a task, and delay inside its
own loop. Deliberately *not* a "run every N ms" method — a Service composes that itself out of
`delay_ms` in its own loop (see `ButtonService::run()`), which keeps the concept usable by services
with non-uniform timing too (e.g. one that occasionally delays longer after an idle period), not
just fixed-interval pollers.

```cpp
template<typename T>
concept TaskRunner = requires(T& runner, void (*entry)(void*), void* context, uint32_t ms) {
    { runner.start(entry, context) } -> std::same_as<void>;
    { runner.delay_ms(ms) } -> std::same_as<void>;
};
```

`start()` creates and begins running a task that calls `entry(context)`. The concrete rp2350
implementation, `FreeRTOSTaskRunner<StackWords>`, owns the static stack array and `StaticTask_t`
itself (templated on stack depth, since that has to be known at compile time — no dynamic
allocation anywhere in this firmware); task name and priority are runtime constructor parameters,
consistent with HAL.md's "peripheral identity via constructor params, not template args" rule.

A Service takes its `RunnerT` by reference in its constructor (same pattern `Button` uses for
`Logger<SinkT>&`), so the glue layer (`main.cpp`) owns and injects the concrete runner:

```cpp
static FreeRTOSTaskRunner<512> button_runner("ButtonService", 1);
static ButtonService<Pin, RttSink, FreeRTOSTaskRunner<512>> button_service(button_runner);
```

The host test double, `FakeTaskRunner`, records what `start()` was called with instead of spawning
anything — it must never actually invoke the entry point, since that's the Service's real `for(;;)`
loop and would hang a test. This is enough to unit-test a Service's `update()`/`add_button()`-style
logic directly, and to assert `start()` wires the right entry point/context through, entirely on
the host, with real doctest coverage and real clang-tidy coverage via the wrapper TU.

### `EventQueue<T, N>` (implemented)

A fixed-capacity, statically-allocated typed queue, exposed as native modern C++ (no `QueueHandle_t`
or `xQueueReceive` in the calling code) rather than a thin FreeRTOS wrapper — so, like `TaskRunner`,
it can be backed by something other than FreeRTOS on the host. Named `EventQueue` rather than the
generic `Queue` to leave room for other queue shapes the system may need later that aren't part of
the event system. First consumers: `ButtonService` (producer, publishes into it) and
`SystemService` (consumer, `receive()`s from it) — see `architecture/EVENT_SYSTEM.md` for the full
design, including the still-not-implemented `EventRouter` that will eventually sit between
multiple producers and consumers instead of the direct queue-sharing `main.cpp` does today.

### `SpinLock` / `SpinLockGuard` and `Mutex` (target design, not implemented)

RAII wrappers — `SpinLockGuard` acquiring in its constructor and releasing in its destructor,
`std::lock_guard`-compatible — around the rp2350 spinlock (`hardware_sync`) and FreeRTOS mutex
primitives respectively. `src/shared/hal/rp2350/rtt_sink.cpp` is the concrete motivating example:
it currently claims and locks a raw `spin_lock_t*` by hand (`spin_lock_claim_unused` /
`spin_lock_blocking` / `spin_unlock`) with no RAII, which is exactly the kind of pre-refactor HAL
code these wrappers are meant to replace once built — out of scope for this pass.

## 4. Worked Example

`ButtonService` is the reference implementation of this pattern, the way `pin_concept.h`/`Pin`/
`Button` is HAL.md's:

- Concept: `src/shared/hal/task_runner_concept.h`
- rp2350 implementation: `src/shared/hal/rp2350/freertos_task_runner.h`
- Host double: `tests/mocks/fake_task_runner.h`
- Service using it: `src/shared/services/button_service.h`
- Host test wrapper: `tests/shared/services/button_service_test.cpp`
- Glue/injection: `src/projects/hub-master/main.cpp`
