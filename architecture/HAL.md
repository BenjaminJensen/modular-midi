# Hardware Abstraction Layer (HAL) Architecture
This project utilizes a zero-overhead, compile-time hardware abstraction layer targeting the Raspberry Pi RP2350 platform. The core architecture relies on the Dependency Inversion Principle (DIP) to decouple high-level application logic from low-level hardware registers and SDKs without sacrificing execution speed or memory efficiency.

## 1. Architectural Core Principles
Zero Execution Overhead: Avoids traditional runtime polymorphism (virtual functions and vtables). Instead, static dispatch via C++20 Concepts and templates allows the compiler to inline peripheral calls directly.

Zero Dynamic Memory Allocation: All instances are statically or stack-allocated within the initialization/glue layer (main.cpp). There is no heap usage (new/delete), ensuring deterministic runtime performance.

Scale Without Bloat: Peripheral identification (such as GPIO pin numbers or SPI instance pointers) is passed via constructor runtime parameters rather than template arguments. This ensures a single unified C++ class type per peripheral, preventing code duplication when utilizing multiple instances.

## 2. Directory Structure
```
src/
├── CMakeLists.txt
├── main.cpp                  # Glue layer: Instantiates hardware & injects into app
├── app/                      # Pure application logic (oblivious to target hardware)
│   └── display_driver.hpp    # Consumes abstract concepts
└── hal/                      # Hardware Abstraction Layer
    ├── spi_concept.hpp       # C++20 concept definition for SPI
    ├── gpio_concept.hpp      # C++20 concept definition for GPIO
    │
    └── rp2350/               # RP2350-specific implementation layer
        ├── CMakeLists.txt    # Generates the interface library for the target
        ├── rp2350_spi.hpp    # Zero-overhead C++ wrapper around Pico SDK
        └── rp2350_gpio.hpp   # Zero-overhead C++ wrapper around Pico SDK
```

## 3. Data Flow & Compilation Mechanics
The Interface Layer (src/hal/): Defines compile-time constraints using C++20 Concepts. It enforces what public methods a peripheral must expose (e.g., write(), read()) but compiles to zero bytes of code.

The Implementation Layer (src/hal/rp2350/): Wraps raw Pico SDK functions (hardware_spi, hardware_gpio) inside inline C++ methods. It does not inherit from any base class.

The Application Layer (src/app/): Written as templates or constrained types that only accept objects fulfilling a specific HAL Concept.

The Glue Layer (src/main.cpp): Resolves the dependencies. It instantiates the concrete RP2350 classes, passes the relevant hardware handles (spi0, pin IDs), and injects them into the application drivers.

## 4. Multi-Platform Strategy via CMake
Target architectures are swapped cleanly at compilation time using CMake cache variables. The application code remains completely unchanged, while CMake selectively compiles and links the matching HAL implementation.

To compile for the physical RP2350 hardware:
```
Bash
cmake -DPLATFORM_TARGET=RP2350 -B build
cmake --build build
Build Selection Logic
CMake
# src/CMakeLists.txt
if(PLATFORM_TARGET STREQUAL "RP2350")
    add_subdirectory(hal/rp2350)
    target_link_libraries(app_bin PRIVATE hal_rp2350 pico_stdlib hardware_spi)
elseif(PLATFORM_TARGET STREQUAL "NATIVE_SIM")
    add_subdirectory(hal/native_sim)
    target_link_libraries(app_bin PRIVATE hal_sim)
endif()
```