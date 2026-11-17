/*
  The board's output lines other than the matrix: RESTORE, C64 /RESET, the
  TeensyROM's menu and ALT buttons, the Kawari's J1 and CRESET_N, the TOD clock
  and the LED.

  Each line is asserted while the host holds it, a timed pulse runs on it, or
  the USB keyboard presses it (RESTORE on TAB, the menu button on F10).
  Unfitted lines stay Hi-Z: transistor drives (RESTORE, /RESET) and TOD with
  the internal pull-down, open-drain lines with no pull at all, since a
  pull-down would fight the target's own pull-up.
*/
#pragma once
#include <stdint.h>
#include "Engine.h"

uint64_t Now64();                                  // µs since start, 64-bit; main loop only

void Outputs_EarlyInit();                          // boot hook: every line released and safe
void Outputs_ApplyFitted(uint8_t Flags);           // FIT_* flags: pin modes and levels
void Outputs_SetHost(OutLine Line, bool Asserted);
void Outputs_SetPhys(OutLine Line, bool Asserted);
void Outputs_StartPulse(OutLine Line, uint32_t Us);
bool Outputs_PulseDone(uint64_t *T0, uint64_t *T1);
void Outputs_StopPulse(uint64_t *T0, uint64_t *T1);
void Outputs_SetJ1(bool Closed);
void Outputs_SetTod(uint8_t Mode);                 // TOD_OFF, TOD_50, TOD_60
void Outputs_LedBlink();                           // a 50 ms blink for an accepted command
void Outputs_LedUpdate(bool Active);               // from the main loop
