/*
  USB product and serial number strings, replacing the core's weak defaults
  ("USB Serial" and a bare number), so a host finds the board by its serial
  string without opening any port: C64USBKeyboard-<chip id>.
  Adapted from TeensyROM's UsbNames.c (same author).

  C only: usb_string_descriptor_struct ends in a flexible array member, and
  these initializers are valid C99 but not reliably accepted by g++.
  Teensy 4 only; a Teensy 3.6 keeps the core's strings.
*/
#if defined(__IMXRT1062__)

#include "usb_names.h"
#include "avr_functions.h"
#include "avr/pgmspace.h"
#include "ChipSerial.h"

// Lengths must match the characters in each name.  The serial number's
// trailing 8 zeros reserve room for the chip id (up to 8 decimal digits).
#define PRODUCT_NAME     {'C','6','4',' ','U','S','B',' ','K','e','y','b','o','a','r','d'}
#define PRODUCT_NAME_LEN 16

#define SERIAL_BASE_LEN  15   // "C64USBKeyboard-"
#define SERIAL_NAME      {'C','6','4','U','S','B','K','e','y','b','o','a','r','d','-','0','0','0','0','0','0','0','0'}
#define SERIAL_NAME_LEN  23

// Do not change this part.  This exact format is required by USB.
struct usb_string_descriptor_struct usb_string_product_name = {
        2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
        2 + SERIAL_NAME_LEN * 2,
        3,
        SERIAL_NAME
};

// Writes the chip id into the serial number's reserved digits and shrinks
// bLength to fit.  Runs from the boot hook (BootHook.cpp), before usb_init().
FLASHMEM void UsbNames_AppendChipId(void)
{
	char buf[11];
	uint32_t i;

	ultoa(KB_ChipSerialNum(), buf, 10);
	for (i = 0; buf[i] && i < 8; i++) usb_string_serial_number.wString[SERIAL_BASE_LEN + i] = buf[i];
	usb_string_serial_number.bLength = 2 + (SERIAL_BASE_LEN + i) * 2;
}

#endif
