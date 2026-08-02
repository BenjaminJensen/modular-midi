#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "hardware/pio.h"

// SpiBus implementation driving the ST7789 "dumb serializer" PIO program
// (st7789_spi.pio) over one PIO state machine plus two self-claimed DMA
// channels - see architecture/RP2350 Quad-Display Concurrent Architecture.md.
// Multiple instances can share the same PIO block: its instruction memory is
// loaded once (guarded internally), but each instance still owns an
// independent SM and its own DMA channels, so concurrent instances never
// collide. Same no-IRQ polling contract as SpiDmaBus: busy() polls the DMA
// engine and the PIO TX-stall debug flag directly, nothing here is
// interrupt-driven.
//
// Pixel bursts need two DMA stages, not one: BSWAP (needed to fix RGB565's
// little-endian-in-RAM vs MSB-first-on-the-wire mismatch) only has an effect
// at >=16-bit transfer granularity, but the SM's 8-bit autopull threshold
// can only safely be fed byte-granularity FIFO writes (confirmed on
// hardware - a direct DMA_SIZE_16 write into the FIFO produced a
// correctly-timed burst of all-zero bits, meaning the SM never actually saw
// the real data). So write_dma() first byte-swaps into a scratch buffer via
// a RAM-to-RAM DMA_SIZE_16+BSWAP transfer, then streams that already-swapped
// buffer to the FIFO one byte at a time (DMA_SIZE_8), matching the same
// byte-at-a-time granularity write_blocking()'s CPU path already uses
// successfully.
class PioSpiBus {
public:
    // Generously covers this project's single-panel worst case (a full
    // 284x76 screen) rather than depending on Display's chunk size - PioSpiBus
    // is a generic HAL class and shouldn't know about any specific panel's
    // dimensions.
    static constexpr size_t MAX_TRANSFER_PIXELS = 284 * 76;

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
    int m_swap_dma_channel;
    int m_stream_dma_channel;
    std::array<uint16_t, MAX_TRANSFER_PIXELS> m_swapped{};
};
