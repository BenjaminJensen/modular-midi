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
    : m_pio(pio), m_sm(sm), m_dma_channel(dma_claim_unused_channel(true)) {
    pio_sm_claim(m_pio, m_sm);
    uint offset = ensure_program_loaded(m_pio);

    // 4 sysclock cycles per bit: 2 instructions, each 1 cycle + a [1] delay.
    float clkdiv = static_cast<float>(clock_get_hz(clk_sys)) / (4.0f * static_cast<float>(scl_hz));
    st7789_8bit_program_init(m_pio, m_sm, offset, sda_pin, scl_pin, clkdiv);

    dma_channel_config c = dma_channel_get_default_config(m_dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_bswap(&c, true); // RGB565-endianness-vs-byte-oriented-SPI fixup (doc §4)
    channel_config_set_dreq(&c, pio_get_dreq(m_pio, m_sm, true));
    // The SM is shift-left/MSB-first with an 8-bit autopull threshold (see
    // st7789_spi.pio), so a 16-bit-wide DMA beat must land in the FIFO
    // word's *upper* halfword - the SM shifts out whatever's on the MSB
    // side of the OSR first. txf[sm] is 32-bit and word-aligned; the cast
    // below targets its upper 16 bits (byte offset +2), not the lower
    // (default) half a plain uint32_t* would hit.
    auto* txf_hi = reinterpret_cast<volatile uint16_t*>(&m_pio->txf[m_sm]) + 1;
    dma_channel_configure(
        m_dma_channel,
        &c,
        txf_hi,  // Write to this SM's TX FIFO (upper halfword lane)
        nullptr, // Read address is set per-transfer in write_dma()
        0,       // Transfer count is set per-transfer in write_dma()
        false    // Don't start immediately
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
    clear_tx_stall(m_pio, m_sm);
    dma_channel_set_read_addr(m_dma_channel, data, false);
    dma_channel_set_trans_count(m_dma_channel, len, true); // true triggers the transfer
}

bool PioSpiBus::busy() const {
    return dma_channel_is_busy(m_dma_channel) || !tx_stalled(m_pio, m_sm);
}
