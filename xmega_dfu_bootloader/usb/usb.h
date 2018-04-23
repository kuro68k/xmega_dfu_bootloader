/* usb.h
 *
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2014 Technical Machine
 * Copyright 2018 Paul Qureshi
 *
 * Single include for application code
 */

#ifndef USB_H_
#define USB_H_


#include <stdbool.h>
#include <string.h>

#define USB_EP0_MAX_PACKET_SIZE		64
#define USB_EP0_BUFFER_SIZE			64

#include "usb_standard.h"
#include "usb_config.h"

extern USB_SetupPacket_t usb_setup;
extern uint8_t ep0_buf_in[USB_EP0_BUFFER_SIZE];
extern uint8_t ep0_buf_out[USB_EP0_BUFFER_SIZE];
extern volatile uint8_t USB_DeviceState;
extern volatile uint8_t USB_Device_ConfigurationNumber;

typedef size_t usb_size;
typedef uint8_t usb_ep;
typedef uint8_t usb_bank;

/// Configure the XMEGA's clock for use with USB.
void usb_configure_clock(void);

/// Callback for a SET_CONFIGURATION request
bool usb_cb_set_configuration(uint8_t config);

/// Initialize the USB controller
void usb_init(void);

/// Configure pull resistor to be detected by the host
void usb_attach(void);

/// Disconnect from the host
void usb_detach(void);

/// Called internally on USB reset
void usb_reset(void);

/// Configure and enable an endpoint
void usb_ep_enable(usb_ep ep, uint8_t type, usb_size bufsize, bool enable_interrupt);

/// Disable an endpoint
void usb_ep_disable(usb_ep ep);

/// Reset an endpoint, clearing pending transfers
void usb_ep_reset(usb_ep ep);

/// Set or clear stall on an endpoint
void usb_ep_set_stall(usb_ep ep);
void usb_ep_clr_stall(usb_ep ep);

/// Returns true if an endpoint can start or queue a transfer
bool usb_ep_is_ready(usb_ep ep);

/// Returns true if there is a completion pending on the endpoint
bool usb_ep_is_transaction_complete(usb_ep ep);

/// Clear a completion on an endpoint
void usb_ep_clear_transaction_complete(usb_ep ep);

/// Start an asynchronous host->device transfer.
/// The data will be received into data up to size len. This buffer must remain valid until
/// the callback is called
void usb_ep_start_out(usb_ep ep, uint8_t* data, usb_size len);

/// Gets the length of a pending completion on an OUT endpoint
usb_size usb_ep_get_out_transaction_length(usb_ep ep);

/// Start an asynchronous device->host transfer.
/// The data will be sent from the data buffer. This buffer must remain valid until the
/// the callback is called. If zlp is set and the data is not a multiple of the packet
/// size, an extra zero-length packet will be sent to terminate the transfer.
void usb_ep_start_in(uint8_t ep, const uint8_t* data, usb_size size, bool zlp);


#endif	// USB_H_
