#include "HostCmd.h"
#include "BuildStamp.h"
#include "Fitted.h"
#include "Fmt.h"
#include <string.h>

static const char SYNTAX[] = "syntax";
static const char RANGE[]  = "range";

#define FAIL(r) do { *Reason = (r); return false; } while (0)

struct Tok
{
   const char *P;
   int         Len;
};

// Space-separated tokens within [P, End)
class Scanner
{
public:
   Scanner(const char *Begin, const char *End) : P(Begin), E(End) {}
   bool Next(Tok &t)
   {
      while (P < E && *P == ' ') P++;
      if (P >= E) return false;
      t.P = P;
      while (P < E && *P != ' ') P++;
      t.Len = (int)(P - t.P);
      return true;
   }
   bool AtEnd()
   {
      const char *q = P;
      while (q < E && *q == ' ') q++;
      return q >= E;
   }
   const char *Pos() const { return P; }

private:
   const char *P;
   const char *E;
};

static bool Is(const Tok &t, const char *Word) { return SameWord(t.P, t.Len, Word); }

// Decimal number in [Min, Max]
static bool Num(const Tok &t, uint32_t Min, uint32_t Max, uint32_t *v, const char **Reason)
{
   if (t.Len == 0) FAIL(SYNTAX);
   uint64_t n = 0;
   for (int i = 0; i < t.Len; i++)
   {
      if (t.P[i] < '0' || t.P[i] > '9') FAIL(SYNTAX);
      if (i >= 10) FAIL(RANGE);
      n = n * 10 + (uint64_t)(t.P[i] - '0');
   }
   if (n < Min || n > Max) FAIL(RANGE);
   *v = (uint32_t)n;
   return true;
}

// Optional number: the next token if there is one, else Default
static bool OptNum(Scanner &s, uint32_t Min, uint32_t Max, uint32_t Default, uint32_t *v, const char **Reason)
{
   Tok t;
   if (!s.Next(t)) { *v = Default; return true; }
   return Num(t, Min, Max, v, Reason);
}

// <pa><pb>(+<pa><pb>)*
static bool Keys(const Tok &t, uint64_t *Mask, const char **Reason)
{
   uint64_t m = 0;
   int count = 0;
   int i = 0;
   for (;;)
   {
      if (i + 2 > t.Len) FAIL(SYNTAX);
      char a = t.P[i], b = t.P[i + 1];
      if (a < '0' || a > '9' || b < '0' || b > '9') FAIL(SYNTAX);
      if (a > '7' || b > '7') FAIL(RANGE);
      m |= (uint64_t)1 << ((a - '0') * 8 + (b - '0'));
      count++;
      i += 2;
      if (i == t.Len) break;
      if (t.P[i] != '+') FAIL(SYNTAX);
      i++;
   }
   if (count > MAX_KEYS) FAIL(RANGE);
   *Mask = m;
   return true;
}

static bool Button(const Tok &t, uint8_t *Btn, const char **Reason)
{
   if (Is(t, "menu")) *Btn = BTN_MENU;
   else if (Is(t, "alt")) *Btn = BTN_ALT;
   else FAIL(SYNTAX);
   return true;
}

static bool End(Scanner &s, const char **Reason)
{
   if (!s.AtEnd()) FAIL(SYNTAX);
   return true;
}

// S and ST text: everything after the one space that follows the last field
static bool Text(const char *After, uint32_t Hold, uint32_t Gap, Cmd &C, const char **Reason)
{
   if (*After != ' ' || !After[1]) FAIL(SYNTAX);
   int errPos;
   int n = TextToStrokes(After + 1, C.Strokes, MAX_STROKES, &errPos);
   if (n < 0) FAIL(RANGE);
   uint64_t total = 0;   // as the Engine runs it
   for (int i = 0; i < n; i++)
   {
      const TextStroke &s = C.Strokes[i];
      total += (s.Restore ? DEF_PULSE_MS : Hold) + Gap + (s.Mods ? 2 * MOD_LEAD_MS : 0);
   }
   if (total > MAX_TYPE_MS) FAIL(RANGE);
   C.NumStrokes = (uint8_t)n;
   C.A = Hold;
   C.B = Gap;
   return true;
}

