#include "Fmt.h"
#include <string.h>

LineBuf &LineBuf::Ch(char c)
{
   if (N < Cap) B[N++] = c;
   return *this;
}

LineBuf &LineBuf::Str(const char *s)
{
   while (*s) Ch(*s++);
   return *this;
}

LineBuf &LineBuf::U64(uint64_t v)
{
   char d[21];
   int i = 0;
   do { d[i++] = (char)('0' + v % 10); v /= 10; } while (v);
   while (i) Ch(d[--i]);
   return *this;
}

LineBuf &LineBuf::Key(uint8_t Key)
{
   return Ch((char)('0' + ((Key >> 3) & 7))).Ch((char)('0' + (Key & 7)));
}

LineBuf &LineBuf::Keys(uint64_t Mask)
{
   if (!Mask) return Ch('-');
   bool first = true;
   for (uint8_t k = 0; k < 64; k++)
   {
      if (!(Mask & ((uint64_t)1 << k))) continue;
      if (!first) Ch('+');
      Key(k);
      first = false;
   }
   return *this;
}

static bool Digit(char c) { return c >= '0' && c <= '9'; }

bool FormatBuild(const char *Date, const char *Time, char *Out, int OutCap)
{
   static const char Months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
   if (OutCap < 20) { if (OutCap > 0) Out[0] = 0; return false; }
   // Date: "Mmm dd yyyy" with the day space-padded; Time: "hh:mm:ss"
   bool ok = Date && Time && strlen(Date) == 11 && strlen(Time) == 8 &&
             (Date[4] == ' ' || Digit(Date[4])) && Digit(Date[5]) &&
             Digit(Date[7]) && Digit(Date[8]) && Digit(Date[9]) && Digit(Date[10]) &&
             Digit(Time[0]) && Digit(Time[1]) && Time[2] == ':' &&
             Digit(Time[3]) && Digit(Time[4]) && Time[5] == ':' &&
             Digit(Time[6]) && Digit(Time[7]);
   int month = 0;
   if (ok)
   {
      for (int m = 0; m < 12; m++)
      {
         if (strncmp(Date, Months + m * 3, 3) == 0) { month = m + 1; break; }
      }
      ok = month != 0;
   }
   if (!ok)
   {
      strncpy(Out, "unknown", (size_t)OutCap);
      Out[OutCap - 1] = 0;
      return false;
   }
   char *o = Out;
   memcpy(o, Date + 7, 4); o += 4;
   *o++ = '-';
   *o++ = (char)('0' + month / 10);
   *o++ = (char)('0' + month % 10);
   *o++ = '-';
   *o++ = Date[4] == ' ' ? '0' : Date[4];
   *o++ = Date[5];
   *o++ = 'T';
   memcpy(o, Time, 8); o += 8;
   *o = 0;
   return true;
}
