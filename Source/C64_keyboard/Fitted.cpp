#include "Fitted.h"
#include "Fmt.h"

static const struct { const char *Name; uint8_t Bit; } FitNames[] =
{
   { "tod", FIT_TOD }, { "menu", FIT_MENU }, { "alt", FIT_ALT },
   { "reset", FIT_RESET }, { "kawari", FIT_KAWARI },
};

static uint8_t Check(const uint8_t *b) { return (uint8_t)(0x5A ^ b[0] ^ b[1] ^ b[2] ^ b[3]); }

void FitRecordEncode(uint8_t Flags, uint8_t Out[FIT_RECORD_LEN])
{
   Out[0] = 'K';
   Out[1] = 'B';
   Out[2] = FIT_RECORD_VERSION;
   Out[3] = Flags & FIT_ALL;
   Out[4] = Check(Out);
}

bool FitRecordDecode(const uint8_t In[FIT_RECORD_LEN], uint8_t *Flags)
{
   *Flags = 0;
   if (In[0] != 'K' || In[1] != 'B') return false;
   if (In[2] != FIT_RECORD_VERSION) return false;   // only version 1 exists so far
   if (In[4] != Check(In)) return false;
   if (In[3] & ~FIT_ALL) return false;
   *Flags = In[3];
   return true;
}

uint8_t FitFromName(const char *Name, int Len)
{
   for (const auto &f : FitNames)
   {
      if (SameWord(Name, Len, f.Name)) return f.Bit;
   }
   return 0;
}

void FitList(LineBuf &Out, uint8_t Flags)
{
   if (!(Flags & FIT_ALL)) { Out.Ch('-'); return; }
   bool first = true;
   for (const auto &f : FitNames)
   {
      if (!(Flags & f.Bit)) continue;
      if (!first) Out.Ch(',');
      Out.Str(f.Name);
      first = false;
   }
}
