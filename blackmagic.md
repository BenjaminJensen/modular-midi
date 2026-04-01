# BlackmMgig Probe setup

## Build

### Delete build directory

```
rm -rf build
```

### Run Meason build command
```
meson setup build --cross-file=cross-file/bluepill.ini
```

Go to "/build" and run "ninja.exe" to create the ".elf" files for bootloader and app

#### Changes to cross-file

Using the "bluepill" profile with changes for "targets", "bootloader" and "RTT"
```
[binaries]
c = 'arm-none-eabi-gcc'
cpp = 'arm-none-eabi-g++'
ld = 'arm-none-eabi-gcc'
ar = 'arm-none-eabi-ar'
nm = 'arm-none-eabi-nm'
strip = 'arm-none-eabi-strip'
objcopy = 'arm-none-eabi-objcopy'
objdump = 'arm-none-eabi-objdump'
size = 'arm-none-eabi-size'

[host_machine]
system = 'bare-metal'
cpu_family = 'arm'
cpu = 'arm'
endian = 'little'

[project options]
probe = 'bluepill'
targets = **'cortexm,rp,stm'**
rtt_support = **true**
bmd_bootloader = true
```

## DFU loading

Download "dfu-util" from [dfu-util](http://sourceforge.net/p/dfu-util/)

Go to the "build" directory and run this command:
```
dfu-util -d 1d50:6018,:6017 -s 0x08002000:leave -D blackmagic_bluepill_firmware.bin
```

## SEGGER RTT
The probe is RTT enabled, the BlackMagic probe implementation is a bit different than others as the probe opens a com port that outputs the RTT stream.

I my case two com ports are created:
- COM5: This is the GDB server port
- COM6: This is the RTT server port

So if you run GDB and enable RTT with the commands "monitor rtt enable" and "monitor rtt" it will start to output at COM6.
The port settings are:
- Baud Rate: Usually doesn't matter for RTT (as it's virtual), but setting it to 115200 is a safe standard.
- Data Bits: 8
- Stop Bits: 1
- Parity: None