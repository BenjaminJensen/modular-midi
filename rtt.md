# RTT (logging) via Ozone

RTT output (printf-style logging from the target) is viewed through SEGGER Ozone — there is no GDB/COM-port step.

1. Open `tools/ozone.jdebug` in Ozone (see [tools/tools.md](tools/tools.md) for setup).
2. Start a debug session (Download & Reset, or Attach).
3. RTT output appears automatically in Ozone's **Terminal** window once the target is running.

RTT buffer configuration lives in [src/shared/hal/rp2350/SEGGER_RTT_Conf.h](src/shared/hal/rp2350/SEGGER_RTT_Conf.h).
