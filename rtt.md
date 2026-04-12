# RTT setup

## Start GDB

Kør GDB med "arm-none-eabi-gdb din_fil.elf"

file F:/git/electronics/modular-midi/build/projects/hub-master/hub_master.elf
F:\git\electronics\modular-midi\build\projects\hub-master\hub_master.elf

target extended-remote \\.\COM5
monitor swdp_scan
attach 1

## RTT gdb commands
https://black-magic.org/usage/rtt.html

monitor rtt status

monitor rtt 
monitor rtt enable

print _SEGGER_RTT

mon rtt cb