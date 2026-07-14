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

    void press_debounced(Button<FakePin, FakeSink>& button, FakePin& pin) {
        pin.reading = false; // active-low: pressed
        for (int i = 0; i < 4; ++i) {
            button.update(TICK_MS);
        }
    }

    void release_debounced(Button<FakePin, FakeSink>& button, FakePin& pin) {
        pin.reading = true; // active-low: released
        for (int i = 0; i < 4; ++i) {
            button.update(TICK_MS);
        }
    }
}

TEST_CASE("button reports not pressed before debounce completes") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    pin.reading = false;
    button.update(TICK_MS);

    CHECK_FALSE(button.is_pressed());
}

TEST_CASE("button debounces into pressed state after 4 consecutive low readings") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);

    CHECK(button.is_pressed());
}

TEST_CASE("button debounces back to released after 4 consecutive high readings") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    REQUIRE(button.is_pressed());

    release_debounced(button, pin);

    CHECK_FALSE(button.is_pressed());
}

TEST_CASE("holding past the long-press threshold sets was_long_pressed") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    REQUIRE_FALSE(button.was_long_pressed());

    // Keep holding until hold_timer passes LONG_PRESS_MS
    for (uint16_t held = 0; held < LONG_PRESS_MS; held += TICK_MS) {
        button.update(TICK_MS);
    }

    CHECK(button.was_long_pressed());
}

TEST_CASE("releasing before the long-press threshold does not trigger a long press") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    button.update(TICK_MS); // hold briefly, well under the threshold

    release_debounced(button, pin);

    CHECK_FALSE(button.was_long_pressed());
}

TEST_CASE("two quick taps within the gap window trigger a double tap") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    release_debounced(button, pin);
    CHECK_FALSE(button.was_double_tapped());

    press_debounced(button, pin);
    release_debounced(button, pin);

    CHECK(button.was_double_tapped());
}

TEST_CASE("a single tap followed by the gap window expiring does not trigger a double tap") {
    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button button(0, &pin, logger, LONG_PRESS_MS);

    press_debounced(button, pin);
    release_debounced(button, pin);

    // Let the gap window expire without a second tap
    for (uint16_t waited = 0; waited < LONG_PRESS_MS; waited += TICK_MS) {
        button.update(TICK_MS);
    }

    CHECK_FALSE(button.was_double_tapped());
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
        button.update(TICK_MS);
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
