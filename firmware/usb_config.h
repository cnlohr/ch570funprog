#ifndef _USB_CONFIG_H
#define _USB_CONFIG_H

#include "funconfig.h"
#include "ch32fun.h"

#define FUSB_BUFFERS_NUMBER   5 // Number of EP buffers (one for EP0, one per each IN/OUT, two for double)
#define FUSB_EP1_MODE         USBFS_EP_MODE_TX // IN
#define FUSB_EP2_MODE         USBFS_EP_MODE_RX // OUT
#define FUSB_EP3_MODE         USBFS_EP_MODE_TX // IN
#define FUSB_EP5_MODE         USBFS_EP_MODE_TX // IN

#define USB_EP_TX             5

#define FUSB_HID_INTERFACES   1
#define FUSB_CURSED_TURBO_DMA 0 // Hacky, but seems fine, shaves 2.5us off filling 64-byte buffers.
#define FUSB_HID_USER_REPORTS 1
#define FUSB_IO_PROFILE       0
#define FUSB_USE_HPE          FUNCONF_ENABLE_HPE
#define FUSB_USER_HANDLERS    1
#define FUSB_USE_DMA7_COPY    0
#define FUSB_VDD_5V           FUNCONF_USE_5V_VDD
#define FUSB_FROM_RAM         1

#include "usb_defines.h"

#define FUSB_USB_VID 0x1209
#define FUSB_USB_PID 0x570f
#define FUSB_USB_REV 0x0007
#define FUSB_STR_MANUFACTURER u"ch32fun"
#define FUSB_STR_PRODUCT      u"ch570 funprog"
#define FUSB_STR_SERIAL       u"007"

//Taken from http://www.usbmadesimple.co.uk/ums_ms_desc_dev.htm
static const uint8_t device_descriptor[] = {
	18, //bLength - Length of this descriptor
	1,  //bDescriptorType - Type (Device)
	0x01, 0x01, //bcdUSB - The highest USB spec version this device supports (USB1.1)
	0x03, //bDeviceClass - Device Class
	0x00, //bDeviceSubClass - Device Subclass (Subclass = 0 = "None")
	0x00, //bDeviceProtocol - Device Protocol  (00 = use config descriptor)
	64, //bMaxPacketSize - Max packet size for EP0
	(uint8_t)(FUSB_USB_VID), (uint8_t)(FUSB_USB_VID >> 8), //idVendor - ID Vendor
	(uint8_t)(FUSB_USB_PID), (uint8_t)(FUSB_USB_PID >> 8), //idProduct - ID Product
	(uint8_t)(FUSB_USB_REV), (uint8_t)(FUSB_USB_REV >> 8), //bcdDevice - Device Release Number
	1, //iManufacturer - Index of Manufacturer string
	2, //iProduct - Index of Product string
	3, //iSerialNumber - Index of Serial string
	1, //bNumConfigurations - Max number of configurations (if more then 1, you can switch between them)
};


