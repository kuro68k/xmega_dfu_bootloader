#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stddef.h>
#include "sp_driver.h"
#include "usb_xmega.h"
#include "dfu.h"
#include "dfu_config.h"

// Notes:
// Fill in VID/PID in device_descriptor
// Fill in msft_extended for WCID
// WCID request ID can be changed below
// Other options in usb.h

USB_ENDPOINTS(1);
#define DFU_INTERFACE			0

#define WCID_REQUEST_ID			0x22
#define WCID_REQUEST_ID_STR		u"\x22"


const USB_DeviceDescriptor PROGMEM device_descriptor = {
	.bLength = sizeof(USB_DeviceDescriptor),
	.bDescriptorType = USB_DTYPE_Device,

	.bcdUSB                 = 0x0200,
	.bDeviceClass           = USB_CSCP_NoDeviceClass,
	.bDeviceSubClass        = USB_CSCP_NoDeviceSubclass,
	.bDeviceProtocol        = USB_CSCP_NoDeviceProtocol,

	.bMaxPacketSize0        = 64,
	.idVendor               = 0x9999,
	.idProduct              = 0x1000,
	.bcdDevice              = 0x0001,

	.iManufacturer          = 0x01,
	.iProduct               = 0x02,
#ifdef USB_SERIAL_NUMBER
	.iSerialNumber          = 0x03,
#else
	.iSerialNumber          = 0x00,
#endif

	.bNumConfigurations     = 1
};

typedef struct ConfigDesc {
	USB_ConfigurationDescriptor Config;
	USB_InterfaceDescriptor DFU_intf_flash;
	DFU_FunctionalDescriptor DFU_desc_flash;
	USB_InterfaceDescriptor DFU_intf_eeprom;
	DFU_FunctionalDescriptor DFU_desc_eeprom;
} ConfigDesc;

const __flash ConfigDesc configuration_descriptor = {
	.Config = {
		.bLength = sizeof(USB_ConfigurationDescriptor),
		.bDescriptorType = USB_DTYPE_Configuration,
		.wTotalLength  = sizeof(ConfigDesc),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = USB_CONFIG_ATTR_BUSPOWERED,
		.bMaxPower = USB_CONFIG_POWER_MA(100)
	},
	.DFU_intf_flash = {
		.bLength = sizeof(USB_InterfaceDescriptor),
		.bDescriptorType = USB_DTYPE_Interface,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0,
		.bInterfaceClass = DFU_INTERFACE_CLASS,
		.bInterfaceSubClass = DFU_INTERFACE_SUBCLASS,
		.bInterfaceProtocol = DFU_INTERFACE_PROTOCOL_DFUMODE,
		.iInterface = 0x10
	},
	.DFU_desc_flash = {
		.bLength = sizeof(DFU_FunctionalDescriptor),
		.bDescriptorType = DFU_DESCRIPTOR_TYPE,
		.bmAttributes = (DFU_ATTR_CANDOWNLOAD_bm | DFU_ATTR_WILLDETACH_bm),
		.wDetachTimeout = 0,
		.wTransferSize = APP_SECTION_PAGE_SIZE,
		.bcdDFUVersion = 0x0101
	},
	.DFU_intf_eeprom = {
		.bLength = sizeof(USB_InterfaceDescriptor),
		.bDescriptorType = USB_DTYPE_Interface,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 1,
		.bNumEndpoints = 0,
		.bInterfaceClass = DFU_INTERFACE_CLASS,
		.bInterfaceSubClass = DFU_INTERFACE_SUBCLASS,
		.bInterfaceProtocol = DFU_INTERFACE_PROTOCOL_DFUMODE,
		.iInterface = 0x11
	},
	.DFU_desc_eeprom = {
		.bLength = sizeof(DFU_FunctionalDescriptor),
		.bDescriptorType = DFU_DESCRIPTOR_TYPE,
		.bmAttributes = (DFU_ATTR_CANDOWNLOAD_bm | DFU_ATTR_WILLDETACH_bm),
		.wDetachTimeout = 0,
		.wTransferSize = APP_SECTION_PAGE_SIZE,
		.bcdDFUVersion = 0x0101
	},
};


/* Strings used by descriptors
*/
const __flash USB_StringDescriptor language_string = {
	.bLength = USB_STRING_LEN(1),
	.bDescriptorType = USB_DTYPE_String,
	.bString = {USB_LANGUAGE_EN_US},
};

const __flash USB_StringDescriptor manufacturer_string = {
	.bLength = USB_STRING_LEN(4),
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"KEIO"
};

const __flash USB_StringDescriptor product_string = {
	.bLength = USB_STRING_LEN(20),
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"XMEGA DFU Bootloader"
};

const __flash USB_StringDescriptor dfu_flash_string = {
	.bLength = USB_STRING_LEN(5),
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"Flash"
};

const __flash USB_StringDescriptor dfu_eeprom_string = {
	.bLength = USB_STRING_LEN(6),
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"EEPROM"
};


#ifdef USB_SERIAL_NUMBER
USB_StringDescriptor serial_string = {
	.bLength = USB_STRING_LEN(22),
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"0000000000000000000000"
};

