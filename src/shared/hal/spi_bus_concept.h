#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

// An SPI bus capable of blocking byte-oriented writes (commands/params) and
// async DMA-driven word-oriented writes (pixel bursts), with a combined busy
// signal covering both the DMA engine and the SPI shift register. Deliberately
// knows nothing about device selection (CS) or DC framing - that's protocol-
// specific and stays with the peripheral driver using the bus.
template<typename T>
concept SpiBus = requires(T& bus, uint8_t bits, const uint8_t* bytes, size_t n,
                          const uint16_t* pixels, size_t len) {
    { bus.set_format(bits) } -> std::same_as<void>;
    { bus.write_blocking(bytes, n) } -> std::same_as<void>;
    { bus.write_dma(pixels, len) } -> std::same_as<void>;
    { bus.busy() } -> std::convertible_to<bool>;
};
