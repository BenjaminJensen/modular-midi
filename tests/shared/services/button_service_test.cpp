#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include "services/button_service.h"
#include "services/button_payload.h"
#include "button.h"
#include "logger.h"
#include "mocks/fake_pin.h"
#include "mocks/fake_sink.h"
#include "mocks/fake_task_runner.h"
#include "mocks/fake_event_queue.h"

namespace {
    using Service = ButtonService<FakePin, FakeSink, FakeTaskRunner, FakeEventQueue>;

    void press_debounced(Button<FakePin, FakeSink>& button, FakePin& pin, Service& service) {
        pin.reading = false; // active-low: pressed
        for (int i = 0; i < 4; ++i) {
            service.update(10);
        }
    }

    void release_debounced(Button<FakePin, FakeSink>& button, FakePin& pin, Service& service) {
        pin.reading = true; // active-low: released
        for (int i = 0; i < 4; ++i) {
            service.update(10);
        }
    }
}

TEST_CASE("a newly constructed service updates zero buttons without crashing") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    service.update(10);
}

TEST_CASE("update() forwards delta_time to every added button") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

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
    FakeEventQueue queue;
    Service service(runner, queue);

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
    FakeEventQueue queue;
    Service service(runner, queue);

    service.start();

    CHECK(runner.started_entry != nullptr);
    CHECK(runner.started_context == &service);
}

TEST_CASE("pressing a button publishes a Pressed event tagged with its id") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button(5, &pin, logger);
    service.add_button(&button);

    press_debounced(button, pin, service);

    REQUIRE_FALSE(queue.sent.empty());
    ButtonPayload payload = ButtonPayload::unpack(queue.sent.back().payload());
    CHECK(queue.sent.back().type() == EventType::Button);
    CHECK(payload.id == 5);
    CHECK(payload.state == ButtonEventState::Pressed);
}

TEST_CASE("releasing a button publishes a Released event") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button(0, &pin, logger);
    service.add_button(&button);

    press_debounced(button, pin, service);
    release_debounced(button, pin, service);

    REQUIRE_FALSE(queue.sent.empty());
    ButtonPayload payload = ButtonPayload::unpack(queue.sent.back().payload());
    CHECK(payload.state == ButtonEventState::Released);
}

TEST_CASE("holding past the long-press threshold publishes a LongPressed event") {
    constexpr uint16_t LONG_PRESS_MS = 200;
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button(0, &pin, logger, LONG_PRESS_MS);
    service.add_button(&button);

    press_debounced(button, pin, service);
    for (uint16_t held = 0; held < LONG_PRESS_MS; held += 10) {
        service.update(10);
    }

    REQUIRE_FALSE(queue.sent.empty());
    ButtonPayload payload = ButtonPayload::unpack(queue.sent.back().payload());
    CHECK(payload.state == ButtonEventState::LongPressed);
}

TEST_CASE("a double tap's second release publishes Released then DoubleTapped") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    FakePin pin;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button(0, &pin, logger);
    service.add_button(&button);

    press_debounced(button, pin, service);
    release_debounced(button, pin, service);
    queue.sent.clear();

    press_debounced(button, pin, service);
    release_debounced(button, pin, service);

    REQUIRE(queue.sent.size() >= 2);
    ButtonPayload second_to_last = ButtonPayload::unpack(queue.sent[queue.sent.size() - 2].payload());
    ButtonPayload last = ButtonPayload::unpack(queue.sent.back().payload());
    CHECK(second_to_last.state == ButtonEventState::Released);
    CHECK(last.state == ButtonEventState::DoubleTapped);
}

TEST_CASE("two buttons publish events tagged with their own distinct ids") {
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(runner, queue);

    FakePin pin1, pin2;
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    Button<FakePin, FakeSink> button1(0, &pin1, logger);
    Button<FakePin, FakeSink> button2(1, &pin2, logger);
    service.add_button(&button1);
    service.add_button(&button2);

    pin1.reading = false; // active-low: pressed
    pin2.reading = false;
    for (int i = 0; i < 4; ++i) {
        service.update(10);
    }

    bool saw_id0 = false;
    bool saw_id1 = false;
    for (const Event& e : queue.sent) {
        ButtonPayload payload = ButtonPayload::unpack(e.payload());
        if (payload.id == 0) saw_id0 = true;
        if (payload.id == 1) saw_id1 = true;
    }
    CHECK(saw_id0);
    CHECK(saw_id1);
}
