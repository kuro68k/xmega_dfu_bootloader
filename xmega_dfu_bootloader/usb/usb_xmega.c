/* usb_xmega.c
 *
 * Copyright 2011-2014 Nonolith Labs
 * Copyright 2014 Technical Machine
 * Copyright 2017-2018 Paul Qureshi
 *
 * Low level USB driver for XMEGA.
 */

#include <avr/io.h>

#include "usb.h"
#include "usb_xmega.h"
#include "usb_xmega_internal.h"
#include "xmega.h"


#define _USB_EP(epaddr) \
	USB_EP_pair_t* pair = &usb_xmega_endpoints[(epaddr & 0x3F)]; \
	USB_EP_t* e __attribute__ ((unused)) = &pair->ep[!!(epaddr&0x80)]; \


/**************************************************************************************************
* Initialize up USB after reset
*/
void usb_init()
{
	uint8_t saved_sreg = SREG;
	cli();
	USB.CAL0 = NVM_read_production_signature_byte(offsetof(NVM_PROD_SIGNATURES_t, USBCAL0));
	USB.CAL1 = NVM_read_production_signature_byte(offsetof(NVM_PROD_SIGNATURES_t, USBCAL1));
	USB.INTCTRLA = USB_BUSEVIE_bm | USB_INTLVL_MED_gc;
	USB.INTCTRLB = USB_TRNIE_bm | USB_SETUPIE_bm;
	SREG = saved_sreg;

	usb_reset();
}

/**************************************************************************************************
* Reset USB stack after device or USB reset
*/
void usb_reset()
{
	USB.EPPTR = (unsigned) usb_xmega_endpoints;
	USB.ADDR = 0;

	// endpoint 0 control IN/OUT
	usb_xmega_endpoints[0].out.STATUS = 0;
	usb_xmega_endpoints[0].out.CTRL = USB_EP_TYPE_CONTROL_gc | USB_EP_size_to_gc(USB_EP0_MAX_PACKET_SIZE);
	usb_xmega_endpoints[0].out.DATAPTR = (unsigned) ep0_buf_out;
	usb_xmega_endpoints[0].in.STATUS = USB_EP_BUSNACK0_bm;
	usb_xmega_endpoints[0].in.CTRL = USB_EP_TYPE_CONTROL_gc | USB_EP_MULTIPKT_bm | USB_EP_size_to_gc(USB_EP0_MAX_PACKET_SIZE);
	usb_xmega_endpoints[0].in.DATAPTR = (unsigned) ep0_buf_in;

#ifdef USB_HID
	usb_ep_enable(0x81, USB_EP_TYPE_BULK_gc, 64, false);
#endif

	USB.CTRLA = USB_ENABLE_bm | USB_SPEED_bm | usb_num_endpoints;
}

/**************************************************************************************************
* Enable an endpoint.
*
* type				USB_EP_TYPE_*_gc
* buffer_size		maximum payload size for endpoint
* enable_interrupt	enable transaction complete interrupt
*/
inline void usb_ep_enable(uint8_t ep, uint8_t type, usb_size buffer_size, bool enable_interrupt)
{
	_USB_EP(ep);
	e->STATUS = USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm;
	e->CTRL = type | USB_EP_size_to_gc(buffer_size) | (enable_interrupt ? 0 : USB_EP_INTDSBL_bm);
}

/**************************************************************************************************
* Disable an endpoint.
*/
inline void usb_ep_disable(uint8_t ep)
{
	_USB_EP(ep);
	e->CTRL = 0;
}

/**************************************************************************************************
* Reset endpoint, clearing all error flags and making ready for use.
*/
inline void usb_ep_reset(uint8_t ep)
{
	_USB_EP(ep);
	e->STATUS = USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm;
}

/**************************************************************************************************
* Start receiving data into buffer from host.
*/
inline void usb_ep_start_out(uint8_t ep, uint8_t* data, usb_size len)
{
	_USB_EP(ep);
	e->DATAPTR = (unsigned) data;
	LACR16(&(e->STATUS), USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm);
}

/**************************************************************************************************
* Start sending data from buffer to host
*/
void usb_ep_start_in(uint8_t ep, const uint8_t* data, usb_size size, bool zlp)
{
	_USB_EP(ep);
	e->DATAPTR = (unsigned) data;
	e->AUXDATA = 0;	// for multi-packet
	e->CNT = size | (zlp << 15);
	LACR16(&(e->STATUS), USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm);
}

