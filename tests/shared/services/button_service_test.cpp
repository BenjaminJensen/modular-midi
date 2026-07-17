#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include "services/button_service.h"
#include "button.h"
#include "logger.h"
#include "mocks/fake_pin.h"
#include "mocks/fake_sink.h"
#include "mocks/fake_task_runner.h"

namespace {
    using Service = ButtonService<FakePin, FakeSink, FakeTaskRunner>;

    void press_debounced(Button<FakePin, FakeSink>& button, FakePin& pin, Service& service) {
        pin.reading = false; // active-low: pressed
        for (int i = 0; i < 4; ++i) {
            service.update(10);
        }
    }
}

TEST_CASE("a newly constructed service updates zero buttons without crashing") {
    FakeTaskRunner runner;
    Service service(runner);

    service.update(10);
}

TEST_CASE("update() forwards delta_time to every added button") {
    FakeTaskRunner runner;
    Service service(runner);

    FakePin pin1, pin2;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button1(0, &pin1, logger);
    Button<FakePin, FakeSink> button2(1, &pin2, logger);

    service.add_button(&button1);
    service.add_button(&button2);

    press_debounced(button1, pin1, service);

    CHECK(button1.is_pressed());
    CHECK(button2.is_pressed());
}

TEST_CASE("adding more than MAX_BUTTONS buttons is safely ignored") {
    FakeTaskRunner runner;
    Service service(runner);

    std::array<FakePin, 5> pins;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    std::array<Button<FakePin, FakeSink>, 5> buttons = {
        Button<FakePin, FakeSink>(0, &pins[0], logger),
        Button<FakePin, FakeSink>(1, &pins[1], logger),
        Button<FakePin, FakeSink>(2, &pins[2], logger),
        Button<FakePin, FakeSink>(3, &pins[3], logger),
        Button<FakePin, FakeSink>(4, &pins[4], logger),
    };

    for (auto& button : buttons) {
        service.add_button(&button);
    }

    for (auto& pin : pins) {
        pin.reading = false; // active-low: pressed
    }
    for (int i = 0; i < 4; ++i) {
        service.update(10);
    }

    CHECK(buttons[0].is_pressed());
    CHECK(buttons[3].is_pressed());
    CHECK_FALSE(buttons[4].is_pressed()); // dropped: 5th add_button was ignored
}

TEST_CASE("start() hands the task entry point and this-context to the runner") {
    FakeTaskRunner runner;
    Service service(runner);

    service.start();

    CHECK(runner.started_entry != nullptr);
    CHECK(runner.started_context == &service);
}
