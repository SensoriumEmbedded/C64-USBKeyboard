#include "Engine.h"
#include "Keys.h"
#include "Fitted.h"
#include "Fmt.h"

Cmd Engine::C;

void Engine::Begin(uint8_t Fitted)
{
   Fit = Fitted & FIT_ALL;
   H.ApplyFitted(Fit);
   if (Fit & FIT_TOD) H.SetTod(Tod);
}

bool Engine::Active() const
{
   return Running || HoldMask || ResetHeld || LoopOn;
}

void Engine::Event(const char *Text)
{
   if (!Events) return;
   LineBuf b;
   b.Str("EV ").Str(Text);
   H.WriteLine(b.C());
}

void Engine::Ok(LineBuf &b)
{
   H.WriteLine(b.C());
   H.Activity();
}

void Engine::Err(const char *Name, const char *Reason)
{
   LineBuf b;
   b.Str("ERR ").Str(Name).Ch(' ').Str(Reason);
   H.WriteLine(b.C());
}

void Engine::TodField(LineBuf &b) const
{
   if (!(Fit & FIT_TOD)) b.Ch('-');
   else if (Tod == TOD_OFF) b.Str("off");
   else b.U64(Tod);
}

void Engine::OnJunkLine()
{
   Err("*", "syntax");
}

void Engine::OnLine(const char *Line)
{
   const char *reason;
   if (!ParseCommand(Line, C, &reason)) { Err(C.Name, reason); return; }
   if (NotFitted(C)) { Err(C.Name, "notfitted"); return; }
   if (Busy(C)) { Err(C.Name, "busy"); return; }
   Run(C);
}

void Engine::OnDtrDrop()
{
   // Nothing from this session reaches the next one: no ERR for the aborted
   // action, and no OK A owed to the closed port
   ReleaseHolds();
   if (Running) AbortAction(false, true);
   AReplies = 0;
   Events = true;
}

bool Engine::Busy(const Cmd &c) const
{
   if (!Running) return false;
   switch (c.Type)
   {
      case CmdType::Ident:
      case CmdType::Time:
      case CmdType::TodQuery:
      case CmdType::KawariQuery:
      case CmdType::FitQuery:
      case CmdType::Events:
      case CmdType::Abort:
         return false;
      default:
         return true;   // it would change an output
   }
}

bool Engine::NotFitted(const Cmd &c) const
{
   switch (c.Type)
   {
      case CmdType::TodSet:
         return !(Fit & FIT_TOD);
      case CmdType::Button:
         return !(Fit & (c.Sel == BTN_MENU ? FIT_MENU : FIT_ALT));
      case CmdType::ResetPulse:
      case CmdType::ResetHold:
      case CmdType::ResetRelease:
         return !(Fit & FIT_RESET);
      case CmdType::KawariSet:
         return (Fit & (FIT_KAWARI | FIT_RESET)) != (FIT_KAWARI | FIT_RESET);
      case CmdType::Seq:
         for (int i = 0; i < c.NumSteps; i++)
         {
            const SeqStep &s = c.Steps[i];
            if (s.Type == StepType::Reset && !(Fit & FIT_RESET)) return true;
            if (s.Type == StepType::Button && !(Fit & (s.Button == BTN_MENU ? FIT_MENU : FIT_ALT))) return true;
         }
         return false;
      default:
         return false;
   }
}

void Engine::UpdateMatrix()
{
   uint64_t m = HoldMask | ActionMask | (LoopOn ? LOOPBACK_KEYS : 0);
   if (m == SentMatrix) return;
   SentMatrix = m;
   H.SetHostMatrix(m);
}

void Engine::UpdateReset()
{
   bool a = ResetHeld || VReset;
   if (a == SentReset) return;
   SentReset = a;
   H.SetLine(OutLine::Reset, a);
}

void Engine::ReleaseHolds()
{
   HoldMask = 0;
   ResetHeld = false;
   LoopOn = false;
   UpdateMatrix();
   UpdateReset();
}

