/*
 * dfu_config.h
 *
 */


#ifndef DFU_CONFIG_H_
#define DFU_CONFIG_H_


/* USB vendor and product IDs
 */
#define USB_VID		0x9999
#define USB_PID		0x1000


/* Generate a USB serial number from the MCU's unique identifiers. Can be
 * disabled to save flash memory.
 */
#define	USB_SERIAL_NUMBER


/* Store the first page written in RAM until the last page is written and
 * the bootloader enters the manifest state. The first page will be erased.
 * If the firmware update is interrupted the bootloader can detect and empty
 * page zero and go back into DFU mode.
 */
#define	DELAYED_ZERO_PAGE


/* Enable upload (read firmware from device) support.
 */
#define	UPLOAD_SUPPORT


/* Return true if the DFU bootloader should be started. DFU can be started
 * by some condition (button pressed, flash memory empty etc.) or by the
 * application firmware.
 */
static inline bool CheckStartConditions(void)
{
	if ((*(uint32_t *)(0) == 0x4c4f4144) ||				// "LOAD"
		(*(const __flash uint16_t *)(0) == 0xFFFF))		// reset vector blank
	{
		*(uint32_t *)(0) = 0;							// clear signature
		return true;
	}
	return false;
}



#endif /* DFU_CONFIG_H_ */