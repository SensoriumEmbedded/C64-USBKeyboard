/*
  Key numbers shared by the host protocol and the MT8808 driver.

  A key number is pa*8 + pb: the CIA1 port A bit ($DC00) and port B bit ($DC01)
  that the key connects, which is also the KERNAL's own key number (its $CB
  value).  Written in octal it is the protocol's two-digit key: 077 = RUN/STOP.

  The firmware's C64KP_* codes (C64keyboard.h) are MT8808 addresses: the same
  two 3-bit fields, each stored bit-reversed (AX2..AX0 and AY2..AY0).
*/
#pragma once
#include <stdint.h>

static inline uint8_t Rev3(uint8_t v) { return (uint8_t)(((v & 1) << 2) | (v & 2) | ((v >> 2) & 1)); }

// C64KP_* code (MT8808 address) to key number, and back
static inline uint8_t KeyNumFromC64KP(uint8_t code) { return (uint8_t)(Rev3((code >> 3) & 7) * 8 + Rev3(code & 7)); }
static inline uint8_t C64KPFromKeyNum(uint8_t key)  { return (uint8_t)((Rev3((key >> 3) & 7) << 3) | Rev3(key & 7)); }

static inline uint64_t KeyBit(uint8_t key) { return (uint64_t)1 << (key & 63); }

// Key numbers used by name (octal = <pa><pb>)
enum : uint8_t
{
   C64K_DEL     = 000,
   C64K_RETURN  = 001,
   C64K_CRSR_LR = 002,
   C64K_F7      = 003,
   C64K_F1      = 004,
   C64K_F3      = 005,
   C64K_F5      = 006,
   C64K_CRSR_UD = 007,
   C64K_LSHIFT  = 017,
   C64K_HOME    = 063,
   C64K_RSHIFT  = 064,
   C64K_CTRL    = 072,
   C64K_SPACE   = 074,
   C64K_CBM     = 075,
   C64K_RUNSTOP = 077,
};

// The loopback diagonal (the diag cartridge's harness): PAn to PBn
static const uint64_t LOOPBACK_KEYS =
   KeyBit(000) | KeyBit(011) | KeyBit(022) | KeyBit(033) |
   KeyBit(044) | KeyBit(055) | KeyBit(066) | KeyBit(077);

// SHIFT, CTRL and C=
static const uint64_t MODIFIER_KEYS =
   KeyBit(C64K_LSHIFT) | KeyBit(C64K_RSHIFT) | KeyBit(C64K_CTRL) | KeyBit(C64K_CBM);

// The order a matrix change is made in, the way a typist works: switches
// opening first, other keys before modifiers, then switches closing,
// modifiers before other keys.  Calls Set(key, on) for each switch that changes.
template <typename F>
inline void ForEachMatrixChange(uint64_t From, uint64_t To, F Set)
{
   const uint64_t opening = From & ~To, closing = To & ~From;
   const uint64_t order[4] =
   {
      opening & ~MODIFIER_KEYS, opening & MODIFIER_KEYS,
      closing & MODIFIER_KEYS,  closing & ~MODIFIER_KEYS,
   };
   for (int g = 0; g < 4; g++)
   {
      for (uint8_t k = 0; k < 64; k++)
      {
         if (order[g] & KeyBit(k)) Set(k, g >= 2);
      }
   }
}