static const uint8_t HIDAPIRepDesc[ ] =
{
	HID_USAGE_PAGE ( 0xff ), // Vendor-defined page.
	HID_USAGE      ( 0x00 ),
	HID_REPORT_SIZE ( 64 ),
	HID_COLLECTION( HID_COLLECTION_APPLICATION ),
		HID_REPORT_COUNT( 8 ),
		HID_USAGE( 0x01 ),
		HID_INPUT( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
		HID_REPORT_COUNT( 8 ),
		HID_USAGE( 0x01 ),
		HID_OUTPUT( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
		HID_COLLECTION ( HID_COLLECTION_LOGICAL ),
			HID_REPORT_COUNT   ( 7 ),
			HID_REPORT_ID      ( 0xaa )
			HID_USAGE          ( 0x01 ),
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
			HID_REPORT_COUNT   ( 127 ),
			HID_REPORT_ID      ( 0xac )
			HID_USAGE          ( 0x01 ),
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
			HID_REPORT_COUNT   ( 78 ),
			HID_REPORT_ID      ( 0xad )
			HID_USAGE          ( 0x01 ),
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
			HID_REPORT_COUNT_N ( 263, 2 ),
			HID_REPORT_ID      ( 0xae )
			HID_USAGE          ( 0x01 ),
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

			HID_REPORT_COUNT   ( 63 ), // For use with `hidapitester --vidpid 1209/D003 --open --read-feature 171`
			HID_REPORT_ID      ( 0xab )
			HID_USAGE          ( 0x01 ),	
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,

			// for reboot or internal terminal.
			HID_REPORT_COUNT   ( 8 ),
			HID_REPORT_ID      ( 0xe2 )
			HID_USAGE          ( 0x01 ),	
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,
			HID_REPORT_COUNT   ( 8 ),
			HID_REPORT_ID      ( 0xe1 )
			HID_USAGE          ( 0x01 ),	
			HID_FEATURE        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,
		HID_COLLECTION_END,
	HID_COLLECTION_END,
};

/* Configuration Descriptor Set */
static const uint8_t config_descriptor[ ] =
{
    /* Configuration Descriptor */
    0x09,                                                   // bLength
    0x02,                                                   // bDescriptorType
    0x5c, 0x00,                                             // wTotalLength
    0x03,                                                   // bNumInterfaces (3)
    0x01,                                                   // bConfigurationValue
    0x00,                                                   // iConfiguration
    0xA0,                                                   // bmAttributes: Bus Powered; Remote Wakeup
    0x32,                                                   // MaxPower: 100mA

    0x09,        // bLength
    0x04,        // bDescriptorType - Interface
    0x00,        // bInterfaceNumber - 0
    0x00,        // bAlternateSetting
    0x01,        // bNumEndpoints - 1
    0x02,        // bInterfaceClass - CDC
    0x02,        // bInterfaceSubClass - Abstract Control Model (Table 4 in CDC120.pdf)
    0x01,        // bInterfaceProtocol - AT Commands: V.250 etc (Table 5)
    0x00,        // iInterface (String Index)

    // Setting up CDC interface (Table 18)
    0x05,        // bLength
    0x24,        // bDescriptorType - CS_INTERFACE (Table 12)
    0x00,        // bDescriptorSubType - Header Functional Descriptor (Table 13)
    0x10, 0x01,  // bcdCDC - USB version - USB1.1
    // Call Management Functional Descriptor
    0x05,        // bLength
    0x24,        // bDescriptorType - CS_INTERFACE
    0x01,        // bDescriptorSubType - Call Management Functional Descriptor (Table 13)
    0x00,        // bmCapabilities: (Table 3 in PSTN120.pdf)
    // Bit 0 — Device handles call management itself:
    //  1 = device handles call management (e.g. call setup, termination, etc.)
    //  0 = host handles it
    // Bit 1 — Device can send/receive call management information over a Data Class interface:
    //  1 = can use the Data Class interface for call management
    //  0 = must use the Communication Class interface
    0x01,        // bDataInterface - Indicates that multiplexed commands are handled via data interface 01h (same value as used in the UNION Functional Descriptor)
    // Abstract Control Management Functional Descriptor
    0x04,        // bLength
    0x24,        // bDescriptorType - CS_INTERFACE
    0x02,        // bDescriptorSubType - Abstract Control Management Functional Descriptor (Table 13)
    0x02,        // bmCapabilities - Device supports the request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State (Table 4 in PSTN120.pdf)
    // Union Descriptor Functional Descriptor
    0x05,        // bLength
    0x24,        // bDescriptorType - CS_INTERFACE
    0x06,        // bDescriptorSubType - Union Descriptor Functional Descriptor (Table 13)
    0x00,        // bControlInterface (Interface number of the control (Communications Class) interface)
    0x01,        // bSubordinateInterface0 (Interface number of the subordinate (Data Class) interface)
    // Setting up EP1 for CDC config interface
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress (IN/D2H)
    0x03,        // bmAttributes (Interrupt)
    0x40, 0x00,  // wMaxPacketSize 64
    0x01,        // bInterval 1 (unit depends on device speed)

    // Transmission interface with two bulk endpoints
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    0x01,        // bInterfaceNumber 1
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints 2
    0x0A,        // bInterfaceClass
    0x00,        // bInterfaceSubClass
    0x00,        // bInterfaceProtocol - Transparent
    0x00,        // iInterface (String Index)
    // EP2 - device to host
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x02,        // bEndpointAddress (OUT/H2D)
    0x02,        // bmAttributes (Bulk)
    0x40, 0x00,  // wMaxPacketSize 64
    0x00,        // bInterval 0 (unit depends on device speed)
    // EP3 - host to device
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x83,        // bEndpointAddress (IN/D2H)
    0x02,        // bmAttributes (Bulk)
    0x40, 0x00,  // wMaxPacketSize 64
    0x00,        // bInterval 0 (unit depends on device speed)

    /* Interface Descriptor (HIDAPI) */
    0x09,                                                   // bLength
    0x04,                                                   // bDescriptorType
    0x02,                                                   // bInterfaceNumber
    0x00,                                                   // bAlternateSetting
    0x01,                                                   // bNumEndpoints
    0x03,                                                   // bInterfaceClass
    0x00,                                                   // bInterfaceSubClass
    0xff,                                                   // bInterfaceProtocol: Other
    0x00,                                                   // iInterface

    /* HID Descriptor (HIDAPI) */
    0x09,                                                   // bLength
    0x21,                                                   // bDescriptorType
    0x10, 0x01,                                             // bcdHID
    0x00,                                                   // bCountryCode
    0x01,                                                   // bNumDescriptors
    0x22,                                                   // bDescriptorType
    sizeof(HIDAPIRepDesc), 0x00,                            // wDescriptorLength

    /* Endpoint Descriptor (HIDAPI) */
    0x07,                                                   // bLength
    0x05,                                                   // bDescriptorType
    0x85,                                                   // bEndpointAddress: IN Endpoint 1
    0x03,                                                   // bmAttributes
    0x08, 0x00,                                             // wMaxPacketSize
    0xff,                                                   // bInterval: 255mS
};

struct usb_string_descriptor_struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wString[];
};
const static struct usb_string_descriptor_struct string0 __attribute__((section(".rodata"))) = {
	4,
	3,
	{0x0409}
};
const static struct usb_string_descriptor_struct string1 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_MANUFACTURER),
	3,
	FUSB_STR_MANUFACTURER
};
const static struct usb_string_descriptor_struct string2 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_PRODUCT),
	3,
	FUSB_STR_PRODUCT
};
const static struct usb_string_descriptor_struct string3 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_SERIAL),
	3,
	FUSB_STR_SERIAL
};

// This table defines which descriptor data is sent for each specific
// request from the host (in wValue and wIndex).
const static struct descriptor_list_struct {
	uint32_t	lIndexValue;
	const uint8_t	*addr;
	uint8_t		length;
} descriptor_list[] = {
	{0x00000100, device_descriptor, sizeof(device_descriptor)},
	{0x00000200, config_descriptor, sizeof(config_descriptor)},
	// interface number // 2200 for hid descriptors.
	{0x00022200, HIDAPIRepDesc, sizeof(HIDAPIRepDesc)},

	{0x00022100, config_descriptor + 76, 9 }, // Not sure why, this seems to be useful for Windows + Android.

	{0x00000300, (const uint8_t *)&string0, 4},
	{0x04090301, (const uint8_t *)&string1, string1.bLength},
	{0x04090302, (const uint8_t *)&string2, string2.bLength},
	{0x04090303, (const uint8_t *)&string3, string3.bLength}
};
#define DESCRIPTOR_LIST_ENTRIES ((sizeof(descriptor_list))/(sizeof(struct descriptor_list_struct)) )


#endif

