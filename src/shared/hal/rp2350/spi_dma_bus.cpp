#include "spi_dma_bus.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"

// sck_pin/tx_pin/cs_pin are peripheral identity params per HAL.md, not
// logically interchangeable despite the shared type.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
SpiDmaBus::SpiDmaBus(spi_inst_t* spi, uint sck_pin, uint tx_pin, uint cs_pin, uint32_t baudrate)
    : m_spi(spi), m_dma_channel(dma_claim_unused_channel(true)) {
    spi_init(m_spi, baudrate);
    set_format(16);

    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(tx_pin, GPIO_FUNC_SPI);
    // Hand CS to the PL022 peripheral itself (see header comment) instead of
    // leaving it as a plain SIO-driven GpioOutputPin.
    gpio_set_function(cs_pin, GPIO_FUNC_SPI);

    dma_channel_config c = dma_channel_get_default_config(m_dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_bswap(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(m_spi, true));
    dma_channel_configure(
        m_dma_channel,
        &c,
        &spi_get_hw(m_spi)->dr, // Write to SPI data register
        nullptr,                // Read address is set per-transfer in write_dma()
        0,                      // Transfer count is set per-transfer in write_dma()
        false                   // Don't start immediately
    );
}

void SpiDmaBus::set_format(uint8_t bits) {
    spi_set_format(m_spi, bits, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
}

void SpiDmaBus::write_blocking(const uint8_t* data, size_t len) {
    spi_write_blocking(m_spi, data, len);
}

void SpiDmaBus::write_dma(const uint16_t* data, size_t len) {
    dma_channel_set_read_addr(m_dma_channel, data, false);
    dma_channel_set_trans_count(m_dma_channel, len, true); // true triggers the transfer
}

bool SpiDmaBus::busy() const {
    return dma_channel_is_busy(m_dma_channel) || spi_is_busy(m_spi);
}