void Engine::Run(const Cmd &c)
{
   uint64_t now = H.NowUs();
   LineBuf b;
   switch (c.Type)
   {
      case CmdType::Ident:
      {
         bool resetOn = ResetHeld || VReset || (Pulsing && PulseLine == OutLine::Reset);
         b.Str("OK ? proto=1 name=C64USBKeyboard fw=").Str(Id.Fw)
          .Str(" build=").Str(Id.Build)
          .Str(" chip=").U64(Id.Chip)
          .Str(" mcu=").Str(Id.Mcu)
          .Str(" fitted=");
         FitList(b, Fit);
         b.Str(" held=").Keys(HoldMask).Str(" reset=");
         if (!(Fit & FIT_RESET)) b.Ch('-'); else b.Ch(resetOn ? '1' : '0');
         b.Str(" tod=");
         TodField(b);
         b.Str(" j1=");
         if (!(Fit & FIT_KAWARI)) b.Ch('-'); else b.Str(J1Closed ? "closed" : "open");
         b.Str(" loop=").Str(LoopOn ? "on" : "off").Str(" e=").Ch(Events ? '1' : '0');
         Ok(b);
         return;
      }
      case CmdType::Time:
         b.Str("OK TIME us=").U64(now);
         Ok(b);
         return;
      case CmdType::KeyDown:
      {
         uint64_t deadline = now + (uint64_t)c.A * 1000;
         for (int k = 0; k < 64; k++)
         {
            if (c.Keys & KeyBit((uint8_t)k)) HoldDeadline[k] = deadline;
         }
         if (deadline < NextHold) NextHold = deadline;
         HoldMask |= c.Keys;
         UpdateMatrix();
         b.Str("OK K t=").U64(now);
         Ok(b);
         return;
      }
      case CmdType::KeyUp:
         HoldMask &= ~c.Keys;
         UpdateMatrix();
         b.Str("OK K t=").U64(now);
         Ok(b);
         return;
      case CmdType::ResetHold:
         ResetHeld = true;
         ResetDeadline = now + (uint64_t)c.A * 1000;
         UpdateReset();
         b.Str("OK R t=").U64(now);
         Ok(b);
         return;
      case CmdType::ResetRelease:
         ResetHeld = false;
         UpdateReset();
         b.Str("OK R t=").U64(now);
         Ok(b);
         return;
      case CmdType::TodSet:
         Tod = c.Sel;
         H.SetTod(Tod);
         b.Str("OK T tod=");
         TodField(b);
         Ok(b);
         return;
      case CmdType::TodQuery:
         b.Str("OK T tod=");
         TodField(b);
         Ok(b);
         return;
      case CmdType::KawariQuery:
         b.Str("OK V j1=");
         if (!(Fit & FIT_KAWARI)) b.Str("- creset=-");
         else
         {
            b.Str(J1Closed ? "closed" : "open").Str(" creset=")
             .Str(Pulsing && PulseLine == OutLine::Creset ? "asserted" : "released");
         }
         Ok(b);
         return;
      case CmdType::Loop:
         LoopOn = c.Sel != 0;
         UpdateMatrix();
         b.Str("OK D loop=").Str(LoopOn ? "on" : "off");
         Ok(b);
         return;
      case CmdType::Events:
         Events = c.Sel != 0;
         b.Str("OK E e=").Ch(Events ? '1' : '0');
         Ok(b);
         return;
      case CmdType::FitQuery:
         b.Str("OK F fitted=");
         FitList(b, Fit);
         Ok(b);
         return;
      case CmdType::FitSet:
      {
         uint8_t nf = c.A ? (uint8_t)(Fit | c.Sel) : (uint8_t)(Fit & ~c.Sel);
         if (nf != Fit)
         {
            if (!(nf & FIT_RESET)) { ResetHeld = false; UpdateReset(); }
            if (!(nf & FIT_KAWARI)) J1Closed = false;
            Fit = nf;
            H.ApplyFitted(Fit);
            if (Fit & FIT_TOD) H.SetTod(Tod);
            H.SaveFitted(Fit);
         }
         b.Str("OK F fitted=");
         FitList(b, Fit);
         Ok(b);
         return;
      }
      case CmdType::Abort:
         ReleaseHolds();
         if (Running && AbortAction(true, false)) return;   // OK A follows V's reply
         b.Str("OK A");
         Ok(b);
         return;
      default:
         Start(c);
         return;
   }
}

void Engine::Add(OpKind k, uint32_t Us, uint64_t Keys, OutLine Line, bool Critical)
{
   if (NumOps >= MAX_OPS) return;   // the parser's limits keep every command well inside
   Op &o = Ops[NumOps++];
   o.Kind = k;
   o.Line = Line;
   o.Critical = Critical;
   o.Us = Us;
   o.Keys = Keys;
}

