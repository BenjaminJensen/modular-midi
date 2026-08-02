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
//
// cs_pin is given to the PL022 peripheral itself (GPIO_FUNC_SPI) rather than
// left to the caller as a plain GpioOutputPin: this SPI instance's CSn
// alternate-function pin is hardware-driven in lockstep with the same
// shift-register clock that moves the data, so there's no software-timing
// race between "CPU thinks the transfer is done" and "CS is actually
// deasserted" - the two are the same hardware event. Requires CPHA=1 (see
// set_format()) - with CPHA=0 the peripheral toggles CS between every single
// byte, which would break multi-byte transactions like set_window()'s
// CASET/RASET writes; CPHA=1 holds CS low across continuous back-to-back
// words and only releases it on a genuine gap in the data stream.
class SpiDmaBus {
public:
    SpiDmaBus(spi_inst_t* spi, uint sck_pin, uint tx_pin, uint cs_pin, uint32_t baudrate);

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
