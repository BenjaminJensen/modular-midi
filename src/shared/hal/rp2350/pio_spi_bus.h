#pragma once

#include <cstddef>
#include <cstdint>
#include "hardware/pio.h"

// SpiBus implementation driving the ST7789 "dumb serializer" PIO program
// (st7789_spi.pio) over one PIO state machine plus a self-claimed DMA
// channel - see architecture/RP2350 Quad-Display Concurrent Architecture.md.
// Multiple instances can share the same PIO block: its instruction memory is
// loaded once (guarded internally), but each instance still owns an
// independent SM and DMA channel, so concurrent instances never collide.
// Same no-IRQ polling contract as SpiDmaBus: busy() polls the DMA engine and
// the PIO TX-stall debug flag directly, nothing here is interrupt-driven.
class PioSpiBus {
public:
    // sda_pin/scl_pin are peripheral identity params per HAL.md, not
    // logically interchangeable despite the shared type.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    PioSpiBus(PIO pio, uint sm, uint sda_pin, uint scl_pin, uint32_t scl_hz);

    PioSpiBus(const PioSpiBus&) = delete;
    PioSpiBus& operator=(const PioSpiBus&) = delete;

    // No-op: the SM always pulls 8 bits regardless of caller intent (see
    // architecture doc §0/§3) - kept only so SpiBus's contract stays uniform.
    void set_format(uint8_t bits);
    void write_blocking(const uint8_t* data, size_t len);
    void write_dma(const uint16_t* data, size_t len);
    [[nodiscard]] bool busy() const;

private:
    PIO m_pio;
    uint m_sm;
    int m_dma_channel;
};
