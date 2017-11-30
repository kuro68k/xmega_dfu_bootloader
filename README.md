# xmega_dfu_bootloader
DFU bootloader for XMEGA parts

Atmel's supplied DFU bootloader does not compile to under 4k with GCC. This one does.

- 3.5k with GCC
- Supports flash and EEPROM, other memories easy to add
- Tested with dfu-util

Known limitations:

- EEPROM size must be a multiple of app section page size (true for all XMEGA parts in 2017)


Instructions: Create dfu_config.h (example supplied). Set configuration options. Adjust the project settings if required (particularly the target device and .text section address in the linker memory section). Check that the compiled bootloader fits into your bootloader section, especially if you have a 4k device.


Based on my fork of Kevin Mehall's minimal USB stack (https://github.com/kuro68k/usb_km_xmega).

Licence: GPL V3