static bool Sequence(const char *Begin, const char *EndP, Cmd &C, const char **Reason)
{
   uint64_t totalUs = 0;
   int n = 0;
   const char *seg = Begin;
   for (;;)
   {
      const char *segEnd = seg;
      while (segEnd < EndP && *segEnd != ';') segEnd++;

      Scanner s(seg, segEnd);
      Tok t, v;
      if (!s.Next(t)) FAIL(SYNTAX);   // empty step
      if (n >= MAX_STEPS) FAIL(RANGE);
      SeqStep &st = C.Steps[n];
      st.Keys = 0;
      st.Val = 0;
      st.Button = 0;
      if (Is(t, "d") || Is(t, "u"))
      {
         st.Type = Is(t, "d") ? StepType::Down : StepType::Up;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Keys(v, &st.Keys, Reason)) return false;
      }
      else if (Is(t, "w"))
      {
         st.Type = StepType::Wait;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Num(v, 0, 10000, &st.Val, Reason)) return false;
         totalUs += (uint64_t)st.Val * 1000;
      }
      else if (Is(t, "n"))
      {
         st.Type = StepType::Restore;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Num(v, 1, 10000, &st.Val, Reason)) return false;
         totalUs += (uint64_t)st.Val * 1000;
      }
      else if (Is(t, "r"))
      {
         st.Type = StepType::Reset;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Num(v, 10, 10000000, &st.Val, Reason)) return false;
         totalUs += st.Val;
      }
      else if (Is(t, "b"))
      {
         st.Type = StepType::Button;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Button(v, &st.Button, Reason)) return false;
         if (!s.Next(v)) FAIL(SYNTAX);
         if (!Num(v, 1, 10000, &st.Val, Reason)) return false;
         totalUs += (uint64_t)st.Val * 1000;
      }
      else FAIL(SYNTAX);
      if (!End(s, Reason)) return false;
      n++;

      if (segEnd >= EndP) break;
      seg = segEnd + 1;
   }
   if (totalUs > (uint64_t)MAX_SEQ_MS * 1000) FAIL(RANGE);
   C.NumSteps = (uint8_t)n;
   return true;
}

