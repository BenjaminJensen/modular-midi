#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "services/system_service.h"
#include "services/button_payload.h"
#include "logger.h"
#include "mocks/fake_sink.h"
#include "mocks/fake_task_runner.h"
#include "mocks/fake_event_queue.h"

namespace {
    constexpr uint8_t DISPLAY_COUNT = 4;
    using TestMailbox = Mailbox<DISPLAY_COUNT>;
    using Service = SystemService<FakeSink, FakeTaskRunner, FakeEventQueue, DISPLAY_COUNT>;
}

TEST_CASE("update() with an empty queue logs nothing") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    service.update();

    CHECK(sink.lines.empty());
}

TEST_CASE("a Pressed button event logs 'event: button <id> pressed'") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(4, ButtonEventState::Pressed));
    service.update();

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "event: button 4 pressed");
}

TEST_CASE("a Released button event logs 'event: button <id> released'") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(1, ButtonEventState::Released));
    service.update();

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "event: button 1 released");
}

TEST_CASE("a LongPressed button event logs 'event: button <id> long press'") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(2, ButtonEventState::LongPressed));
    service.update();

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "event: button 2 long press");
}

TEST_CASE("a DoubleTapped button event logs 'event: button <id> double tap'") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(0, ButtonEventState::DoubleTapped));
    service.update();

    REQUIRE_FALSE(sink.lines.empty());
    CHECK(sink.lines.back() == "event: button 0 double tap");
}

TEST_CASE("update() drains every queued event, logging them in order") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(0, ButtonEventState::Pressed));
    queue.sent.push_back(ButtonPayload::pack(0, ButtonEventState::Released));
    queue.sent.push_back(ButtonPayload::pack(0, ButtonEventState::DoubleTapped));
    service.update();

    REQUIRE(sink.lines.size() == 3);
    CHECK(sink.lines[0] == "event: button 0 pressed");
    CHECK(sink.lines[1] == "event: button 0 released");
    CHECK(sink.lines[2] == "event: button 0 double tap");
}

TEST_CASE("start() hands the task entry point and this-context to the runner") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    service.start();

    CHECK(runner.started_entry != nullptr);
    CHECK(runner.started_context == &service);
}

TEST_CASE("a Pressed button event writes 'B<id>: Pressed' to display 0") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(0, ButtonEventState::Pressed));
    service.update();

    Label label;
    REQUIRE(mailbox.take_if_dirty(0, label));
    CHECK(std::string_view(label.text_cstr()) == "B0: Pressed");
}

TEST_CASE("a button event's label reflects its id and every ButtonEventState") {
    std::array cases = {
        std::pair{ButtonEventState::Pressed, std::string_view("B2: Pressed")},
        std::pair{ButtonEventState::Released, std::string_view("B2: Released")},
        std::pair{ButtonEventState::LongPressed, std::string_view("B2: LongPressed")},
        std::pair{ButtonEventState::DoubleTapped, std::string_view("B2: DoubleTapped")},
    };

    for (auto [state, expected_text] : cases) {
        FakeSink sink;
        Logger<FakeSink> logger(sink);
        FakeTaskRunner runner;
        FakeEventQueue queue;
        TestMailbox mailbox;
        Service service(logger, runner, queue, mailbox);

        queue.sent.push_back(ButtonPayload::pack(2, state));
        service.update();

        Label label;
        REQUIRE(mailbox.take_if_dirty(0, label));
        CHECK(std::string_view(label.text_cstr()) == expected_text);
    }
}

TEST_CASE("every button's events land on display 0 regardless of button id") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    TestMailbox mailbox;
    Service service(logger, runner, queue, mailbox);

    queue.sent.push_back(ButtonPayload::pack(3, ButtonEventState::Pressed));
    service.update();

    Label label;
    CHECK_FALSE(mailbox.take_if_dirty(1, label));
    CHECK_FALSE(mailbox.take_if_dirty(2, label));
    CHECK_FALSE(mailbox.take_if_dirty(3, label));
    REQUIRE(mailbox.take_if_dirty(0, label));
    CHECK(std::string_view(label.text_cstr()) == "B3: Pressed");
}
