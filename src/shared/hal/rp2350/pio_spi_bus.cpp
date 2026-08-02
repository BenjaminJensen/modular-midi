#include "pio_spi_bus.h"

#include <array>
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "st7789_spi.pio.h"

namespace {

// pio_add_program() loads shared instruction memory once per PIO block -
// every PioSpiBus instance sharing that block must reuse the same offset
// rather than re-adding the program. Guarded the same way
// Display::ensure_lvgl_globals_initialized() guards LVGL's one-time init.
uint ensure_program_loaded(PIO pio) {
    static std::array<int, NUM_PIOS> offset = {-1, -1, -1};
    uint index = pio_get_index(pio);
    if (offset[index] < 0) {
        offset[index] = pio_add_program(pio, &st7789_8bit_program);
    }
    return static_cast<uint>(offset[index]);
}

// TXSTALL is sticky and write-1-to-clear: cleared right before a burst
// starts, it only becomes set again once the SM has actually run dry (FIFO
// empty *and* the last bits have finished shifting out) - the "FIFO empty
// AND SM idle, not mid-shift" check architecture doc §5 calls for, in one flag.
void clear_tx_stall(PIO pio, uint sm) {
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
}

bool tx_stalled(PIO pio, uint sm) {
    return (pio->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + sm))) != 0;
}

} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PioSpiBus::PioSpiBus(PIO pio, uint sm, uint sda_pin, uint scl_pin, uint32_t scl_hz)
    : m_pio(pio), m_sm(sm),
      m_swap_dma_channel(dma_claim_unused_channel(true)),
      m_stream_dma_channel(dma_claim_unused_channel(true)) {
    pio_sm_claim(m_pio, m_sm);
    uint offset = ensure_program_loaded(m_pio);

    // 4 sysclock cycles per bit: 2 instructions, each 1 cycle + a [1] delay.
    float clkdiv = static_cast<float>(clock_get_hz(clk_sys)) / (4.0f * static_cast<float>(scl_hz));
    st7789_8bit_program_init(m_pio, m_sm, offset, sda_pin, scl_pin, clkdiv);

    // Stage 2's destination never changes call-to-call: txf[sm]'s base
    // address, narrow (8-bit) writes only - relying on RP2040/2350's IO
    // fabric replicating a narrow store across all four byte lanes of the
    // bus transaction (confirmed against Raspberry Pi's own pico-examples
    // pio/st7789_lcd/st7789_lcd.pio, which uses this exact technique via
    // `*(volatile uint8_t*)&pio->txf[sm] = x`). There's no specific byte
    // lane to target - a +3 offset (targeting what I mistakenly assumed was
    // "the MSB lane") isn't how this works and doesn't reliably land the
    // value where the shift-left SM reads from.
    auto* txf_byte = reinterpret_cast<volatile uint8_t*>(&m_pio->txf[m_sm]);
    dma_channel_config stream_c = dma_channel_get_default_config(m_stream_dma_channel);
    channel_config_set_transfer_data_size(&stream_c, DMA_SIZE_8);
    channel_config_set_dreq(&stream_c, pio_get_dreq(m_pio, m_sm, true));
    dma_channel_configure(
        m_stream_dma_channel,
        &stream_c,
        txf_byte, // Write to this SM's TX FIFO (narrow byte write)
        nullptr,      // Read address is set per-transfer in write_dma()
        0,            // Transfer count is set per-transfer in write_dma()
        false         // Don't start immediately
    );
}

void PioSpiBus::set_format(uint8_t /*bits*/) {}

void PioSpiBus::write_blocking(const uint8_t* data, size_t len) {
    clear_tx_stall(m_pio, m_sm);
    for (size_t i = 0; i < len; ++i) {
        // Left-justify to the OSR's MSB side - pio_sm_put_blocking() is a
        // plain 32-bit register store (see hardware/pio.h), not a narrow
        // bus write, so there's no automatic justification: an unshifted
        // byte would shift out as 24 leading zero bits before the real
        // data, which the 8-bit autopull threshold would never reach.
        pio_sm_put_blocking(m_pio, m_sm, static_cast<uint32_t>(data[i]) << 24);
    }

    // pio_sm_put_blocking() only blocks until the FIFO has room, not until
    // the bits have actually been clocked out over SCL - unlike
    // spi_write_blocking() (SpiDmaBus), which is genuinely done by the time
    // it returns. ST7789::send_cmd()/send_data()/set_window() raise CS
    // immediately after calling write_blocking() with no poll in between,
    // so this has to spin here to preserve that "truly complete" contract -
    // otherwise CS gets deasserted mid-shift and corrupts the byte on the wire.
    while (!tx_stalled(m_pio, m_sm)) {
        tight_loop_contents();
    }
}

void PioSpiBus::write_dma(const uint16_t* data, size_t len) {
    // Stage 1: RAM-to-RAM byte-swap pre-pass. Both ends are plain memory (not
    // a FIFO push register), so there's no ambiguity about narrow-write
    // justification here - a straightforward DMA_SIZE_16+BSWAP transfer.
    // Blocking on this is fine: it's pure SRAM-to-SRAM bandwidth, not
    // throttled by the ~10 MHz SCL rate that dominates stage 2's runtime.
    dma_channel_config swap_c = dma_channel_get_default_config(m_swap_dma_channel);
    channel_config_set_transfer_data_size(&swap_c, DMA_SIZE_16);
    channel_config_set_bswap(&swap_c, true); // RGB565-endianness-vs-byte-oriented-SPI fixup (doc §4)
    // dma_channel_get_default_config() defaults write_increment to false -
    // correct for stage 2's fixed FIFO destination, but wrong here: this is
    // a RAM-to-RAM buffer copy, so the destination must advance too, or
    // every transfer overwrites m_swapped[0] and the rest of the buffer
    // never gets touched.
    channel_config_set_write_increment(&swap_c, true);
    dma_channel_configure(m_swap_dma_channel, &swap_c, m_swapped.data(), data, len, true);
    dma_channel_wait_for_finish_blocking(m_swap_dma_channel);

    // Stage 2: stream the now-swapped bytes into the PIO FIFO one byte per
    // DMA beat - the same granularity the CPU command path uses, so the
    // SM's 8-bit autopull threshold is never fed more than it can consume.
    clear_tx_stall(m_pio, m_sm);
    dma_channel_set_read_addr(m_stream_dma_channel, m_swapped.data(), false);
    dma_channel_set_trans_count(m_stream_dma_channel, len * 2, true); // *2: bytes, not pixels
}

bool PioSpiBus::busy() const {
    return dma_channel_is_busy(m_stream_dma_channel) || !tx_stalled(m_pio, m_sm);
}
