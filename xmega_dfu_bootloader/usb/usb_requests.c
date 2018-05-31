/* usb_xmega.c
 *
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2014 Technical Machine
 * Copyright 2017-2018 Paul Qureshi
 *
 * Handle supported USB requests
 */

#include <avr/io.h>
#include "usb.h"
#include "usb_config.h"
#include "usb_xmega.h"
#include "hid.h"
#include "dfu.h"

USB_SetupPacket_t usb_setup;
__attribute__((__aligned__(2))) uint8_t ep0_buf_in[USB_EP0_BUFFER_SIZE];
__attribute__((__aligned__(2))) uint8_t ep0_buf_out[USB_EP0_BUFFER_SIZE];
volatile uint8_t usb_configuration;


extern uint16_t usb_handle_descriptor_request(uint8_t type, uint8_t index);
extern void handle_msft_compatible(void);


/**************************************************************************************************
* Handle standard setup requests
*/
void usb_handle_standard_setup_requests(void)
{
	switch (usb_setup.bRequest)
	{
		case USB_REQ_GetStatus:
			// Device:		D0	Self powered
			//				D1	Remote wake-up
			// Interface:	(all reserved)
			// Endpoint:	D0 endpoint halted
			ep0_buf_in[0] = 0;
			ep0_buf_in[1] = 0;
			usb_ep0_in(2);
			return usb_ep0_out();

		case USB_REQ_ClearFeature:
		case USB_REQ_SetFeature:
			// not implemented
			usb_ep0_in(0);
			return usb_ep0_out();

		case USB_REQ_SetAddress:
			// USB.ADDR must only change after the IN transaction has completed,
			// see USB_TRNCOMPL_vect vector
			usb_ep0_in(0);
			return usb_ep0_out();

		case USB_REQ_GetDescriptor:
		{
			uint8_t type = usb_setup.wValue >> 8;
			uint8_t index = usb_setup.wValue & 0xFF;
			uint16_t size = usb_handle_descriptor_request(type, index);

			if (size)
			{
				if (size > usb_setup.wLength)	// host requested partial descriptor
					size = usb_setup.wLength;

				return usb_ep_start_in(0x80, ep0_buf_in, size, true);
			}
			else
				return usb_ep0_stall();
		}

		case USB_REQ_GetConfiguration:
			ep0_buf_in[0] = usb_configuration;
			usb_ep0_in(1);
			return usb_ep0_out();

		case USB_REQ_SetConfiguration:
			if (usb_cb_set_configuration((uint8_t)usb_setup.wValue))
			{
				usb_ep0_in(0);
				usb_configuration = (uint8_t)(usb_setup.wValue);
				return usb_ep0_out();
			}
			return usb_ep0_stall();

		case USB_REQ_SetInterface:
			if (usb_handle_set_interface(usb_setup.wIndex, usb_setup.wValue))
			{
				usb_ep0_in(0);
				return usb_ep0_out();
			}
			return usb_ep0_stall();

		default:
			return usb_ep0_stall();
	}
}

