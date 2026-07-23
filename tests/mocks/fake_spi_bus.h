#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// SpiBus double: records every blocking write's bytes (concatenated, in call
// order) and the most recent DMA write's pointer/length, so tests can assert
// on a driver's exact protocol framing. busy() is driven entirely by the
// test via busy_polls_remaining - decrementing once per call and reporting
// true until it hits zero - rather than trying to simulate real DMA/SPI
// timing.
class FakeSpiBus {
public:
    std::vector<uint8_t> blocking_writes;
    uint8_t last_format = 0;
    const uint16_t* last_dma_data = nullptr;
    size_t last_dma_len = 0;
    int dma_write_count = 0;
    int busy_polls_remaining = 0;

    void set_format(uint8_t bits) {
        last_format = bits;
    }

    void write_blocking(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            blocking_writes.push_back(data[i]);
        }
    }

    void write_dma(const uint16_t* data, size_t len) {
        last_dma_data = data;
        last_dma_len = len;
        ++dma_write_count;
    }

    bool busy() {
        if (busy_polls_remaining > 0) {
            --busy_polls_remaining;
            return true;
        }
        return false;
    }
};
