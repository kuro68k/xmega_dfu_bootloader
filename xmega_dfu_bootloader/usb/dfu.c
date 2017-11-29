/*
 * dfu.c
 *
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "sp_driver.h"
#include "eeprom.h"
#include "usb.h"
#include "dfu.h"
#include "dfu_config.h"

extern bool reset_flag;

uint8_t	state = DFU_STATE_dfuIDLE;
uint8_t	status = DFU_STATUS_OK;
uint16_t write_head = 0;
uint8_t write_buffer[APP_SECTION_PAGE_SIZE];
uint8_t alternative = 0;

#ifdef DELAYED_ZERO_PAGE
	uint8_t zero_buffer[APP_SECTION_PAGE_SIZE];
#endif


void dfu_write_buffer(uint16_t page)
{
	if (alternative == 0)	// flash
	{
		SP_WaitForSPM();
		SP_LoadFlashPage(write_buffer);
		SP_WriteApplicationPage(APP_SECTION_START + ((uint32_t)page * APP_SECTION_PAGE_SIZE));
		memset(write_buffer, 0xFF, sizeof(write_buffer));
	}
	else					// EEPROM
	{
		uint8_t eeprom_pages_per_buffer = APP_SECTION_PAGE_SIZE / EEPROM_PAGE_SIZE;
		for (uint8_t i = 0; i < eeprom_pages_per_buffer; i++)
		{
			EEP_WaitForNVM();
			EEP_LoadPageBuffer(&write_buffer[i * EEPROM_PAGE_SIZE], EEPROM_PAGE_SIZE);
			EEP_AtomicWritePage((page * eeprom_pages_per_buffer) + i);
			memset(write_buffer, 0xFF, sizeof(write_buffer));
		}
	}
}

void dfu_error(uint8_t error_status)
{
	status = error_status;
	state = DFU_STATE_dfuERROR;
}

void dfu_reset(void)
{
	state = DFU_STATE_dfuIDLE;
	status = DFU_STATUS_OK;
	write_head = 0;
}

void dfu_set_alternative(uint8_t alt)
{
	alternative = alt;
	dfu_reset();
}

void dfu_control_setup(void)
{
	static uint32_t read_head = 0;

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
					return usb_ep0_in(0);
				}

				// else start next page by erasing it
				write_head = 0;
				if (usb_setup.wLength > APP_SECTION_PAGE_SIZE) {
					dfu_error(DFU_STATUS_errUNKNOWN);
					return;
				}
				if (usb_setup.wValue > (APP_SECTION_SIZE/APP_SECTION_PAGE_SIZE)) {
					dfu_error(DFU_STATUS_errADDRESS);
					return;
				}
				SP_EraseApplicationPage(APP_SECTION_START + ((uint32_t)usb_setup.wValue * APP_SECTION_PAGE_SIZE));
				state = DFU_STATE_dfuDNBUSY;
				return usb_ep0_out();
			}
			else
				dfu_error(DFU_STATUS_errSTALLEDPKT);

			return usb_ep0_stall();

		// read memory
		case DFU_UPLOAD:
			if (usb_setup.wValue >= (APP_SECTION_SIZE / APP_SECTION_PAGE_SIZE))
				return usb_ep0_in(0);	// end of firmware image
			if (usb_setup.wLength > sizeof(write_buffer))
			{
				dfu_error(DFU_STATUS_errNOTDONE);
				return;
			}

			//memcpy_PF(write_buffer, (uint32_t)(APP_SECTION_START + (usb_setup.wValue * APP_SECTION_PAGE_SIZE)), usb_setup.wLength);
			memcpy_PF(write_buffer, read_head, usb_setup.wLength);
			read_head += usb_setup.wLength;
			state = DFU_STATE_dfuUPLOAD_IDLE;
			usb_ep_start_in(0x80, write_buffer, usb_setup.wLength, false);
			return;

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

		// unsupported requests
		default:
			dfu_error(DFU_STATUS_errSTALLEDPKT);
			return usb_ep0_stall();
	}
}

void dfu_control_out_completion(void)
{
	switch(usb_setup.bRequest) {
		case DFU_DNLOAD: {
			uint16_t len = usb_ep_out_length(0);
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

void dfu_control_in_completion(void)
{
	if (state == DFU_STATE_dfuUPLOAD_IDLE)
	{
		// stay in UPLOAD_IDLE state as we are expecting further UPLOAD commands
		usb_ep0_out();
	}
}
