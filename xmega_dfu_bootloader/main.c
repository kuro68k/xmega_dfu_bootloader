/*
 * main.c
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usb_xmega.h"
#include "dfu_config.h"

typedef void (*AppPtr)(void) __attribute__ ((noreturn));

extern void CCPWrite(volatile uint8_t *address, uint8_t value);

bool reset_flag = false;

int main(void)
{
	if (!CheckStartConditions())
	{
		// exit bootloader
		AppPtr application_vector = (AppPtr)0x000000;
		CCP = CCP_IOREG_gc;		// unlock IVSEL
		PMIC.CTRL = 0;			// disable interrupts, set vector table to app section
		EIND = 0;				// indirect jumps go to lower 128k of app section
		RAMPZ = 0;				// LPM uses lower 64k of flash
		application_vector();
	}

	CCPWrite(&PMIC.CTRL, PMIC_IVSEL_bm);

	usb_configure_clock();
	USB.INTCTRLA = USB_BUSEVIE_bm | USB_INTLVL_MED_gc;
	USB.INTCTRLB = USB_TRNIE_bm | USB_SETUPIE_bm;
	usb_init();
	USB.CTRLA |= USB_FIFOEN_bm;

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	usb_attach();
	//usb_ep_enable(0, USB_EP_TYPE_BULK, APP_SECTION_PAGE_SIZE);
	//usb_ep_start_out(0, ep0_buf_out, APP_SECTION_PAGE_SIZE);
	while (!reset_flag);
	_delay_ms(25);
	usb_detach();
	_delay_ms(100);
	for(;;)
		CCPWrite(&RST.CTRL, RST_SWRST_bm);
}