/**************************************************************************************************
* DFU vendor requests
*/
#ifdef USB_DFU_RUNTIME
void dfu_control_setup(void)
{
	switch (usb_setup.bRequest)
	{
		case DFU_DETACH:
			dfu_cb_enter_dfu_mode();
			usb_ep0_in(0);
			return usb_ep0_out();

		// read status
		case DFU_GETSTATUS: {
			uint8_t len = usb_setup.wLength;
			if (len > sizeof(DFU_StatusResponse))
				len = sizeof(DFU_StatusResponse);
			DFU_StatusResponse *st = (DFU_StatusResponse *)ep0_buf_in;
			st->bStatus = DFU_STATUS_OK;
			st->bState = DFU_STATE_appIDLE;
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
			usb_ep0_in(0);
			return usb_ep0_out();

		// read state
		case DFU_GETSTATE:
			ep0_buf_in[0] = 0;
			usb_ep0_in(1);
			return usb_ep0_out();

		// unsupported requests
		default:
			return usb_ep0_stall();
	}
}
#endif

/**************************************************************************************************
* Handle class setup requests
*/
void usb_handle_class_setup_requests(void)
{
#if defined(USB_DFU_RUNTIME) || defined(USB_DFU_MODE)
	if (((usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK) == USB_RECIPIENT_INTERFACE) &&
		(usb_setup.wIndex == DFU_INTERFACE))
		return dfu_control_setup();
#endif

#ifdef USB_HID
	switch (usb_setup.bRequest)
	{
		// IN requests
		case USB_HIDREQ_GET_REPORT:
		{
			switch(usb_setup.wValue >> 8)
			{
				case USB_HID_REPORT_TYPE_INPUT:
				{
					int16_t size = hid_cb_get_report_input(ep0_buf_in, usb_setup.wIndex & 0xFF);
					if (size == -1)
						return usb_ep0_stall();
					usb_ep0_in(size);
					return usb_ep0_out();
				}
				case USB_HID_REPORT_TYPE_OUTPUT:
				{
					int16_t size = hid_cb_get_report_output(ep0_buf_in, usb_setup.wValue & 0xFF);
					if (size == -1)
						return usb_ep0_stall();
					usb_ep0_in(size);
					return usb_ep0_out();
				}
				case USB_HID_REPORT_TYPE_FEATURE:
				{
					int16_t size = hid_cb_get_report_feature(ep0_buf_in, usb_setup.wValue & 0xFF);
					if (size == -1)
						return usb_ep0_stall();
					usb_ep0_in(size);
					return usb_ep0_out();
				}
				default:
					return usb_ep0_stall();
			}
		}

		case USB_HIDREQ_GET_IDLE:
			return usb_ep0_stall();

		case USB_HIDREQ_GET_PROTOCOL:
			return usb_ep0_stall();

		// OUT requests
		case USB_HIDREQ_SET_REPORT:
		{
			switch(usb_setup.wValue >> 8)
			{
				case USB_HID_REPORT_TYPE_INPUT:
					if (hid_cb_set_report_input(ep0_buf_out, usb_setup.wLength, usb_setup.wValue & 0xFF))
					{
						usb_ep0_in(0);
						return usb_ep0_clear_out_setup();
					}
					return usb_ep0_stall();
				case USB_HID_REPORT_TYPE_OUTPUT:
					if (hid_cb_set_report_output(ep0_buf_out, usb_setup.wLength, usb_setup.wValue & 0xFF))
					{
						usb_ep0_in(0);
						return usb_ep0_clear_out_setup();
					}
					return usb_ep0_stall();
				case USB_HID_REPORT_TYPE_FEATURE:
					if (hid_cb_set_report_feature(ep0_buf_out, usb_setup.wLength, usb_setup.wValue & 0xFF))
					{
						usb_ep0_in(0);
						return usb_ep0_clear_out_setup();
					}
					return usb_ep0_stall();
				default:
					return usb_ep0_stall();
			}
		}

		case USB_HIDREQ_SET_IDLE:
			usb_ep0_in(0);
			return usb_ep0_out();

		case USB_HIDREQ_SET_PROTOCOL:
			return usb_ep0_stall();

		default:
			return usb_ep0_stall();
	}
#else
	return usb_ep0_stall();
#endif
}

/**************************************************************************************************
* Handle vendor setup requests
*/
void usb_handle_vendor_setup_requests(void)
{
	uint8_t recipient = usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK;
	if (recipient == USB_RECIPIENT_DEVICE)
	{
		switch(usb_setup.bRequest)
		{
#ifdef USB_WCID
			case WCID_REQUEST_ID:
				return handle_msft_compatible();
#endif
		}
	}
	else if (recipient == USB_RECIPIENT_INTERFACE)
	{
		if (usb_setup.wIndex == 0)
		{			// main interface
			switch(usb_setup.bRequest)
			{
#ifdef USB_WCID_EXTENDED
				case WCID_REQUEST_ID:
					return handle_msft_compatible();
#endif
			}
		}
	}

	return usb_ep0_stall();
}

/**************************************************************************************************
* Handle control SETUP requests
*/
void usb_handle_control_setup(void)
{
	switch (usb_setup.bmRequestType & USB_REQTYPE_TYPE_MASK)
	{
		case USB_REQTYPE_STANDARD:
			return usb_handle_standard_setup_requests();

		case USB_REQTYPE_CLASS:
			return usb_handle_class_setup_requests();

		case USB_REQTYPE_VENDOR:
		default:
			return usb_handle_vendor_setup_requests();
	}
}

/**************************************************************************************************
* Handle control OUT requests
*/
void usb_handle_control_out(void)
{
}

/**************************************************************************************************
* Handle control IN requests
*/
void usb_handle_control_in(void)
{
	if (((usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK) == USB_RECIPIENT_INTERFACE) &&
		(usb_setup.wIndex == DFU_INTERFACE))
		return dfu_control_in_completion();
}

/**************************************************************************************************
* Handle set interface requests
*/
bool usb_handle_set_interface(uint16_t interface, uint16_t altsetting)
{
	if (interface == DFU_INTERFACE)
	{
		if (altsetting < 2)
		{
			dfu_set_alternative(altsetting);
			return true;
		}
	}
	return false;
}
