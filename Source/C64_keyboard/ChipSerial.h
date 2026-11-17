/*
  This chip's unique number, as PJRC's core computes it for the default USB
  serial number: the 24-bit ID from the HW_OCOTP_MAC0 fuse, times 10 when it's
  under 8 digits.  The same value as TeensyROM's TR_ChipSerialNum().
  static inline: included from C (UsbNames.c) and C++ alike.
*/
#pragma once
#include <stdint.h>

#if defined(__IMXRT1062__)
#include "imxrt.h"
static inline uint32_t KB_ChipSerialNum(void)
{
   uint32_t serialNum = HW_OCOTP_MAC0 & 0xFFFFFF;
   if (serialNum < 10000000) serialNum *= 10;
   return serialNum;
}
#else
static inline uint32_t KB_ChipSerialNum(void) { return 0; }   // Teensy 3.6: not reported
#endif

#if defined(ARDUINO_TEENSY41)
#define KB_MCU "teensy41"
#elif defined(ARDUINO_TEENSY36)
#define KB_MCU "teensy36"
#else
#define KB_MCU "teensy"
#endif