void byte2char16(uint8_t byte, __CHAR16_TYPE__ *c)
{
	*c++ = 'A' + (byte >> 4);
	*c = 'A' + (byte & 0xF);
}
void generate_serial(void)
{
	__CHAR16_TYPE__ *c = (__CHAR16_TYPE__ *)&serial_string.bString;
	//uint8_t temp;
	uint8_t idx = offsetof(NVM_PROD_SIGNATURES_t, LOTNUM0);
	for (uint8_t i = 0; i < 6; i++)
	{
		byte2char16(SP_ReadCalibrationByte(idx++), c);
		c += 2;
	}
	byte2char16(SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, WAFNUM)), c);
	c += 2;
	idx = offsetof(NVM_PROD_SIGNATURES_t, COORDX0);
	for (uint8_t i = 0; i < 4; i++)
	{
		byte2char16(SP_ReadCalibrationByte(idx++), c);
		c += 2;
	}
}
#endif


/* Microsoft WCID descriptor
*/
const __flash USB_StringDescriptor msft_string = {
	.bLength = 18,
	.bDescriptorType = USB_DTYPE_String,
	.bString = u"MSFT100" WCID_REQUEST_ID_STR
};

__attribute__((__aligned__(4))) const USB_MicrosoftCompatibleDescriptor msft_compatible = {
	.dwLength = sizeof(USB_MicrosoftCompatibleDescriptor) +
				1*sizeof(USB_MicrosoftCompatibleDescriptor_Interface),
	.bcdVersion = 0x0100,
	.wIndex = 0x0004,
	.bCount = 1,
	.reserved = {0, 0, 0, 0, 0, 0, 0},
	.interfaces = {
		{
			.bFirstInterfaceNumber = 0,
			.reserved1 = 0x01,
			.compatibleID = "WINUSB\0\0",
			.subCompatibleID = {0, 0, 0, 0, 0, 0, 0, 0},
			.reserved2 = {0, 0, 0, 0, 0, 0},
		},
	}
};



uint16_t usb_cb_get_descriptor(uint8_t type, uint8_t index, const uint8_t** ptr) {
	const void* address = NULL;
	uint16_t size    = 0;

	switch (type) {
		case USB_DTYPE_Device:
			address = &device_descriptor;
			size    = sizeof(USB_DeviceDescriptor);
			break;
		case USB_DTYPE_Configuration:
			address = &configuration_descriptor;
			size    = sizeof(ConfigDesc);
			break;
		case USB_DTYPE_String:
			switch (index) {
				case 0x00:
					address = &language_string;
					break;
				case 0x01:
					address = &manufacturer_string;
					break;
				case 0x02:
					address = &product_string;
					break;
#ifdef USB_SERIAL_NUMBER
				case 0x03:
					generate_serial();
					*ptr = (uint8_t *)&serial_string;
					return serial_string.bLength;
#endif
				case 0x10:
					address = &dfu_flash_string;
					break;
				case 0x11:
					address = &dfu_eeprom_string;
					break;
				case 0xEE:
					address = &msft_string;
					break;
			}
			size = pgm_read_byte_far((uint16_t)(&((USB_StringDescriptor*)address)->bLength) | BOOTLDR_ADDR);
			break;
	}

	*ptr = usb_ep0_from_progmem(address, size);
	return size;
}

void usb_cb_reset(void) {
}

bool usb_cb_set_configuration(uint8_t config) {
	if (config < 2)
		return true;
	return false;
}

void handle_msft_compatible(void) {
	const uint8_t *data;
	uint16_t len;
	if (usb_setup.wIndex == 0x0004) {
		len = msft_compatible.dwLength;
		data = (const uint8_t *)&msft_compatible;
	} else {
		return usb_ep0_stall();
	}
	if (len > usb_setup.wLength) {
		len = usb_setup.wLength;
	}
	usb_ep_start_in(0x80, data, len, true);
	usb_ep0_out();
}

void usb_cb_control_setup(void) {
	uint8_t recipient = usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK;
	if (recipient == USB_RECIPIENT_DEVICE) {
		if (usb_setup.bRequest == WCID_REQUEST_ID)
			return handle_msft_compatible();
	} else if (recipient == USB_RECIPIENT_INTERFACE) {
		if (usb_setup.wIndex == DFU_INTERFACE)
			return dfu_control_setup();
	}

	return usb_ep0_stall();
}

void usb_cb_completion(void) {
}

void usb_cb_control_in_completion(void) {
	uint8_t recipient = usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK;
	if (recipient == USB_RECIPIENT_INTERFACE) {
		if (usb_setup.wIndex == DFU_INTERFACE) {
			dfu_control_in_completion();
		}
	}
}

void usb_cb_control_out_completion(void) {
	uint8_t recipient = usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK;
	if (recipient == USB_RECIPIENT_INTERFACE) {
		if (usb_setup.wIndex == DFU_INTERFACE) {
			dfu_control_out_completion();
		}
	}
}

bool usb_cb_set_interface(uint16_t interface, uint16_t altsetting) {
	if (interface == DFU_INTERFACE) {
		if (altsetting < 2) {
			dfu_set_alternative(altsetting);
			return true;
		}
	}
	return false;
}
