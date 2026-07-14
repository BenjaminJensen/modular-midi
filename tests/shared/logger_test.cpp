#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "logger.h"
#include "mocks/fake_sink.h"

#include <utility>

TEST_CASE("streaming mixed types produces the expected concatenated line") {
    FakeSink sink;
    Logger<FakeSink> log(sink);

    log.debug() << "x=" << 42 << " y=" << true;

    REQUIRE(sink.lines.size() == 1);
    CHECK(sink.lines[0] == "x=42 y=true");
}

TEST_CASE("a line below the configured minimum level does not reach the sink") {
    FakeSink sink;
    Logger<FakeSink> log(sink);
    log.set_min_level(LogLevel::Warn);

    log.debug() << "should not appear";
    CHECK(sink.lines.empty());

    log.warn() << "should appear";
    REQUIRE(sink.lines.size() == 1);
    CHECK(sink.lines[0] == "should appear");
}

TEST_CASE("multiple appends to one log line still produce exactly one write") {
    FakeSink sink;
    Logger<FakeSink> log(sink);

    log.info() << "a=" << 1 << ", b=" << 2 << ", c=" << 3;

    CHECK(sink.lines.size() == 1);
}

TEST_CASE("a moved-from log stream does not double-flush") {
    FakeSink sink;
    Logger<FakeSink> log(sink);

    {
        LogStream<FakeSink> a = log.debug();
        a << "hello";
        LogStream<FakeSink> b = std::move(a);
        (void)b;
    }

    REQUIRE(sink.lines.size() == 1);
    CHECK(sink.lines[0] == "hello");
}
