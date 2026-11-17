/*
  The host protocol engine (Docs/Host_Protocol.md): runs parsed commands,
  timed actions, holds, timeouts, aborts and replies against a Hal.  No
  Arduino dependencies: the firmware gives it the Teensy's pins, and the host
  tests give it a simulated clock and outputs.
*/
#pragma once
#include <stdint.h>
#include "HostCmd.h"

enum class OutLine : uint8_t { Restore, Reset, Menu, Alt, Creset, Count };

class Hal
{
public:
   virtual uint64_t NowUs() = 0;                              // µs since start, 64-bit
   virtual void SetHostMatrix(uint64_t Mask) = 0;             // the host's switch closures (key numbers)
   virtual void SetLine(OutLine Line, bool Asserted) = 0;     // the host's own hold on a line
   virtual void StartPulse(OutLine Line, uint32_t Us) = 0;    // asserts now, releases Us later, exactly
   virtual bool PulseDone(uint64_t *T0, uint64_t *T1) = 0;    // true once released, with its edge times
   virtual void StopPulse(uint64_t *T0, uint64_t *T1) = 0;    // releases a running pulse now
   virtual void SetJ1(bool Closed) = 0;
   virtual void SetTod(uint8_t Mode) = 0;                     // TOD_OFF, TOD_50, TOD_60
   virtual void ApplyFitted(uint8_t Flags) = 0;               // pin setup for fitted and unfitted outputs
   virtual void SaveFitted(uint8_t Flags) = 0;
   virtual void WriteLine(const char *Line) = 0;              // one line, without its LF
   virtual void Activity() = 0;                               // a command was accepted (LED blink)
};

class LineBuf;

struct Identity
{
   const char *Fw;
   const char *Build;
   uint32_t    Chip;
   const char *Mcu;
};

class Engine
{
public:
   Engine(Hal &h, const Identity &id) : H(h), Id(id) {}

   void Begin(uint8_t Fitted);          // after the fitted flags are loaded
   void OnLine(const char *Line);       // a complete command line
   void OnJunkLine();                   // a line that was too long or held a bad byte
   void OnDtrDrop();                    // the host closed the port
   void Poll();                         // from the main loop: timed steps and hold timeouts

   bool    Active() const;              // the host holds something or an action runs (LED)
   bool    EventsOn() const { return Events; }
   uint8_t Fitted() const { return Fit; }
   void    PhysTodToggle();             // the USB keyboard's F9
   void    Event(const char *Text);     // a key or debug EV line, sent only while E is 1

   static const int MAX_OPS = MAX_STROKES * 4 + 8;

private:
   enum OpKind : uint8_t { OP_DOWN, OP_UP, OP_WAIT, OP_PULSE, OP_VRESET_ON, OP_VRESET_OFF, OP_J1 };
   struct Op
   {
      OpKind   Kind;
      OutLine  Line;       // OP_PULSE
      bool     Critical;   // V: from the CRESET_N pulse on, an abort must wait for the end
      uint32_t Us;         // OP_WAIT, OP_PULSE; OP_J1: 1 = closed
      uint64_t Keys;       // OP_DOWN, OP_UP
   };

   Hal            &H;
   const Identity &Id;

   uint8_t Fit = 0;
   bool    Events = true;
   uint8_t Tod = TOD_50;
   bool    J1Closed = false;

   // holds
   uint64_t HoldMask = 0;
   uint64_t HoldDeadline[64] = {};
   bool     ResetHeld = false;
   uint64_t ResetDeadline = 0;
   bool     LoopOn = false;
   uint64_t SentMatrix = 0;
   bool     SentReset = false;

   // the running timed action
   Op          Ops[MAX_OPS];
   int         NumOps = 0;
   int         Pc = 0;
   bool        Running = false;
   CmdType     RunType = CmdType::Press;
   const char *RunName = "";
   uint32_t    RunCount = 0;      // S: keys, X: steps
   uint64_t    StartUs = 0;
   bool        HaveT0 = false;
   uint64_t    T0 = 0, T1 = 0;
   bool        Waiting = false;
   uint64_t    WaitUntil = 0;
   bool        Pulsing = false;
   OutLine     PulseLine = OutLine::Restore;
   uint64_t    ActionMask = 0;
   bool        VReset = false;     // V's own hold on /RESET
   bool        AbortPending = false;   // V was aborted in its critical part; reply at its end
   bool        AbortSilent = false;    // ...but the port closed since, so no ERR V aborted
   uint8_t     AReplies = 0;           // OK A replies owed at that point
   bool        J1Before = false;       // J1 before V, restored if V stops before its pulse

   static Cmd C;   // parse buffer (large; one line at a time)

   void Run(const Cmd &c);
   bool Busy(const Cmd &c) const;
   bool NotFitted(const Cmd &c) const;
   void ReleaseHolds();
   bool AbortAction(bool FromA, bool Silent);   // true if deferred until V's end
   void UpdateMatrix();
   void UpdateReset();
   void Add(OpKind k, uint32_t Us = 0, uint64_t Keys = 0, OutLine Line = OutLine::Restore, bool Critical = false);
   void Start(const Cmd &c);
   void Step();
   void Edge(uint64_t t);
   void Finish();
   void Ok(LineBuf &b);
   void Err(const char *Name, const char *Reason);
   void TodField(LineBuf &b) const;
};
