/*
  The USB keyboard's matrix closures.  They are worked out from the USB keys
  held, not set and cleared one switch at a time, so a C64 switch that two USB
  keys close stays closed until both are up: CapsLock and L-Shift, R-Shift and
  a key the C64 types shifted (F2, cursor up or left, INST), both Ctrl keys,
  Backspace and Delete.  No Arduino dependencies.
*/
#pragma once
#include <stdint.h>

class PhysKeys
{
public:
   bool     Key(uint8_t KeyCode, bool Pressed);   // a USB key; false if it isn't a C64 key
   void     ToggleCaps();                          // CapsLock: SHIFT latched on or off
   bool     Caps() const { return CapsOn; }
   void     Loopback();                            // F11: the loopback diagonal closed, until Clear()
   void     Clear();                               // F12, a lost event: nothing closed, CapsLock off
   uint64_t Mask() const;                          // the key numbers to close

private:
   uint64_t Held[4] = {};   // USB key codes down that are C64 keys, one bit each
   bool     CapsOn = false;
   bool     LoopOn = false;
};
