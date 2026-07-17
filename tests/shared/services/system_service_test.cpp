#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "services/system_service.h"
#include "services/button_payload.h"
#include "logger.h"
#include "mocks/fake_sink.h"
#include "mocks/fake_task_runner.h"
#include "mocks/fake_event_queue.h"

namespace {
    using Service = SystemService<FakeSink, FakeTaskRunner, FakeEventQueue>;
}

TEST_CASE("update() with an empty queue logs nothing") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(logger, runner, queue);

    service.update();

    CHECK(sink.lines.empty());
}

TEST_CASE("a Pressed button event logs 'event: button <id> pressed'") {
    FakeSink sink;
    Logger<FakeSink> logger(sink);
    FakeTaskRunner runner;
    FakeEventQueue queue;
    Service service(logger, runner, queue);

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
    Service service(logger, runner, queue);

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
    Service service(logger, runner, queue);

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
    Service service(logger, runner, queue);

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
    Service service(logger, runner, queue);

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
    Service service(logger, runner, queue);

    service.start();

    CHECK(runner.started_entry != nullptr);
    CHECK(runner.started_context == &service);
}
