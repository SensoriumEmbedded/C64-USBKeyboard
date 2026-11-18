/*
  This chip's unique number, as PJRC's core computes it for the default USB
  serial number, times 10 when it's under 8 digits:
    Teensy 4.1: the 24-bit ID from the HW_OCOTP_MAC0 fuse.  The same value as
                TeensyROM's TR_ChipSerialNum().
    Teensy 3.6: read from the flash's program-once area, as the core's
                usb_init_serialnumber() (cores/teensy3/usb_desc.c) does.
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
#elif defined(__MK66FX1M0__)
#include "kinetis.h"
static inline uint32_t KB_ChipSerialNum(void)
{
   uint32_t primask, serialNum;
   __asm__ volatile("mrs %0, primask" : "=r"(primask));
   __disable_irq();
   kinetis_hsrun_disable();   // flash commands can't run in HSRUN (180 MHz)
   FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
   *(uint32_t *)&FTFL_FCCOB3 = 0x41070000;   // read once, record 7
   FTFL_FSTAT = FTFL_FSTAT_CCIF;
   while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ;
   serialNum = *(uint32_t *)&FTFL_FCCOBB;
   kinetis_hsrun_enable();
   if (!(primask & 1)) __enable_irq();
   if (serialNum < 10000000) serialNum *= 10;
   return serialNum;
}
#else
static inline uint32_t KB_ChipSerialNum(void) { return 0; }   // not reported
#endif

#if defined(ARDUINO_TEENSY41)
#define KB_MCU "teensy41"
#elif defined(ARDUINO_TEENSY36)
#define KB_MCU "teensy36"
#else
#define KB_MCU "teensy"
#endif
