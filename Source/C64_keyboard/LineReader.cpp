#include "LineReader.h"

LineReader::Result LineReader::Feed(uint8_t c, uint64_t NowUs)
{
   LastUs = NowUs;
   if (c == '\n')
   {
      Result r = Bad ? Junk : (Len ? Line : None);
      Buf[Len] = 0;
      Len = 0;
      Bad = false;
      Partial = false;
      return r;
   }
   Partial = true;
   if (c == '\r') return None;
   if (c < 0x20 || c > 0x7E || Len >= MaxLen)
   {
      Bad = true;
      return None;
   }
   Buf[Len++] = (char)c;
   return None;
}

void LineReader::Tick(uint64_t NowUs)
{
   if (Partial && NowUs - LastUs >= IdleUs) Reset();
}

void LineReader::Reset()
{
   Len = 0;
   Bad = false;
   Partial = false;
}