void Engine::Start(const Cmd &c)
{
   NumOps = 0;
   Pc = 0;
   HaveT0 = false;
   Waiting = false;
   Pulsing = false;
   ActionMask = 0;
   AbortPending = false;
   AbortSilent = false;
   J1Before = J1Closed;
   RunType = c.Type;
   RunName = c.Name;
   RunCount = 0;
   StartUs = H.NowUs();

   switch (c.Type)
   {
      case CmdType::Press:
         Add(OP_DOWN, 0, c.Keys);
         Add(OP_WAIT, c.A * 1000);
         Add(OP_UP, 0, c.Keys);
         Add(OP_WAIT, c.B * 1000);
         break;
      case CmdType::Type:
         for (int i = 0; i < c.NumStrokes; i++)
         {
            const TextStroke &s = c.Strokes[i];
            if (s.Restore)
            {
               Add(OP_PULSE, DEF_PULSE_MS * 1000, 0, OutLine::Restore);
            }
            else
            {
               // Modifiers a scan ahead of the key, and released a scan after it
               uint64_t mods = 0;
               if (s.Mods & MOD_SHIFT) mods |= KeyBit(C64K_LSHIFT);
               if (s.Mods & MOD_CTRL)  mods |= KeyBit(C64K_CTRL);
               if (s.Mods & MOD_CBM)   mods |= KeyBit(C64K_CBM);
               if (mods)
               {
                  Add(OP_DOWN, 0, mods);
                  Add(OP_WAIT, MOD_LEAD_MS * 1000);
               }
               Add(OP_DOWN, 0, KeyBit(s.Key));
               Add(OP_WAIT, c.A * 1000);
               Add(OP_UP, 0, KeyBit(s.Key));
               if (mods)
               {
                  Add(OP_WAIT, MOD_LEAD_MS * 1000);
                  Add(OP_UP, 0, mods);
               }
            }
            Add(OP_WAIT, c.B * 1000);
         }
         RunCount = c.NumStrokes;
         break;
      case CmdType::Restore:
         Add(OP_PULSE, c.A * 1000, 0, OutLine::Restore);
         break;
      case CmdType::ResetPulse:
         Add(OP_PULSE, c.A, 0, OutLine::Reset);
         break;
      case CmdType::Button:
         Add(OP_PULSE, c.A * 1000, 0, c.Sel == BTN_MENU ? OutLine::Menu : OutLine::Alt);
         break;
      case CmdType::KawariSet:
         // /RESET down, J1 set, CRESET_N pulse, the Kawari's restart, /RESET up.
         // From the pulse on, an abort waits for the end: releasing /RESET while
         // the FPGA reloads would start the 6510 with PHI0 stopped.
         Add(OP_VRESET_ON);
         Add(OP_J1, c.Sel);
         Add(OP_WAIT, 1000);
         Add(OP_PULSE, 1000, 0, OutLine::Creset, true);
         Add(OP_WAIT, c.A * 1000, 0, OutLine::Restore, true);
         Add(OP_VRESET_OFF, 0, 0, OutLine::Restore, true);
         break;
      case CmdType::Seq:
         for (int i = 0; i < c.NumSteps; i++)
         {
            const SeqStep &s = c.Steps[i];
            switch (s.Type)
            {
               case StepType::Down:    Add(OP_DOWN, 0, s.Keys); break;
               case StepType::Up:      Add(OP_UP, 0, s.Keys); break;
               case StepType::Wait:    Add(OP_WAIT, s.Val * 1000); break;
               case StepType::Restore: Add(OP_PULSE, s.Val * 1000, 0, OutLine::Restore); break;
               case StepType::Reset:   Add(OP_PULSE, s.Val, 0, OutLine::Reset); break;
               case StepType::Button:
                  Add(OP_PULSE, s.Val * 1000, 0, s.Button == BTN_MENU ? OutLine::Menu : OutLine::Alt);
                  break;
            }
         }
         RunCount = c.NumSteps;
         break;
      default:
         return;
   }
   Running = true;
   Step();
}

void Engine::Edge(uint64_t t)
{
   if (!HaveT0)
   {
      T0 = t;
      HaveT0 = true;
   }
   T1 = t;
}

