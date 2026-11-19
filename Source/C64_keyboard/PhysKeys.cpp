#include "PhysKeys.h"
#include "BuildStamp.h"
#include "C64keyboard.h"   // KeyCodeToC64Map
#include "Keys.h"

bool PhysKeys::Key(uint8_t KeyCode, bool Pressed)
{
   if (KeyCode >= sizeof(KeyCodeToC64Map) || KeyCodeToC64Map[KeyCode] == C64KP_IGNORE) return false;
   uint64_t bit = (uint64_t)1 << (KeyCode & 63);
   if (Pressed) Held[KeyCode >> 6] |= bit;
   else Held[KeyCode >> 6] &= ~bit;
   return true;
}

void PhysKeys::ToggleCaps()
{
   CapsOn = !CapsOn;
}

void PhysKeys::Loopback()
{
   LoopOn = true;
}

void PhysKeys::Clear()
{
   for (uint64_t &h : Held) h = 0;
   CapsOn = false;
   LoopOn = false;
}

uint64_t PhysKeys::Mask() const
{
   uint64_t m = LoopOn ? LOOPBACK_KEYS : 0;
   if (CapsOn) m |= KeyBit(C64K_LSHIFT);
   for (unsigned code = 0; code < sizeof(KeyCodeToC64Map); code++)
   {
      if (!((Held[code >> 6] >> (code & 63)) & 1)) continue;
      uint8_t map = KeyCodeToC64Map[code];
      m |= KeyBit(KeyNumFromC64KP(map & 63));
      if (map & SHIFT) m |= KeyBit(C64K_RSHIFT);   // F2, cursor up/left, INST: typed shifted on the C64
   }
   return m;
}
