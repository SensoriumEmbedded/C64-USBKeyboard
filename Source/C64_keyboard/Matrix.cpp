#include <Arduino.h>
#include "Matrix.h"
#include "Pins.h"
#include "Keys.h"

static uint64_t Phys = 0;      // the USB keyboard's closures
static uint64_t Host = 0;      // the host protocol's
static uint64_t Current = 0;   // what the MT8808 holds

static void ResetPulse()
{
   digitalWriteFast(MT8808_RESET_PIN, HIGH);
   delayMicroseconds(1);
   digitalWriteFast(MT8808_RESET_PIN, LOW);
   Current = 0;
}

// One switch, by its MT8808 address (C64KP_* code)
static void SetSwitch(uint8_t Code, bool On)
{
   digitalWrite(MT8808_DATA_PIN, On);
   for (uint8_t AddrBit = 0; AddrBit < 6; AddrBit++) digitalWrite(MT8808_ADDRESS_PINS[AddrBit], Code & (1 << AddrBit));
   digitalWrite(MT8808_STROBE_PIN, HIGH);
   digitalWrite(MT8808_STROBE_PIN, LOW);
}

static void Apply()
{
   uint64_t want = Phys | Host;
   uint64_t diff = want ^ Current;
   if (!diff) return;
   for (uint8_t k = 0; k < 64; k++)
   {
      if (diff & KeyBit(k)) SetSwitch(C64KPFromKeyNum(k), (want & KeyBit(k)) != 0);
   }
   Current = want;
}

void Matrix_EarlyInit()
{
   // Held in reset from the boot hook until setup(), so no switch can close
   // during the core's USB start-up delay
   digitalWriteFast(MT8808_RESET_PIN, HIGH);
   pinMode(MT8808_RESET_PIN, OUTPUT);
}

void Matrix_Begin()
{
   for (uint8_t AddrBit = 0; AddrBit < 6; AddrBit++) pinMode(MT8808_ADDRESS_PINS[AddrBit], OUTPUT);
   pinMode(MT8808_STROBE_PIN, OUTPUT);
   pinMode(MT8808_DATA_PIN, OUTPUT);
   pinMode(MT8808_RESET_PIN, OUTPUT);
   digitalWrite(MT8808_STROBE_PIN, LOW);
   ResetPulse();
   Phys = Host = 0;
}

void Matrix_SetPhys(uint8_t Key, bool On)
{
   if (On) Phys |= KeyBit(Key);
   else Phys &= ~KeyBit(Key);
   Apply();
}

void Matrix_ResetChip()
{
   ResetPulse();
   Phys = 0;
   Apply();   // the host's closures go back
}

void Matrix_SetHost(uint64_t Mask)
{
   Host = Mask;
   Apply();
}