/**************************************************************************************************
* Check if an endpoint is ready to start the next transaction
*/
inline bool usb_ep_is_ready(uint8_t ep)
{
	_USB_EP(ep);
	return !(e->STATUS & USB_EP_TRNCOMPL0_bm);
}

/**************************************************************************************************
* Check if an unhandled transaction has completed on an endpoint
*/
inline bool usb_ep_is_transaction_complete(uint8_t ep)
{
	_USB_EP(ep);
	return e->STATUS & USB_EP_TRNCOMPL0_bm;
}

/**************************************************************************************************
* Handle a completed transaction on an endpoint
*/
void usb_ep_clear_transaction_complete(uint8_t ep)
{
	_USB_EP(ep);
	LACR16(&(e->STATUS), USB_EP_TRNCOMPL0_bm | USB_EP_BUSNACK0_bm);
}

/**************************************************************************************************
* Get the number of bytes available from a completed transaction on an OUT endpoint
*/
inline uint16_t usb_ep_get_out_transaction_length(uint8_t ep)
{
	_USB_EP(ep);
	return e->CNT;
}

/**************************************************************************************************
* Physically detach from USB bus
*/
void usb_detach(void) {
	USB.CTRLB &= ~USB_ATTACH_bm;
}

/**************************************************************************************************
* Physically attach to USB bus
*/
void usb_attach(void) {
	USB.CTRLB |= USB_ATTACH_bm;
}

/**************************************************************************************************
* Clear SETUP OUT stage on the default control pipe
*/
void usb_ep0_clear_out_setup(void) {
	LACR16(&usb_xmega_endpoints[0].out.STATUS, USB_EP_SETUP_bm | USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm | USB_EP_OVF_bm | USB_EP_TOGGLE_bm);
}

/**************************************************************************************************
* Enable the OUT stage on the default control pipe
*/
void usb_ep0_out(void) {
	LACR16(&usb_xmega_endpoints[0].out.STATUS, USB_EP_SETUP_bm | USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm | USB_EP_OVF_bm);
}

/**************************************************************************************************
* Enable the IN stage on the default control pipe
*/
void usb_ep0_in(uint8_t size){
	usb_ep_start_in(0x80, ep0_buf_in, size, true);
}

/**************************************************************************************************
* Stall the default control pipe
*/
void usb_ep0_stall(void) {
	usb_xmega_endpoints[0].out.CTRL |= USB_EP_STALL_bm;
	usb_xmega_endpoints[0].in.CTRL  |= USB_EP_STALL_bm;
}

/**************************************************************************************************
* Set up the main CPU clock and USB clock
*/
void usb_configure_clock()
{
#ifdef USB_USE_PLL
	OSC.XOSCCTRL = OSC_FRQRANGE_12TO16_gc | OSC_XOSCSEL_XTAL_16KCLK_gc;
	OSC.CTRL |= OSC_XOSCEN_bm;
	while(!(OSC.STATUS & OSC_XOSCRDY_bm));

	OSC.PLLCTRL = OSC_PLLSRC_XOSC_gc | 3;		// 48MHz for USB
	OSC.CTRL |= OSC_PLLEN_bm;
	while(!(OSC.STATUS & OSC_PLLRDY_bm));

	CCPWrite(&CLK.PSCTRL, CLK_PSADIV_2_gc | CLK_PSBCDIV_1_1_gc);	// 24MHz CPU clock
	CCPWrite(&CLK.CTRL, CLK_SCLKSEL_PLL_gc);

	OSC.CTRL = OSC_XOSCEN_bm | OSC_PLLEN_bm;	// disable other clocks

	CLK.USBCTRL = CLK_USBPSDIV_1_gc | CLK_USBSRC_PLL_gc | CLK_USBSEN_bm;
#endif

#ifdef USB_USE_RC32
	// Configure DFLL for 48MHz, calibrated by USB SOF
	OSC.DFLLCTRL = OSC_RC32MCREF_USBSOF_gc;
	DFLLRC32M.CALB = NVM_read_production_signature_byte(offsetof(NVM_PROD_SIGNATURES_t, USBRCOSC));
	DFLLRC32M.COMP1 = 0x1B; //Xmega AU manual, 4.17.19
	DFLLRC32M.COMP2 = 0xB7;
	DFLLRC32M.CTRL = DFLL_ENABLE_bm;

	CCP = CCP_IOREG_gc; //Security Signature to modify clock
	OSC.CTRL = OSC_RC32MEN_bm | OSC_RC2MEN_bm; // enable internal 32MHz oscillator

	while(!(OSC.STATUS & OSC_RC32MRDY_bm)); // wait for oscillator ready

	OSC.PLLCTRL = OSC_PLLSRC_RC2M_gc | 16; // 2MHz * 16 = 32MHz

	CCP = CCP_IOREG_gc;
	OSC.CTRL = OSC_RC32MEN_bm | OSC_PLLEN_bm | OSC_RC2MEN_bm ; // Enable PLL

	while(!(OSC.STATUS & OSC_PLLRDY_bm)); // wait for PLL ready

	DFLLRC2M.CTRL = DFLL_ENABLE_bm;

	CCP = CCP_IOREG_gc; //Security Signature to modify clock
	CLK.CTRL = CLK_SCLKSEL_PLL_gc; // Select PLL
	CLK.PSCTRL = 0x00; // No peripheral clock prescaler

	CLK.USBCTRL = CLK_USBPSDIV_1_gc | CLK_USBSRC_RC32M_gc | CLK_USBSEN_bm;
#endif
}

