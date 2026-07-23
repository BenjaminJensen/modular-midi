#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "shared/hal/spi_bus_concept.h"
#include "shared/hal/gpio_output_pin_concept.h"
#include "shared/hal/delay_concept.h"

/*
    Hardware driver for the ST7789 display controller. Templated on the SPI
    bus, the three GPIO output pins (CS/DC/RST), and a blocking delay source,
    all injected by reference so the glue layer (main.cpp) owns and wires
    them - no pico-sdk dependency at all here, direct or transitive.
    Deliberately has no knowledge of LVGL: callers hand it raw pixel data
    plus a rectangle, nothing more.

    Completion of the async pixel-burst DMA transfer is polled, not interrupt-
    driven: is_busy() checks the underlying bus and, the moment it notices a
    transfer just finished, deselects CS itself. This is the only place that
    bookkeeping happens, so both the blocking waits used internally (before
    issuing a new command) and an external caller polling for "is my last
    flush done yet" share one source of truth.
*/
template<SpiBus SpiT, GpioOutputPin CsPinT, GpioOutputPin DcPinT, GpioOutputPin RstPinT, Delay DelayT>
class ST7789 {
public:
    ST7789(SpiT& spi, CsPinT& cs, DcPinT& dc, RstPinT& rst, DelayT& delay,
           uint16_t y_offset = 82, uint16_t x_offset = 18)
        : m_spi(spi), m_cs(cs), m_dc(dc), m_rst(rst), m_delay(delay),
          m_y_offset(y_offset), m_x_offset(x_offset) {}

    void init() {
        reset();
        m_delay.delay_ms(150);

        sleep_out();
        m_delay.delay_ms(150);

        // 16-bit/pixel (RGB 5-6-5), 65K colors
        set_pixel_format(0x05);

        // Memory Access Control - rotation/RGB order
        set_data_access(0x70);

        set_mode_on();
        m_delay.delay_ms(150);
    }

    void display_on() {
        send_cmd(0x29); // DISPON
        m_delay.delay_ms(100);
    }

    // A rectangle's corners - reordering them wouldn't reduce the swap risk this flags.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void update(const uint16_t* data, size_t len, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        wait_idle();

        set_window(static_cast<uint16_t>(x1), static_cast<uint16_t>(y1),
                   static_cast<uint16_t>(x2), static_cast<uint16_t>(y2));
        set_memory_write();

        m_spi.set_format(16);
        m_cs.write(false);
        m_dc.write(true);
        m_spi.write_dma(data, len);
        m_transfer_pending = true;
    }

    // True if a previously started pixel-burst DMA transfer is still in
    // flight. Has a side effect: the first call after the transfer actually
    // finishes deselects CS, so this doubles as the "finalize" step - callers
    // don't need a separate completion hook.
    bool is_busy() {
        bool still_busy = m_spi.busy();
        if (!still_busy && m_transfer_pending) {
            m_cs.write(true);
            m_transfer_pending = false;
        }
        return still_busy;
    }

private:
    SpiT& m_spi;
    CsPinT& m_cs;
    DcPinT& m_dc;
    RstPinT& m_rst;
    DelayT& m_delay;
    const uint16_t m_y_offset;
    const uint16_t m_x_offset;
    bool m_transfer_pending = false;

    void wait_idle() {
        while (is_busy()) {
            m_delay.spin_hint();
        }
    }

    void reset() {
        m_rst.write(false);
        m_delay.delay_ms(100);
        m_rst.write(true);
        m_delay.delay_ms(100);
    }

    void sleep_out() {
        send_cmd(0x11); // Sleep Out
    }

    void set_pixel_format(uint8_t format) {
        send_cmd(0x3A); // COLMOD
        send_data(format);
    }

    void set_data_access(uint8_t format) {
        send_cmd(0x36); // MADCTL
        send_data(format);
    }

    void set_mode_on() {
        send_cmd(0x13); // NORON
    }

    void set_memory_write() {
        send_cmd(0x2C); // RAMWR
    }

    void send_cmd(uint8_t cmd) {
        wait_idle();
        m_spi.set_format(8);
        m_cs.write(false);
        m_dc.write(false); // DC low for command
        m_spi.write_blocking(&cmd, 1);
        m_cs.write(true);
        m_spi.set_format(16);
    }

    void send_data(uint8_t data) {
        wait_idle();
        m_spi.set_format(8);
        m_cs.write(false);
        m_dc.write(true); // DC high for data
        m_spi.write_blocking(&data, 1);
        m_cs.write(true);
        m_spi.set_format(16);
    }

    // A rectangle's corners - reordering them wouldn't reduce the swap risk this flags.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
        wait_idle();

        const auto x1a = static_cast<uint16_t>(m_x_offset + x1);
        const auto x2a = static_cast<uint16_t>(m_x_offset + x2);
        const auto y1a = static_cast<uint16_t>(m_y_offset + y1);
        const auto y2a = static_cast<uint16_t>(m_y_offset + y2);

        m_spi.set_format(8);
        m_cs.write(false);

        m_dc.write(false); // CASET
        uint8_t caset = 0x2A;
        m_spi.write_blocking(&caset, 1);
        m_dc.write(true);
        const std::array<uint8_t, 4> x_bytes = {
            static_cast<uint8_t>(x1a >> 8), static_cast<uint8_t>(x1a & 0xFF),
            static_cast<uint8_t>(x2a >> 8), static_cast<uint8_t>(x2a & 0xFF),
        };
        m_spi.write_blocking(x_bytes.data(), x_bytes.size());

        m_dc.write(false); // RASET
        uint8_t raset = 0x2B;
        m_spi.write_blocking(&raset, 1);
        m_dc.write(true);
        const std::array<uint8_t, 4> y_bytes = {
            static_cast<uint8_t>(y1a >> 8), static_cast<uint8_t>(y1a & 0xFF),
            static_cast<uint8_t>(y2a >> 8), static_cast<uint8_t>(y2a & 0xFF),
        };
        m_spi.write_blocking(y_bytes.data(), y_bytes.size());

        m_cs.write(true);
        m_spi.set_format(16);
    }
};
