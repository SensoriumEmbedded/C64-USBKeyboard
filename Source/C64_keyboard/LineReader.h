/*
  Collects host protocol command lines from the serial byte stream.
  See Docs/Host_Protocol.md ("Lines").  No Arduino dependencies.
*/
#pragma once
#include <stdint.h>

class LineReader
{
public:
   static const int      MaxLen = 120;      // characters, not counting the LF
   static const uint32_t IdleUs = 500000;   // a partial line is dropped after this long with no byte

   enum Result : uint8_t
   {
      None,   // nothing complete yet, or an empty line (ignored)
      Line,   // a valid line is ready in Text()
      Junk,   // a line ended that was too long or held a non-printable byte
   };

   Result Feed(uint8_t c, uint64_t NowUs);
   void Tick(uint64_t NowUs);   // drops a partial line that has gone quiet
   void Reset();                // drops a partial line (DTR drop)
   const char *Text() const { return Buf; }   // valid after Feed() returns Line, until the next Feed()

private:
   char     Buf[MaxLen + 1] = {};
   uint16_t Len = 0;
   bool     Bad = false;
   bool     Partial = false;   // bytes received since the last LF
   uint64_t LastUs = 0;
};
