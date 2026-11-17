/*
  MT8808 crosspoint driver.  The USB keyboard's closures and the host's are
  kept apart and OR-ed: a switch is closed while either closes it.  Keys are
  key numbers (Keys.h).  Main loop only: the USB keyboard's events reach it
  through the queue in C64_keyboard.ino, never from the USB host interrupt.
*/
#pragma once
#include <stdint.h>

void Matrix_EarlyInit();                    // boot hook: hold the MT8808 in reset, every switch open
void Matrix_Begin();                        // setup(): pins, release reset
void Matrix_SetPhys(uint8_t Key, bool On);  // the USB keyboard
void Matrix_ResetChip();                    // F11/F12: MT8808 reset, the USB keyboard's closures dropped
void Matrix_SetHost(uint64_t Mask);         // the host protocol's closures