/**************************************************************************************************
* Handle bus event interrupts
*/
ISR(USB_BUSEVENT_vect)
{
	if (USB.INTFLAGSACLR & (USB_CRCIF_bm | USB_UNFIF_bm | USB_OVFIF_bm))	// CRC error, under/overflow
		USB.INTFLAGSACLR = USB_CRCIF_bm | USB_UNFIF_bm | USB_OVFIF_bm;

	if (USB.INTFLAGSACLR & USB_STALLIF_bm)
		USB.INTFLAGSACLR = USB_STALLIF_bm;

	// USB bus reset signal
	if (USB.INTFLAGSASET & USB_RSTIF_bm)
	{
		USB.INTFLAGSACLR = USB_RSTIF_bm;
		usb_reset();
	}

	// start of frame, unused
	//if (USB.INTFLAGSACLR & USB_SOFIF_bm)
	//	USB.INTFLAGSACLR = USB_SOFIF_bm;

	USB.INTFLAGSACLR = USB_SUSPENDIF_bm | USB_RESUMEIF_bm;
}

/**************************************************************************************************
* Handle transaction complete interrupts. Uncomment callbacks if required.
*/
ISR(USB_TRNCOMPL_vect)
{
	USB.FIFOWP = 0;	// clear TCIF
	USB.INTFLAGSBCLR = USB_SETUPIF_bm | USB_TRNIF_bm;

	// EP0 (control) OUT/SETUP
	uint8_t status = usb_xmega_endpoints[0].out.STATUS;		// Read once to prevent race condition
	if (status & USB_EP_SETUP_bm)
	{
		memcpy(&usb_setup, ep0_buf_out, sizeof(usb_setup));
		LACR16(&(usb_xmega_endpoints[0].out.STATUS), USB_EP_TRNCOMPL0_bm | USB_EP_BUSNACK0_bm | USB_EP_SETUP_bm);
		if (((usb_setup.bmRequestType & 0x80) != 0) ||	// IN host requesting response
			(usb_setup.wLength == 0))					// OUT but no data
			usb_handle_control_setup();
		// else deferred until data stage complete
	}
	else if (status & USB_EP_TRNCOMPL0_bm)
	{
		//usb_handle_control_setup();
		usb_handle_control_out();
		LACR16(&(usb_xmega_endpoints[0].out.STATUS), USB_EP_TRNCOMPL0_bm);
	}

	// EP0 (control) IN
	if (usb_xmega_endpoints[0].in.STATUS & USB_EP_TRNCOMPL0_bm)
	{
		// SET_ADDRESS requests must only take effect after the response IN packet has
		// been sent.
		if ((usb_setup.bmRequestType & USB_REQTYPE_TYPE_MASK) == USB_REQTYPE_STANDARD)
		{
			if (usb_setup.bRequest == USB_REQ_SetAddress)
					USB.ADDR = usb_setup.wValue & 0x7F;
		}
		usb_handle_control_in();
		LACR16(&usb_xmega_endpoints[0].in.STATUS, USB_EP_TRNCOMPL0_bm);
	}

	// EP1 IN
	if (usb_xmega_endpoints[1].in.STATUS & USB_EP_TRNCOMPL0_bm)
	{
		LACR16(&usb_xmega_endpoints[1].in.STATUS, USB_EP_TRNCOMPL0_bm);
	}

	// empty callback
	//usb_cb_completion();
}
