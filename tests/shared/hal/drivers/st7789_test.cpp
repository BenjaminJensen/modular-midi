#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <vector>
#include "hal/drivers/st7789.h"
#include "mocks/fake_spi_bus.h"
#include "mocks/fake_output_pin.h"
#include "mocks/fake_delay.h"

namespace {
    using Driver = ST7789<FakeSpiBus, FakeOutputPin, FakeOutputPin, FakeDelay>;

    // operator new being deleted is a compile-time property - a runtime CHECK can't
    // observe it, so this asserts on it directly via a requires-expression instead.
    // Guards against a future refactor silently dropping the `= delete`.
    template<typename T>
    concept HeapConstructible = requires(FakeSpiBus& spi, FakeOutputPin& pin, FakeDelay& delay) {
        new T(spi, pin, pin, delay);
    };
    static_assert(!HeapConstructible<Driver>,
                  "ST7789 must stay non-heap-constructible (operator new is deleted for the no-heap-anywhere rule)");
}

TEST_CASE("update() starts exactly one DMA write with the given data and length") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    std::array<uint16_t, 4> pixels = {1, 2, 3, 4};
    driver.update(pixels.data(), pixels.size(), 0, 0, 1, 1);

    CHECK(spi.dma_write_count == 1);
    CHECK(spi.last_dma_data == pixels.data());
    CHECK(spi.last_dma_len == pixels.size());
}

TEST_CASE("update() leaves DC high for the pixel burst") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    std::array<uint16_t, 1> pixels = {0};
    driver.update(pixels.data(), pixels.size(), 0, 0, 0, 0);

    CHECK(dc.level); // data mode
    CHECK(spi.last_format == 16);
}

TEST_CASE("is_busy() reflects the bus") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    std::array<uint16_t, 1> pixels = {0};
    driver.update(pixels.data(), pixels.size(), 0, 0, 0, 0);

    spi.busy_polls_remaining = 2;
    CHECK(driver.is_busy());
    CHECK(driver.is_busy());
    CHECK_FALSE(driver.is_busy()); // now idle
    CHECK_FALSE(driver.is_busy()); // calling again stays idle
}

TEST_CASE("update() waits for a prior transfer to finish before starting the next one") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    std::array<uint16_t, 1> pixels = {0};
    driver.update(pixels.data(), pixels.size(), 0, 0, 0, 0);

    spi.busy_polls_remaining = 3;
    driver.update(pixels.data(), pixels.size(), 0, 0, 0, 0); // must busy-poll (via spin_hint) until idle first

    CHECK(spi.dma_write_count == 2);
    CHECK(delay.spin_hint_count >= 3);
}

TEST_CASE("set_window frames CASET/RASET with the configured x/y offsets") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay, /*y_offset=*/82, /*x_offset=*/18);

    std::array<uint16_t, 1> pixels = {0};
    driver.update(pixels.data(), pixels.size(), /*x1=*/0, /*y1=*/0, /*x2=*/1, /*y2=*/1);

    const std::vector<uint8_t> expected = {
        0x2A, 0, 18, 0, 19, // CASET: x1=18, x2=19
        0x2B, 0, 82, 0, 83, // RASET: y1=82, y2=83
        0x2C,               // RAMWR
    };
    CHECK(spi.blocking_writes == expected);
}

TEST_CASE("zero offsets are honored in set_window's CASET/RASET framing") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay, /*y_offset=*/0, /*x_offset=*/0);

    std::array<uint16_t, 1> pixels = {0};
    driver.update(pixels.data(), pixels.size(), 0, 0, 1, 1);

    const std::vector<uint8_t> expected = {
        0x2A, 0, 0, 0, 1,
        0x2B, 0, 0, 0, 1,
        0x2C,
    };
    CHECK(spi.blocking_writes == expected);
}

TEST_CASE("init() sends the documented ST7789 init command sequence") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    driver.init();

    const std::vector<uint8_t> expected = {
        0x11,       // Sleep Out
        0x3A, 0x05, // COLMOD: 16-bit/pixel RGB565
        0x36, 0x70, // MADCTL: rotation/RGB order
        0x13,       // NORON
    };
    CHECK(spi.blocking_writes == expected);
    CHECK(rst.level); // reset pulse ends with RST held high
}

TEST_CASE("init() pulses RST and waits the documented settle times") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    driver.init();

    // reset(): 100 + 100; then three 150ms settle waits after each init step.
    CHECK(delay.total_delay_ms == 100 + 100 + 150 + 150 + 150);
}

TEST_CASE("display_on() sends DISPON and waits 100ms") {
    FakeSpiBus spi;
    FakeOutputPin dc, rst;
    FakeDelay delay;
    Driver driver(spi, dc, rst, delay);

    driver.display_on();

    REQUIRE_FALSE(spi.blocking_writes.empty());
    CHECK(spi.blocking_writes.back() == 0x29);
    CHECK(delay.total_delay_ms == 100);
}
