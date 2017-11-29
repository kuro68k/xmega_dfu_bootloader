/*
 * dfu_config.h
 *
 */


#ifndef DFU_CONFIG_H_
#define DFU_CONFIG_H_


// Store the first page written in RAM until the last page is written and
// the bootloader enters the manifest state. The first page will be erased.
// If the firmware update is interrupted the bootloader can detect and empty
// page zero and go back into DFU mode.
#define	DELAYED_ZERO_PAGE



#endif /* DFU_CONFIG_H_ */