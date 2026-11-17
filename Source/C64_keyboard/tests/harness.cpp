// Host harness for the keyboard board's protocol engine.  Reads a script on
// stdin and prints what the board would send ("< ...") and every output change
// with its time ("= ... @µs").  Built and driven by host-protocol.test.mjs.
//
// Script lines:
//   boot <flags>        Engine.Begin with these FIT_* flags (first line)
//   > <text>            send a line (text plus LF)
//   >> <text>           send bytes with no LF
//   raw <hex> ...       send raw bytes
//   +<us>               advance the clock, polling every 100 µs
//   dtr0, dtr1          the host closes the port, or opens it (it starts open)
//   f9                  the USB keyboard's F9
//   usb <hex> d|u       a USB key down or up (PhysKeys); prints the closures
//   usb caps|f11|f12    CapsLock, loopback, MT8808 reset; prints the closures
//   order <hex> <hex>   the switch changes from one key mask to another, in order
//   ev <text>           a key/debug event line from the firmware
//   active              print whether the engine reports activity
//   fitrec <5 hex>      decode an EEPROM record
//   fitenc <flags>      encode one
//   build <date>|<time> format a build stamp
//   c64kp <hex>         C64KP_* code to key number
//   roundtrip           check every key number survives C64KP and back
//   strokes <text>      TextToStrokes
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../Engine.h"
#include "../HostPort.h"
#include "../PhysKeys.h"
#include "../Fitted.h"
#include "../Fmt.h"
#include "../Keys.h"
#include "../TextKeys.h"

static const char *LineNames[] = { "restore", "reset", "menu", "alt", "creset" };

class FakeHal : public Hal
{
public:
   uint64_t Now = 1000000;
   bool     Host[5] = {}, Pulse[5] = {}, Out[5] = {};
   bool     PulseOn = false, PulseDoneFlag = false;
   int      PulseIdx = 0;
   uint64_t PulseT0 = 0, PulseEnd = 0, PulseT1 = 0;

   void Log(const std::string &s, uint64_t t) { printf("= %s @%llu\n", s.c_str(), (unsigned long long)t); }
   void Drive(int i, uint64_t t)
   {
      bool o = Host[i] || Pulse[i];
      if (o == Out[i]) return;
      Out[i] = o;
      Log(std::string(LineNames[i]) + (o ? " 1" : " 0"), t);
   }
   void Expire()
   {
      if (!PulseOn || Now < PulseEnd) return;
      Pulse[PulseIdx] = false;
      PulseOn = false;
      PulseT1 = PulseEnd;
      PulseDoneFlag = true;
      Drive(PulseIdx, PulseEnd);
   }

   uint64_t NowUs() override { return Now; }
   void SetHostMatrix(uint64_t m) override
   {
      LineBuf b;
      b.Keys(m);
      Log(std::string("m ") + b.C(), Now);
   }
   void SetLine(OutLine l, bool a) override { Host[(int)l] = a; Drive((int)l, Now); }
   void StartPulse(OutLine l, uint32_t us) override
   {
      PulseIdx = (int)l;
      Pulse[PulseIdx] = true;
      PulseOn = true;
      PulseDoneFlag = false;
      PulseT0 = Now;
      PulseEnd = Now + us;
      Drive(PulseIdx, Now);
   }
   bool PulseDone(uint64_t *a, uint64_t *b) override
   {
      Expire();
      if (!PulseDoneFlag) return false;
      PulseDoneFlag = false;
      *a = PulseT0;
      *b = PulseT1;
      return true;
   }
   void StopPulse(uint64_t *a, uint64_t *b) override
   {
      if (PulseOn)
      {
         Pulse[PulseIdx] = false;
         PulseOn = false;
         PulseT1 = Now;
         Drive(PulseIdx, Now);
      }
      PulseDoneFlag = false;
      *a = PulseT0;
      *b = PulseT1;
   }
   void SetJ1(bool c) override { Log(c ? "j1 closed" : "j1 open", Now); }
   void SetTod(uint8_t m) override { Log(m == TOD_OFF ? "tod off" : (m == TOD_50 ? "tod 50" : "tod 60"), Now); }
   void ApplyFitted(uint8_t f) override
   {
      LineBuf b;
      FitList(b, f);
      Log(std::string("fitted ") + b.C(), Now);
   }
   void SaveFitted(uint8_t f) override
   {
      LineBuf b;
      FitList(b, f);
      Log(std::string("save ") + b.C(), Now);
   }
   void WriteLine(const char *s) override { printf("< %s\n", s); }
   void Activity() override {}
};

static FakeHal    TheHal;
static Identity   Id = { "1.0.0", "2026-01-02T03:04:05", 12345678, "teensy41" };
static Engine     Eng(TheHal, Id);
static HostPort   Port(Eng);
static PhysKeys   Phys;

static void Feed(uint8_t c)
{
   Port.Byte(c, TheHal.Now);
}

static void PrintKeys(const char *What, uint64_t Mask)
{
   LineBuf b;
   b.Keys(Mask);
   printf("%s %s\n", What, b.C());
}

static void Advance(uint64_t us)
{
   uint64_t end = TheHal.Now + us;
   while (TheHal.Now < end)
   {
      uint64_t step = end - TheHal.Now;
      if (step > 100) step = 100;
      if (TheHal.PulseOn && TheHal.PulseEnd > TheHal.Now && TheHal.PulseEnd < TheHal.Now + step)
         step = TheHal.PulseEnd - TheHal.Now;
      TheHal.Now += step;
      TheHal.Expire();
      Port.Tick(TheHal.Now);
      Eng.Poll();
   }
}