bool ParseCommand(const char *Line, Cmd &C, const char **Reason)
{
   const char *lineEnd = Line + strlen(Line);
   Scanner s(Line, lineEnd);
   Tok t, v;

   C.Name = "*";
   C.Keys = 0;
   C.A = C.B = 0;
   C.Sel = 0;
   C.NumSteps = 0;
   C.NumStrokes = 0;
   *Reason = SYNTAX;
   if (!s.Next(t)) return false;

   if (Is(t, "?"))
   {
      C.Name = "?";
      C.Type = CmdType::Ident;
      return End(s, Reason);
   }
   if (Is(t, "TIME"))
   {
      C.Name = "TIME";
      C.Type = CmdType::Time;
      return End(s, Reason);
   }
   if (Is(t, "K"))
   {
      C.Name = "K";
      if (!s.Next(v)) FAIL(SYNTAX);
      if (!Keys(v, &C.Keys, Reason)) return false;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "d"))
      {
         C.Type = CmdType::KeyDown;
         if (!OptNum(s, 1, 10000, DEF_MAX_MS, &C.A, Reason)) return false;
      }
      else if (Is(v, "u")) C.Type = CmdType::KeyUp;
      else FAIL(SYNTAX);
      return End(s, Reason);
   }
   if (Is(t, "P"))
   {
      C.Name = "P";
      C.Type = CmdType::Press;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (!Keys(v, &C.Keys, Reason)) return false;
      if (!OptNum(s, 1, 10000, DEF_HOLD_MS, &C.A, Reason)) return false;
      if (!OptNum(s, 0, 10000, DEF_GAP_MS, &C.B, Reason)) return false;
      return End(s, Reason);
   }
   if (Is(t, "S"))
   {
      C.Name = "S";
      C.Type = CmdType::Type;
      return Text(s.Pos(), DEF_HOLD_MS, DEF_GAP_MS, C, Reason);
   }
   if (Is(t, "ST"))
   {
      C.Name = "ST";
      C.Type = CmdType::Type;
      uint32_t hold, gap;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (!Num(v, MIN_TYPE_MS, 10000, &hold, Reason)) return false;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (!Num(v, MIN_TYPE_MS, 10000, &gap, Reason)) return false;
      return Text(s.Pos(), hold, gap, C, Reason);
   }
   if (Is(t, "N"))
   {
      C.Name = "N";
      C.Type = CmdType::Restore;
      if (!OptNum(s, 1, 10000, DEF_PULSE_MS, &C.A, Reason)) return false;
      return End(s, Reason);
   }
   if (Is(t, "A"))
   {
      C.Name = "A";
      C.Type = CmdType::Abort;
      return End(s, Reason);
   }
   if (Is(t, "R"))
   {
      C.Name = "R";
      if (!s.Next(v))
      {
         C.Type = CmdType::ResetPulse;
         C.A = DEF_RESET_US;
         return true;
      }
      if (Is(v, "hold"))
      {
         C.Type = CmdType::ResetHold;
         if (!OptNum(s, 1, 10000, DEF_MAX_MS, &C.A, Reason)) return false;
      }
      else if (Is(v, "release")) C.Type = CmdType::ResetRelease;
      else
      {
         C.Type = CmdType::ResetPulse;
         if (!Num(v, 10, 10000000, &C.A, Reason)) return false;
      }
      return End(s, Reason);
   }
   if (Is(t, "B"))
   {
      C.Name = "B";
      C.Type = CmdType::Button;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (!Button(v, &C.Sel, Reason)) return false;
      if (!OptNum(s, 1, 10000, DEF_PULSE_MS, &C.A, Reason)) return false;
      return End(s, Reason);
   }
   if (Is(t, "T"))
   {
      C.Name = "T";
      if (!s.Next(v)) FAIL(SYNTAX);
      C.Type = CmdType::TodSet;
      if (Is(v, "50")) C.Sel = TOD_50;
      else if (Is(v, "60")) C.Sel = TOD_60;
      else if (Is(v, "off")) C.Sel = TOD_OFF;
      else if (Is(v, "?")) C.Type = CmdType::TodQuery;
      else FAIL(SYNTAX);
      return End(s, Reason);
   }
   if (Is(t, "V"))
   {
      C.Name = "V";
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "?"))
      {
         C.Type = CmdType::KawariQuery;
         return End(s, Reason);
      }
      C.Type = CmdType::KawariSet;
      if (Is(v, "open")) C.Sel = 0;
      else if (Is(v, "closed")) C.Sel = 1;
      else FAIL(SYNTAX);
      if (!OptNum(s, 10, 10000, DEF_RESTART_MS, &C.A, Reason)) return false;
      return End(s, Reason);
   }
   if (Is(t, "D"))
   {
      C.Name = "D";
      C.Type = CmdType::Loop;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "on")) C.Sel = 1;
      else if (Is(v, "off")) C.Sel = 0;
      else FAIL(SYNTAX);
      return End(s, Reason);
   }
   if (Is(t, "X"))
   {
      C.Name = "X";
      C.Type = CmdType::Seq;
      return Sequence(s.Pos(), lineEnd, C, Reason);
   }
   if (Is(t, "E"))
   {
      C.Name = "E";
      C.Type = CmdType::Events;
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "1")) C.Sel = 1;
      else if (Is(v, "0")) C.Sel = 0;
      else FAIL(SYNTAX);
      return End(s, Reason);
   }
   if (Is(t, "F"))
   {
      C.Name = "F";
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "?"))
      {
         C.Type = CmdType::FitQuery;
         return End(s, Reason);
      }
      C.Type = CmdType::FitSet;
      C.Sel = FitFromName(v.P, v.Len);
      if (!C.Sel) FAIL(SYNTAX);
      if (!s.Next(v)) FAIL(SYNTAX);
      if (Is(v, "1")) C.A = 1;
      else if (Is(v, "0")) C.A = 0;
      else FAIL(SYNTAX);
      return End(s, Reason);
   }
   FAIL(SYNTAX);
}
