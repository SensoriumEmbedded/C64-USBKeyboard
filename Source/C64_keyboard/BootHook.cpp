/*
  Boot hook.  The core calls startup_middle_hook after clocks and pins are up
  (and on a Teensy 4, memory), before usb_init() and its ~300 ms start-up
  delay, and before the C++ constructors and setup(): cores/teensy4/startup.c
  on a Teensy 4, cores/teensy3/pins_teensy.c on a 3.6.  So the matrix is open
  and every output released within milliseconds of power-on, and the USB
  strings are final before the host reads them.
*/
#include <Arduino.h>
#include "Matrix.h"
#include "BuildStamp.h"
#include "Outputs.h"

extern "C" void UsbNames_AppendChipId(void);

extern "C" FLASHMEM void startup_middle_hook(void)
{
   Matrix_EarlyInit();
   Outputs_EarlyInit();
   UsbNames_AppendChipId();
}