void Engine::Step()
{
   while (Running)
   {
      if (Pc >= NumOps)
      {
         Finish();
         return;
      }
      const Op &o = Ops[Pc];
      uint64_t now = H.NowUs();
      switch (o.Kind)
      {
         case OP_DOWN:
            ActionMask |= o.Keys;
            UpdateMatrix();
            Edge(now);
            break;
         case OP_UP:
            ActionMask &= ~o.Keys;
            UpdateMatrix();
            Edge(now);
            break;
         case OP_WAIT:
            if (!Waiting)
            {
               Waiting = true;
               WaitUntil = now + o.Us;
            }
            if (now < WaitUntil) return;
            Waiting = false;
            break;
         case OP_PULSE:
         {
            if (!Pulsing)
            {
               Pulsing = true;
               PulseLine = o.Line;
               H.StartPulse(o.Line, o.Us);
            }
            uint64_t a, b;
            if (!H.PulseDone(&a, &b)) return;
            Pulsing = false;
            Edge(a);
            Edge(b);
            break;
         }
         case OP_VRESET_ON:
            VReset = true;
            UpdateReset();
            Edge(now);
            break;
         case OP_VRESET_OFF:
            VReset = false;
            UpdateReset();
            Edge(now);
            break;
         case OP_J1:
            J1Closed = o.Us != 0;
            H.SetJ1(J1Closed);
            break;
      }
      Pc++;
   }
}

void Engine::Finish()
{
   uint64_t now = H.NowUs();
   if (ActionMask)   // X releases whatever it left down
   {
      ActionMask = 0;
      UpdateMatrix();
      Edge(now);
   }
   Running = false;
   if (!HaveT0)
   {
      T0 = StartUs;
      T1 = now;
   }

   LineBuf b;
   if (AbortPending)
   {
      AbortPending = false;
      if (!AbortSilent) Err(RunName, "aborted");
      AbortSilent = false;
      for (; AReplies; AReplies--)
      {
         LineBuf a;
         a.Str("OK A");
         Ok(a);
      }
      return;
   }
   b.Str("OK ").Str(RunName);
   if (RunType == CmdType::Type) b.Str(" n=").U64(RunCount);
   else if (RunType == CmdType::Seq) b.Str(" steps=").U64(RunCount);
   else if (RunType == CmdType::KawariSet) b.Str(" j1=").Str(J1Closed ? "closed" : "open");
   b.Str(" t0=").U64(T0).Str(" t1=").U64(T1);
   Ok(b);
}

bool Engine::AbortAction(bool FromA, bool Silent)
{
   if (!Running) return false;
   if (Pc < NumOps && Ops[Pc].Critical)
   {
      AbortPending = true;
      if (Silent) AbortSilent = true;
      if (FromA) AReplies++;
      return true;
   }
   if (RunType == CmdType::KawariSet && J1Closed != J1Before)
   {
      // Stopped before its CRESET_N pulse: the Kawari never read the new J1,
      // so put it back, or a later reload would switch the standard unasked
      J1Closed = J1Before;
      H.SetJ1(J1Closed);
   }
   if (Pulsing)
   {
      H.StopPulse();
      Pulsing = false;
   }
   Waiting = false;
   if (ActionMask)
   {
      ActionMask = 0;
      UpdateMatrix();
   }
   if (VReset)
   {
      VReset = false;
      UpdateReset();
   }
   Running = false;
   if (!Silent) Err(RunName, "aborted");
   return false;
}

void Engine::Poll()
{
   if (Running) Step();

   uint64_t now = H.NowUs();
   if (HoldMask && now >= NextHold)
   {
      // NextHold is a lower bound: K u and A never raise it, so a scan here
      // may find nothing due yet.  It leaves NextHold exact.
      uint64_t expired = 0;
      NextHold = ~(uint64_t)0;
      for (int k = 0; k < 64; k++)
      {
         if (!(HoldMask & KeyBit((uint8_t)k))) continue;
         if (now >= HoldDeadline[k]) expired |= KeyBit((uint8_t)k);
         else if (HoldDeadline[k] < NextHold) NextHold = HoldDeadline[k];
      }
      if (expired)
      {
         HoldMask &= ~expired;
         UpdateMatrix();
         LineBuf b;
         b.Str("EV K ").Keys(expired).Str(" timeout");   // sent whatever E says
         H.WriteLine(b.C());
      }
   }
   if (ResetHeld && now >= ResetDeadline)
   {
      ResetHeld = false;
      UpdateReset();
      H.WriteLine("EV R timeout");
   }
}

void Engine::PhysTodToggle()
{
   if (!(Fit & FIT_TOD))
   {
      Event("f9 tod notfitted");
      return;
   }
   Tod = (Tod == TOD_50) ? TOD_60 : TOD_50;
   H.SetTod(Tod);
   LineBuf b;
   b.Str("f9 tod=");
   TodField(b);
   Event(b.C());
}
