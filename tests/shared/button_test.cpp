#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "button.h"
#include "logger.h"
#include "mocks/fake_pin.h"
#include "mocks/fake_sink.h"

namespace {
    // LONG_PRESS_MS is kept well above 4*TICK_MS so the debounce delay itself
    // (up to ~3 ticks of "still counted as pressed" while release settles)
    // can't accidentally cross the long-press threshold on its own.
    constexpr uint16_t LONG_PRESS_MS = 200;
    constexpr uint8_t TICK_MS = 10;

    ButtonTransition press_debounced(Button<FakePin, FakeSink>& button, FakePin& pin) {
        pin.reading = false; // active-low: pressed
        ButtonTransition transitions = ButtonTransition::None;
        for (int i = 0; i < 4; ++i) {
            transitions |= button.update(TICK_MS);
        }
        return transitions;
    }

    ButtonTransition release_debounced(Button<FakePin, FakeSink>& button, FakePin& pin) {
        pin.reading = true; // active-low: released
        ButtonTransition transitions = ButtonTransition::None;
        for (int i = 0; i < 4; ++i) {
            transitions |= button.update(TICK_MS);
        }
        return transitions;
    }
}

TEST_CASE("button reports not pressed before debounce completes") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    pin.reading = false;
    (void)button.update(TICK_MS);

    CHECK_FALSE(button.is_pressed());
}

TEST_CASE("button debounces into pressed state after 4 consecutive low readings") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    ButtonTransition transitions = press_debounced(button, pin);

    CHECK(button.is_pressed());
    CHECK(has(transitions, ButtonTransition::Pressed));
}

TEST_CASE("button debounces back to released after 4 consecutive high readings") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    REQUIRE(button.is_pressed());

    ButtonTransition transitions = release_debounced(button, pin);

    CHECK_FALSE(button.is_pressed());
    CHECK(has(transitions, ButtonTransition::Released));
}

TEST_CASE("holding past the long-press threshold returns a LongPressed transition") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);

    ButtonTransition transitions = ButtonTransition::None;
    // Keep holding until hold_timer passes LONG_PRESS_MS
    for (uint16_t held = 0; held < LONG_PRESS_MS; held += TICK_MS) {
        transitions |= button.update(TICK_MS);
    }

    CHECK(has(transitions, ButtonTransition::LongPressed));
}

TEST_CASE("releasing before the long-press threshold does not trigger a long press") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    (void)button.update(TICK_MS); // hold briefly, well under the threshold

    ButtonTransition transitions = release_debounced(button, pin);

    CHECK_FALSE(has(transitions, ButtonTransition::LongPressed));
}

TEST_CASE("two quick taps within the gap window trigger a double tap on the second release") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    ButtonTransition first_release = release_debounced(button, pin);
    CHECK_FALSE(has(first_release, ButtonTransition::DoubleTapped));

    press_debounced(button, pin);
    ButtonTransition second_release = release_debounced(button, pin);

    // The tick that resolves the double tap is the same tick as the second
    // release edge itself - both transitions fire from the same update() call.
    CHECK(has(second_release, ButtonTransition::Released));
    CHECK(has(second_release, ButtonTransition::DoubleTapped));
}

TEST_CASE("a single tap followed by the gap window expiring does not trigger a double tap") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    release_debounced(button, pin);

    ButtonTransition transitions = ButtonTransition::None;
    // Let the gap window expire without a second tap
    for (uint16_t waited = 0; waited < LONG_PRESS_MS; waited += TICK_MS) {
        transitions |= button.update(TICK_MS);
    }

    CHECK_FALSE(has(transitions, ButtonTransition::DoubleTapped));
}

TEST_CASE("pressing the button logs a line identifying the button by id") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(3, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "Button 3 pressed");
}

TEST_CASE("releasing the button logs a released line") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(1, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    release_debounced(button, pin);

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "Button 1 released");
}

TEST_CASE("holding past the long-press threshold logs a long press line") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(2, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    for (uint16_t held = 0; held < LONG_PRESS_MS; held += TICK_MS) {
        (void)button.update(TICK_MS);
    }

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "Button 2 long press");
}

TEST_CASE("a double tap logs a double tap line") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    release_debounced(button, pin);
    press_debounced(button, pin);
    release_debounced(button, pin);

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "Button 0 double tap");
}

TEST_CASE("id() returns the id the button was constructed with") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(7, &pin, logger, LONG_PRESS_MS);

    CHECK(button.id() == 7);
}