static bool Starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

int main()
{
   char line[4096];
   Port.SetConnected(true);
   while (fgets(line, sizeof line, stdin))
   {
      size_t n = strlen(line);
      while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
      if (!n) continue;

      if (Starts(line, "boot ")) Eng.Begin((uint8_t)strtoul(line + 5, nullptr, 0));
      else if (Starts(line, ">> ")) { for (const char *p = line + 3; *p; p++) Feed((uint8_t)*p); }
      else if (Starts(line, "> ") || strcmp(line, ">") == 0)
      {
         if (n > 2) for (const char *p = line + 2; *p; p++) Feed((uint8_t)*p);
         Feed('\n');
      }
      else if (Starts(line, "raw "))
      {
         char *p = line + 4;
         while (*p)
         {
            char *e;
            unsigned long v = strtoul(p, &e, 16);
            if (e == p) break;
            Feed((uint8_t)v);
            p = e;
         }
      }
      else if (line[0] == '+') Advance(strtoull(line + 1, nullptr, 10));
      else if (strcmp(line, "dtr0") == 0) Port.SetConnected(false);
      else if (strcmp(line, "dtr1") == 0) Port.SetConnected(true);
      else if (Starts(line, "usb "))
      {
         const char *a = line + 4;
         if (strcmp(a, "caps") == 0) Phys.ToggleCaps();
         else if (strcmp(a, "f11") == 0) { Phys.Clear(); Phys.Loopback(); }
         else if (strcmp(a, "f12") == 0) Phys.Clear();
         else
         {
            char *e;
            unsigned long code = strtoul(a, &e, 16);
            if (e == a || (strcmp(e, " d") != 0 && strcmp(e, " u") != 0)) return 2;
            if (!Phys.Key((uint8_t)code, e[1] == 'd')) printf("not a C64 key\n");
         }
         PrintKeys("phys", Phys.Mask());
      }
      else if (Starts(line, "order "))
      {
         char *p;
         uint64_t from = strtoull(line + 6, &p, 16);
         uint64_t to = strtoull(p, nullptr, 16);
         std::string s = "order";
         ForEachMatrixChange(from, to, [&s](uint8_t k, bool On) {
            LineBuf b;
            b.Key(k).Ch(On ? '+' : '-');
            s += std::string(" ") + b.C();
         });
         printf("%s\n", s.c_str());
      }
      else if (strcmp(line, "f9") == 0) Eng.PhysTodToggle();
      else if (Starts(line, "ev ")) Eng.Event(line + 3);
      else if (strcmp(line, "active") == 0) printf("active %d\n", Eng.Active() ? 1 : 0);
      else if (Starts(line, "fitrec "))
      {
         uint8_t rec[FIT_RECORD_LEN];
         char *p = line + 7;
         for (int i = 0; i < FIT_RECORD_LEN; i++) rec[i] = (uint8_t)strtoul(p, &p, 16);
         uint8_t f;
         if (FitRecordDecode(rec, &f))
         {
            LineBuf b;
            FitList(b, f);
            printf("fit %s\n", b.C());
         }
         else printf("fit defaults\n");
      }
      else if (Starts(line, "fitenc "))
      {
         uint8_t rec[FIT_RECORD_LEN];
         FitRecordEncode((uint8_t)strtoul(line + 7, nullptr, 0), rec);
         printf("rec");
         for (uint8_t b : rec) printf(" %02x", b);
         printf("\n");
      }
      else if (Starts(line, "build "))
      {
         char *bar = strchr(line, '|');
         if (!bar) return 2;
         *bar = 0;
         char out[32];
         bool ok = FormatBuild(line + 6, bar + 1, out, sizeof out);
         printf("build %s %s\n", ok ? "ok" : "bad", out);
      }
      else if (Starts(line, "c64kp "))
      {
         LineBuf b;
         b.Key(KeyNumFromC64KP((uint8_t)strtoul(line + 6, nullptr, 16)));
         printf("key %s\n", b.C());
      }
      else if (strcmp(line, "roundtrip") == 0)
      {
         bool ok = true;
         for (uint8_t k = 0; k < 64; k++)
         {
            if (KeyNumFromC64KP(C64KPFromKeyNum(k)) != k) ok = false;
            if (C64KPFromKeyNum(KeyNumFromC64KP(k)) != k) ok = false;
         }
         printf("roundtrip %s\n", ok ? "ok" : "fail");
      }
      else if (Starts(line, "strokes "))
      {
         TextStroke st[MAX_STROKES];
         int pos = -1;
         int count = TextToStrokes(line + 8, st, MAX_STROKES, &pos);
         if (count < 0) { printf("err %d\n", pos); continue; }
         for (int i = 0; i < count; i++)
         {
            LineBuf b;
            b.Key(st[i].Key);
            printf("%s%s%s%s%s", i ? " " : "", st[i].Restore ? "restore" : b.C(),
                   (st[i].Mods & MOD_SHIFT) ? "s" : "", (st[i].Mods & MOD_CTRL) ? "c" : "",
                   (st[i].Mods & MOD_CBM) ? "k" : "");
         }
         printf("\n");
      }
      else
      {
         printf("? bad script line: %s\n", line);
         return 1;
      }
   }
   return 0;
}
