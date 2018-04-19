/*
 * xmega.h
 *
 */


#ifndef XMEGA_H_
#define XMEGA_H_


#define NOP()	__asm__ __volatile__("nop")
#define	WDR()	__asm__ __volatile__("wdr")

// port settings
#define DISABLE_DIGITAL_INPUT(A)	A = (A & ~PORT_ISC_gm) | PORT_ISC_INPUT_DISABLE_gc
#define ENABLE_PULLUP(A)			A = (A & ~PORT_OPC_gm) | PORT_OPC_PULLUP_gc
#define ENABLE_PULLDOWN(A)			A = (A & ~PORT_OPC_gm) | PORT_OPC_PULLDOWN_gc
#define ENABLE_TOTEM(A)				A = (A & ~PORT_OPC_gm) | PORT_OPC_TOTEM_gc

#define NVM_wait_not_busy()			do {} while ((NVM.STATUS & NVM_NVMBUSY_bm) == NVM_NVMBUSY_bm);

// Watchdog timer
#define WATCHDOG_ON					do {	WDR();																	\
											CCPWrite(&WDT.CTRL, WDT_PER_8KCLK_gc | WDT_ENABLE_bm | WDT_CEN_bm);	\
									} while(0);
#define WATCHDOG_OFF				do {	while (WDT.STATUS & WDT_SYNCBUSY_bm);									\
											CCPWrite(&WDT.CTRL, WDT_CEN_bm);										\
									} while(0);


extern void		CCPWrite(volatile uint8_t *address, uint8_t value);
extern uint8_t	NVM_read_fuse_byte(uint8_t index);
extern uint8_t	NVM_read_production_signature_byte(uint8_t index);
extern uint8_t	NVM_read_user_signature_byte(uint16_t index);
extern uint32_t	NVM_application_crc(void);
extern uint32_t	NVM_boot_crc(void);


#endif /* XMEGA_H_ */
