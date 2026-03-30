# Project Setup

## Tools

To build and debug this project, you will need the following tools installed:

- **GCC arm**: The GNU Compiler Collection toolchain for ARM architectures (e.g., `arm-none-eabi-gcc`).
- **ninja**: A small and incredibly fast build system.
- **cmake**: A cross-platform build system generator used to configure the project.
- **cortex-debug**: A Visual Studio Code extension that provides debugging support for ARM Cortex-M microcontrollers.
- **blackmagic-probe**: An open-source JTAG/SWD debugging tool for ARM microcontrollers.

## FreeRTOS
As of now 30th March 2026, the official version of FreeRTOS does not support the RP2350, therefore the submodule is to the raspberrypi maintained of FreeRTOS

## Cortex-Debug Setup

The project uses the `cortex-debug` VS Code extension to interface with the Black Magic Probe. 
This configuration is based on the Black Magic Probe Specific Configuration guide.

To set up your debugger, create or update your `.vscode/launch.json` file with the following configuration. It sets the GDB serial port to `COM5` and points to the right build directory:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Cortex Debug (BMP)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "bmp",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/projects/hub-master/hub-master.elf",
            "BMPGDBSerialPort": "COM5",
            "interface": "swd",
            "device": "RP2040",
            "runToEntryPoint": "main"
        }
    ]
}
```