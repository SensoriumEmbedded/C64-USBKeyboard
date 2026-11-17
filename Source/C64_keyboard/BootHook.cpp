/*
  Teensy 4 boot hook.  The core (cores/teensy4/startup.c) calls
  startup_middle_hook after memory, clocks and pins are up, before usb_init()
  and its ~300 ms start-up delay, and before the C++ constructors and setup().
  So the matrix is open and every output released within milliseconds of
  power-on, and the USB strings are final before the host reads them.
  A Teensy 3.6 has no such hook; setup() does the same there, later.
*/
#if defined(__IMXRT1062__)

#include <Arduino.h>
#include "Matrix.h"
#include "Outputs.h"

extern "C" void UsbNames_AppendChipId(void);

extern "C" FLASHMEM void startup_middle_hook(void)
{
   Matrix_EarlyInit();
   Outputs_EarlyInit();
   UsbNames_AppendChipId();
}

#endif
