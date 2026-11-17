/*
  Parses one host protocol command line (Docs/Host_Protocol.md) into a Cmd.
  Checks syntax and ranges only; what the board has fitted, and whether it is
  busy, are the Engine's to check.  No Arduino dependencies.
*/
#pragma once
#include <stdint.h>
#include "TextKeys.h"

enum class CmdType : uint8_t
{
   Ident, Time,
   KeyDown, KeyUp, Press, Type, Restore, Abort,
   ResetPulse, ResetHold, ResetRelease, Button,
   TodSet, TodQuery, KawariSet, KawariQuery,
   Loop, Seq, Events, FitSet, FitQuery,
};

enum class StepType : uint8_t { Down, Up, Wait, Restore, Reset, Button };

enum : uint8_t { BTN_MENU = 0, BTN_ALT = 1 };
enum : uint8_t { TOD_OFF = 0, TOD_50 = 50, TOD_60 = 60 };

static const int MAX_KEYS    = 16;    // keys in one + list
static const int MAX_STEPS   = 16;    // X steps
static const int MAX_STROKES = 120;   // S keys

static const uint32_t DEF_HOLD_MS     = 50;
static const uint32_t DEF_GAP_MS      = 50;
static const uint32_t DEF_PULSE_MS    = 50;      // N, B, {restore}
static const uint32_t DEF_RESET_US    = 50000;
static const uint32_t DEF_MAX_MS      = 10000;   // K d and R hold
static const uint32_t DEF_RESTART_MS  = 1000;    // V
static const uint32_t MAX_SEQ_MS      = 10000;   // X total
static const uint32_t MAX_TYPE_MS     = 60000;   // S total

struct SeqStep
{
   StepType Type;
   uint8_t  Button;   // BTN_* for a b step
   uint32_t Val;      // ms (w, n, b) or µs (r)
   uint64_t Keys;     // d and u
};

struct Cmd
{
   CmdType     Type;
   const char *Name;     // command name for the reply: "?", "TIME", "K", ...
   uint64_t    Keys;     // KeyDown, KeyUp, Press
   uint32_t    A;        // KeyDown/ResetHold: max_ms; Press/Type: hold_ms; Restore/Button: ms;
                         // ResetPulse: µs; KawariSet: restart_ms; FitSet: 0 or 1
   uint32_t    B;        // Press/Type: gap_ms
   uint8_t     Sel;      // Button: BTN_*; TodSet: TOD_*; KawariSet: 1 = closed; Loop, Events: 1 = on;
                         // FitSet: FIT_* bit
   uint8_t     NumSteps;
   SeqStep     Steps[MAX_STEPS];
   uint8_t     NumStrokes;
   TextStroke  Strokes[MAX_STROKES];
};

// Returns true with Out filled, or false with Out.Name set (to "*" when the line
// has no recognizable command) and *Reason set to "syntax" or "range".
bool ParseCommand(const char *Line, Cmd &Out, const char **Reason);
