/*
 * dfu.c
 *
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "sp_driver.h"
#include "eeprom.h"
#include "usb.h"
#include "usb_xmega.h"
#include "dfu.h"
#include "dfu_config.h"

extern volatile bool reset_flag;

uint8_t	state = DFU_STATE_dfuIDLE;
uint8_t	status = DFU_STATUS_OK;
uint16_t write_head = 0;
uint8_t write_buffer[APP_SECTION_PAGE_SIZE];
uint8_t alternative = 0;
uint16_t max_page = APP_SECTION_SIZE/APP_SECTION_PAGE_SIZE;

#ifdef DELAYED_ZERO_PAGE
	uint8_t zero_buffer[APP_SECTION_PAGE_SIZE];
#endif


/**************************************************************************************************
* Write buffer to flash/EEPROM
*/
void dfu_write_buffer(uint16_t page)
{
	if (alternative == 0)	// flash
	{
		SP_WaitForSPM();
		SP_LoadFlashPage(write_buffer);
		SP_WriteApplicationPage(APP_SECTION_START + ((uint32_t)page * APP_SECTION_PAGE_SIZE));
	}
	else					// EEPROM
	{
		EEP_DisableMapping();
		uint8_t *ptr = write_buffer;
		for (uint8_t i = 0; i < (APP_SECTION_PAGE_SIZE / EEPROM_PAGE_SIZE); i++)
		{
//			EEP_LoadPageBuffer(ptr, EEPROM_PAGE_SIZE);
			EEP_WaitForNVM();
			NVM.CMD = NVM_CMD_LOAD_EEPROM_BUFFER_gc;
			NVM.ADDR1 = 0x00;
			NVM.ADDR2 = 0x00;
			for (uint8_t i = 0; i < EEPROM_PAGE_SIZE; ++i) {
				NVM.ADDR0 = i;
				NVM.DATA0 = *ptr++;
			}
//			EEP_AtomicWritePage(page + i);
			uint16_t address = (((page * (APP_SECTION_PAGE_SIZE/EEPROM_PAGE_SIZE)) + i) * EEPROM_PAGE_SIZE);
			NVM.ADDR0 = address & 0xFF;
			NVM.ADDR1 = (address >> 8) & 0x1F;
			NVM.ADDR2 = 0x00;
			NVM.CMD = NVM_CMD_ERASE_WRITE_EEPROM_PAGE_gc;
			NVM_EXEC();
		}
	}
	memset(write_buffer, 0xFF, sizeof(write_buffer));
}

/**************************************************************************************************
* Handle errors with DFU protocol
*/
void dfu_error(uint8_t error_status)
{
	status = error_status;
	state = DFU_STATE_dfuERROR;
}

/**************************************************************************************************
* Reset DFU state machine
*/
void dfu_reset(void)
{
	state = DFU_STATE_dfuIDLE;
	status = DFU_STATUS_OK;
	write_head = 0;
}

/**************************************************************************************************
* Handle set USB interface request
*/
void dfu_set_alternative(uint8_t alt)
{
	alternative = alt;
	switch (alternative)
	{
		case 0:	// flash
			max_page = APP_SECTION_SIZE/APP_SECTION_PAGE_SIZE;
			break;
		case 1: // EEPROM
			max_page = EEPROM_SIZE/APP_SECTION_PAGE_SIZE;
			break;
	}
	dfu_reset();
}

