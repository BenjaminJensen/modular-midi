# 1. Connect to BMP via COM-porten
target extended-remote \\.\COM5

# 2. Scan for RP2350 targets
monitor swdp_scan

# 3. Attach to first core (Core 0)
attach 1

# 4. Make access to the full memory availlable
set mem inaccessible-by-default off

# 5. Load elf file to target, elf file should be supplied to the gdb start command
load

# Enable RTT
monitor rtt 
monitor rtt enable

# Run the code - On the BMP RTT will stop when GDB stops
continue
