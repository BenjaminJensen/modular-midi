# New EventType::State instead of reusing the reserved EventType::UI

RenderService needs to know when to redraw a display, but should stay pure presentation
logic — it must not know or care what caused a change (a button press, a MIDI program
change, anything else). `event_common.h` already reserves `EventType::UI = 2`, unused
until now, which was the obvious candidate to route these updates through.

We decided against reusing it. "UI" names events by who happens to consume them today,
not by what they represent, and the events in question (program loaded, mode changed,
looper started/stopped) are application/system-level state transitions that could
plausibly have other consumers later (e.g. MIDI program-change output reacting to the
same "program loaded" transition). We introduced `EventType::State` instead, and
RenderService subscribes only to it — never to raw `EventType::Button`. A separate,
not-yet-built service owns translating raw button presses into State events; that
translation logic is out of scope of this decision.

`EventType::UI` is left reserved but unassigned. Its payload codec (how sub-kinds like
"program loaded" vs "looper started" pack into the 24-bit payload) is also not yet
designed — deferred until the first State-producing service is built, per this
codebase's incremental per-type-codec approach.
