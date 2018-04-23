/* usb_xmega.h
 *
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2014 Technical Machine
 * Copyright 2018 Paul Qureshi
 *
 * XMEGA specific low level implementation
 */

#ifndef USB_XMEGA_H_
#define USB_XMEGA_H_


#include "usb_xmega_internal.h"

typedef union USB_EP_pair{
	union{
		struct{
			USB_EP_t out;
			USB_EP_t in;
		};
		USB_EP_t ep[2];
	};
} __attribute__((packed)) USB_EP_pair_t;

//extern USB_EP_pair_t *usb_xmega_endpoints;	// for FIFO mode
extern USB_EP_pair_t usb_xmega_endpoints[];
extern const uint8_t usb_num_endpoints;

/* FIFO mode
#define USB_ENDPOINTS(NUM_EP) \
	const uint8_t usb_num_endpoints = (NUM_EP); \
	struct { \
		uint8_t fifo_buffer[((NUM_EP)+1)*4]; \
		USB_EP_pair_t usb_xmega_endpoints[(NUM_EP)+1]; \
	} epptr_ram __attribute__((aligned(2))); \
	USB_EP_pair_t *usb_xmega_endpoints = epptr_ram.usb_xmega_endpoints;
*/

#define USB_ENDPOINTS(NUM_EP) \
	const uint8_t usb_num_endpoints = (NUM_EP); \
	USB_EP_pair_t usb_xmega_endpoints[(NUM_EP)+1] __attribute__((aligned(2)));


/// Copy data from program memory to the ep0 IN buffer
const uint8_t* usb_ep0_from_progmem(const uint8_t* addr, uint16_t size);

/// Send size bytes from ep0_buf_in on endpoint 0
void usb_ep0_in(uint8_t size);

/// Clear out setup packet
void usb_ep0_clear_out_setup(void);

/// Accept a packet into ep0_buf_out on endpoint 0
void usb_ep0_out(void);

/// Stall endpoint 0
void usb_ep0_stall(void);

/// Internal common methods called by the hardware API
void usb_handle_control_setup(void);
void usb_handle_control_out(void);
void usb_handle_control_in(void);
bool usb_handle_set_interface(uint16_t interface, uint16_t altsetting);


#endif // USB_XMEGA_H_
