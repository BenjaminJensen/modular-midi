# Tools

## Segger JLink & VS Code
Ensure the JLink root folder is in the path variable:
1. In VS Code, go to File > Preferences > Settings.
2. Click the Open Settings (JSON) icon at the top right.
3. Add (or update) the terminal.integrated.env.windows section like this:

'''
"terminal.integrated.env.windows": {
    "PATH": "C:\\Program Files\\SEGGER\\JLink_V924a;${env:PATH}"
}
'''

## Segger flash task
1. ctrl+shift+p
2. Type in "task run"
3. Choose "J-Link: Flash RP2350"

## Segger Ozone (debugging)
Debugging is done through SEGGER Ozone rather than VS Code's built-in debugger.

1. Open `tools/ozone.jdebug` in Ozone.
2. It's pre-configured for the RP2350 (Cortex-M33 core 0) over J-Link/SWD, with the FreeRTOS-aware plugin enabled, and loads `build/src/projects/hub-master/hub_master.elf`.
3. RTT output (logging) appears automatically in Ozone's Terminal window once the target is running — see [rtt.md](../rtt.md).