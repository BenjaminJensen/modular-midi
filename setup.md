# Project Setup

## Tools

To build and debug this project, you will need the following tools installed:

- **GCC arm**: The GNU Compiler Collection toolchain for ARM architectures (e.g., `arm-none-eabi-gcc`).
- **ninja**: A small and incredibly fast build system.
- **cmake**: A cross-platform build system generator used to configure the project.
- **SEGGER J-Link software**: Provides `JLink.exe` used for flashing (see [tools/tools.md](tools/tools.md)).
- **SEGGER Ozone**: The debugger used for this project (see [tools/tools.md](tools/tools.md) and `tools/ozone.jdebug`).

## FreeRTOS
As of now 30th March 2026, the official version of FreeRTOS does not support the RP2350, therefore the submodule is to the raspberrypi maintained of FreeRTOS

## Debugging

Debugging is done through SEGGER Ozone, not VS Code's built-in debugger. Flashing is a separate step done via the VS Code task described in [tools/tools.md](tools/tools.md). See [rtt.md](rtt.md) for viewing log output.