/**************************************************************************************************
* Handle DFU commands
*/
void dfu_control_setup(void)
{
#ifdef UPLOAD_SUPPORT
	static uint32_t read_head = 0;
#endif

	switch (usb_setup.bRequest)
	{
		// write block
		case DFU_DNLOAD:
			if ((state == DFU_STATE_dfuIDLE) || (state == DFU_STATE_dfuDNLOAD_IDLE))
			{
				// enter manifest mode?
				if (usb_setup.wLength == 0) {
					state = DFU_STATE_dfuMANIFEST_SYNC;
					usb_ep0_out();
					usb_ep0_in(0);
					return;
				}

				// else start next page
				write_head = 0;
				if (usb_setup.wLength > APP_SECTION_PAGE_SIZE) {
					dfu_error(DFU_STATUS_errUNKNOWN);
					return;
				}
				if (usb_setup.wValue > max_page) {
					dfu_error(DFU_STATUS_errADDRESS);
					return;
				}
				if (alternative == 0)
					SP_EraseApplicationPage(APP_SECTION_START + ((uint32_t)usb_setup.wValue * APP_SECTION_PAGE_SIZE));
				state = DFU_STATE_dfuDNBUSY;
				return usb_ep0_out();
			}
			else
				dfu_error(DFU_STATUS_errSTALLEDPKT);

			return usb_ep0_stall();

		// read memory
#ifdef UPLOAD_SUPPORT
		case DFU_UPLOAD:
			if (usb_setup.wValue >= max_page)
				return usb_ep0_in(0);	// end of firmware image
			if (usb_setup.wLength > sizeof(write_buffer))
			{
				dfu_error(DFU_STATUS_errNOTDONE);
				return;
			}

			if (alternative == 0)
				memcpy_PF(write_buffer, read_head, usb_setup.wLength);
			else {
				EEP_EnableMapping();
				memcpy(write_buffer, (void *)(MAPPED_EEPROM_START + (uint16_t)read_head), usb_setup.wLength);
			}
			read_head += usb_setup.wLength;
			state = DFU_STATE_dfuUPLOAD_IDLE;
			usb_ep_start_in(0x80, write_buffer, usb_setup.wLength, false);
			return;
#endif

		// read status
		case DFU_GETSTATUS: {
			if (state == DFU_STATE_dfuMANIFEST_SYNC) {
				state = DFU_STATE_dfuMANIFEST_WAIT_RST;
				reset_flag = true;
#ifdef DELAYED_ZERO_PAGE
				memcpy(write_buffer, zero_buffer, sizeof(write_buffer));
				dfu_write_buffer(0);
#endif
			}

			uint8_t len = usb_setup.wLength;
			if (len > sizeof(DFU_StatusResponse))
				len = sizeof(DFU_StatusResponse);
			DFU_StatusResponse *st = (DFU_StatusResponse *)ep0_buf_in;
			st->bStatus = status;
			st->bState = state;
			st->bwPollTimeout[0] = 0;
			st->bwPollTimeout[1] = 0;
			st->bwPollTimeout[2] = 0;
			st->iString = 0;
			usb_ep0_in(len);
			return usb_ep0_out();
		}

		// abort, clear status
		case DFU_ABORT:
		case DFU_CLRSTATUS:
			dfu_reset();
			usb_ep0_in(0);
			return usb_ep0_out();

		// read state
		case DFU_GETSTATE:
			ep0_buf_in[0] = state;
			usb_ep0_in(1);
			return usb_ep0_out();

		case DFU_DETACH:
			reset_flag = true;
			usb_ep0_in(0);
			return usb_ep0_out();

		// unsupported requests
		default:
			dfu_error(DFU_STATUS_errSTALLEDPKT);
			return usb_ep0_stall();
	}
}

/**************************************************************************************************
* Handle control endpoint OUT requests
*/
void dfu_control_out_completion(void)
{
	switch(usb_setup.bRequest) {
		case DFU_DNLOAD: {
			uint16_t len = usb_ep_get_out_transaction_length(0);
			while (len > 0)
			{
				if (write_head >= sizeof(write_buffer)) {
					dfu_error(DFU_STATUS_errADDRESS);
					break;
				}

				uint16_t maxlen = sizeof(write_buffer) - write_head;
				if (maxlen > len)
					maxlen = len;
				memcpy(&write_buffer[write_head], ep0_buf_out, maxlen);
				write_head += maxlen;

				len -= maxlen;
			}

			if (write_head >= usb_setup.wLength)
			{
#ifdef DELAYED_ZERO_PAGE
				if (usb_setup.wValue == 0)	// zero page
					memcpy(zero_buffer, write_buffer, sizeof(zero_buffer));
				else
#endif
				dfu_write_buffer(usb_setup.wValue);

				write_head = 0;
				state = DFU_STATE_dfuDNLOAD_IDLE;
				usb_ep0_in(0);
			}
			else
				usb_ep0_out();
		}
	}
}

/**************************************************************************************************
* Handle control endpoint IN requests
*/
void dfu_control_in_completion(void)
{
	if (state == DFU_STATE_dfuUPLOAD_IDLE)
	{
		// stay in UPLOAD_IDLE state as we are expecting further UPLOAD commands
		usb_ep0_out();
	}
}
