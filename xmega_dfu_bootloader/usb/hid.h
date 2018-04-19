/* hid.h
 *
 * Copyright 2018 Paul Qureshi
 *
 * Human Interface Device support
 */

#ifndef HID_H_
#define HID_H_


extern uint8_t hid_report[USB_HID_REPORT_SIZE];


extern void hid_send_report(void);


#endif /* HID_H_ */