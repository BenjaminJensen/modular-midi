#pragma once

#include <cstddef>
#include <cstdint>
#include "hardware/spi.h"

// SpiBus implementation wrapping one SPI peripheral plus a self-claimed DMA
// channel for async pixel bursts. No IRQ involved: busy() polls the DMA
// engine and the SPI shift register directly, so completion is detected by
// the caller polling rather than an interrupt callback. That also means
// multiple instances (distinct spi_inst_t/pins) never share any interrupt
// state - each owns its DMA channel outright.
class SpiDmaBus {
public:
    SpiDmaBus(spi_inst_t* spi, uint sck_pin, uint tx_pin, uint32_t baudrate);

    SpiDmaBus(const SpiDmaBus&) = delete;
    SpiDmaBus& operator=(const SpiDmaBus&) = delete;

    void set_format(uint8_t bits);
    void write_blocking(const uint8_t* data, size_t len);
    void write_dma(const uint16_t* data, size_t len);
    [[nodiscard]] bool busy() const;

private:
    spi_inst_t* m_spi;
    int m_dma_channel;
};
