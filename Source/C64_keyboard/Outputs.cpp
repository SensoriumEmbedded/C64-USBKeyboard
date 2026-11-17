#include <Arduino.h>
#include "Outputs.h"
#include "Pins.h"
#include "Fitted.h"

// Interrupts off for the scope, restoring the previous state on exit
class IrqLock
{
public:
   IrqLock() { __asm__ volatile("mrs %0, primask" : "=r"(Primask)); __disable_irq(); }
   ~IrqLock() { if (!(Primask & 1)) __enable_irq(); }
private:
   uint32_t Primask;
};

enum LineKind : uint8_t { PUSH_PULL, OPEN_DRAIN };   // push-pull drives a transistor's base

struct LineDef
{
   uint8_t  Pin;
   LineKind Kind;
   uint8_t  Fit;   // FIT_* bit, 0 = always fitted
};

static const LineDef Lines[(int)OutLine::Count] =
{
   { NMI_RESTORE_PIN,   PUSH_PULL,  0          },   // Restore (Q1, on the PCB)
   { C64_RESET_OUT_PIN, PUSH_PULL,  FIT_RESET  },   // Reset
   { EXT_PULSE_OUT_PIN, OPEN_DRAIN, FIT_MENU   },   // Menu
   { EXT_ALT_OUT_PIN,   OPEN_DRAIN, FIT_ALT    },   // Alt
   { KAWARI_CRESET_PIN, OPEN_DRAIN, FIT_KAWARI },   // Creset
};

static volatile uint8_t HostBits = 0, PhysBits = 0, PulseBits = 0;   // bit per OutLine
static volatile uint8_t Fitted = 0;
static bool J1Closed = false;

static bool LineFitted(int i) { return !Lines[i].Fit || (Fitted & Lines[i].Fit); }
static bool LineOn(int i) { return ((HostBits | PhysBits | PulseBits) >> i) & 1; }

// Sets line i's pin from its state.  Interrupts off, or from the pulse interrupt.
static void Drive(int i)
{
   if (!LineFitted(i)) return;
   bool on = LineOn(i);
   if (Lines[i].Kind == PUSH_PULL) digitalWriteFast(Lines[i].Pin, on ? HIGH : LOW);
   else digitalWriteFast(Lines[i].Pin, on ? LOW : HIGH);
}

// --- clock ---------------------------------------------------------------------

// Main loop only: micros() extended to 64 bits.  It is read many times a
// second, far more often than micros() wraps (71 minutes).
uint64_t Now64()
{
   static uint32_t Last = 0, High = 0;
   uint32_t m = micros();
   if (m < Last) High++;
   Last = m;
   return ((uint64_t)High << 32) | m;
}

// A micros() value taken in an interrupt, as a Now64() time
static uint64_t Extend(uint32_t Then)
{
   uint64_t now = Now64();
   return now - (uint32_t)((uint32_t)now - Then);
}

// --- boot and fitted --------------------------------------------------------------

void Outputs_EarlyInit()
{
   digitalWriteFast(NMI_RESTORE_PIN, LOW);
   pinMode(NMI_RESTORE_PIN, OUTPUT);
   pinMode(C64_RESET_OUT_PIN, INPUT_PULLDOWN);
   pinMode(EXT_TOD_CLK_OUT_PIN, INPUT_PULLDOWN);
   pinMode(EXT_PULSE_OUT_PIN, INPUT);
   pinMode(EXT_ALT_OUT_PIN, INPUT);
   pinMode(KAWARI_J1_PIN, INPUT);
   pinMode(KAWARI_CRESET_PIN, INPUT);
   digitalWriteFast(INDICATOR_LED_PIN, HIGH);   // on while starting
   pinMode(INDICATOR_LED_PIN, OUTPUT);
}

static IntervalTimer TodTimer;
static bool TodRunning = false;
static volatile bool TodLevel = false;

