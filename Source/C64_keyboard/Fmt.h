/*
  Small text helpers for host protocol replies.  No Arduino dependencies:
  64-bit numbers are formatted here because printf's %llu is not available
  in every Teensy C library build.
*/
#pragma once
#include <stdint.h>

class LineBuf
{
public:
   static const int Cap = 400;   // the longest line, ? with all 64 keys held, is about 370

   LineBuf &Str(const char *s);
   LineBuf &Ch(char c);
   LineBuf &U64(uint64_t v);
   LineBuf &Key(uint8_t Key);       // two octal digits: 077
   LineBuf &Keys(uint64_t Mask);    // "17+12" in key order, or "-" for none
   const char *C() { B[N] = 0; return B; }

private:
   char B[Cap + 1] = {};
   int  N = 0;
};

// __DATE__ ("Nov 16 2026") and __TIME__ ("14:41:00") to "2026-11-16T14:41:00".
// Returns false, with Out set to "unknown", if either is malformed.
bool FormatBuild(const char *Date, const char *Time, char *Out, int OutCap);
