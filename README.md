# xmega_dfu_bootloader
DFU bootloader for XMEGA parts

Atmel's supplied DFU bootloader does not compile to under 4k with GCC. This one does.

- 3.5k with GCC
- Supports flash and EEPROM, other memories easy to add
- Tested with dfu-util

Based on my fork of Kevin Mehall's minimal USB stack (https://github.com/kuro68k/usb_km_xmega).