void Outputs_ApplyFitted(uint8_t Flags)
{
   IrqLock lock;
   Fitted = Flags;
   for (int i = 0; i < (int)OutLine::Count; i++)
   {
      const LineDef &d = Lines[i];
      if (!LineFitted(i))
      {
         pinMode(d.Pin, d.Kind == PUSH_PULL ? INPUT_PULLDOWN : INPUT);
         continue;
      }
      // Level first, so becoming an output makes no glitch
      bool on = LineOn(i);
      if (d.Kind == PUSH_PULL)
      {
         digitalWriteFast(d.Pin, on ? HIGH : LOW);
         pinMode(d.Pin, OUTPUT);
      }
      else
      {
         digitalWriteFast(d.Pin, on ? LOW : HIGH);
         pinMode(d.Pin, OUTPUT_OPENDRAIN);
      }
   }

   if (Flags & FIT_KAWARI)
   {
      digitalWriteFast(KAWARI_J1_PIN, J1Closed ? LOW : HIGH);
      pinMode(KAWARI_J1_PIN, OUTPUT_OPENDRAIN);
   }
   else
   {
      J1Closed = false;
      pinMode(KAWARI_J1_PIN, INPUT);
   }

   if (Flags & FIT_TOD) pinMode(EXT_TOD_CLK_OUT_PIN, OUTPUT);   // Outputs_SetTod starts it
   else
   {
      TodTimer.end();
      TodRunning = false;
      pinMode(EXT_TOD_CLK_OUT_PIN, INPUT_PULLDOWN);
   }
}

// --- lines ---------------------------------------------------------------------

void Outputs_SetHost(OutLine Line, bool Asserted)
{
   int i = (int)Line;
   IrqLock lock;
   if (Asserted) HostBits |= (uint8_t)(1 << i);
   else HostBits &= (uint8_t)~(1 << i);
   Drive(i);
}

void Outputs_SetPhys(OutLine Line, bool Asserted)
{
   int i = (int)Line;
   IrqLock lock;
   if (Asserted) PhysBits |= (uint8_t)(1 << i);
   else PhysBits &= (uint8_t)~(1 << i);
   Drive(i);
}

void Outputs_SetJ1(bool Closed)
{
   J1Closed = Closed;
   if (Fitted & FIT_KAWARI) digitalWriteFast(KAWARI_J1_PIN, Closed ? LOW : HIGH);
}

// --- pulses ----------------------------------------------------------------------
// One pulse at a time.  It ends in the timer interrupt, so how long the main
// loop takes never stretches it.

static IntervalTimer      PulseTimer;
static volatile int       PulseIdx = -1;
static volatile bool      PulseFinished = false;
static uint64_t           PulseT0 = 0;
static volatile uint32_t  PulseT1Micros = 0;

static void PulseIsr()
{
   PulseTimer.end();
   int i = PulseIdx;
   if (i < 0) return;
   PulseBits &= (uint8_t)~(1 << i);
   Drive(i);
   PulseT1Micros = micros();
   PulseIdx = -1;
   PulseFinished = true;
}

void Outputs_StartPulse(OutLine Line, uint32_t Us)
{
   int i = (int)Line;
   PulseT0 = Now64();
   IrqLock lock;
   PulseFinished = false;
   PulseIdx = i;
   PulseBits |= (uint8_t)(1 << i);
   Drive(i);
   PulseTimer.begin(PulseIsr, Us);
}

bool Outputs_PulseDone(uint64_t *T0, uint64_t *T1)
{
   uint32_t t1;
   {
      IrqLock lock;
      if (!PulseFinished) return false;
      PulseFinished = false;
      t1 = PulseT1Micros;
   }
   *T0 = PulseT0;
   *T1 = Extend(t1);
   return true;
}

void Outputs_StopPulse()
{
   IrqLock lock;
   if (PulseIdx >= 0)
   {
      PulseTimer.end();
      int i = PulseIdx;
      PulseBits &= (uint8_t)~(1 << i);
      Drive(i);
      PulseIdx = -1;
   }
   PulseFinished = false;
}

// --- TOD -------------------------------------------------------------------------

static void TodIsr()
{
   TodLevel = !TodLevel;
   digitalWriteFast(EXT_TOD_CLK_OUT_PIN, TodLevel);
}

void Outputs_SetTod(uint8_t Mode)
{
   if (!(Fitted & FIT_TOD)) return;
   if (Mode == TOD_OFF)
   {
      TodTimer.end();   // the pin keeps its level, so no runt edge
      TodRunning = false;
      return;
   }
   uint32_t us = (Mode == TOD_60) ? IntervaluS_60Hz : IntervaluS_50Hz;
   if (TodRunning) TodTimer.update(us);   // takes effect at the next edge
   else TodRunning = TodTimer.begin(TodIsr, us);
}

// --- LED -------------------------------------------------------------------------

static uint32_t BlinkUntil = 0;

void Outputs_LedBlink()
{
   BlinkUntil = millis() + 50;
}

void Outputs_LedUpdate(bool Active)
{
   bool on = Active || (int32_t)(BlinkUntil - millis()) > 0;
   digitalWriteFast(INDICATOR_LED_PIN, on ? HIGH : LOW);
}
