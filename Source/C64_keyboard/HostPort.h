/*
  The serial side of the host protocol: whether a host has the port open, and
  the bytes it sends, into the Engine.  Bytes that arrive while no host has the
  port open are read and discarded, so nothing runs in a session that has
  already closed, or one that never asserted DTR.  No Arduino dependencies.
*/
#pragma once
#include <stdint.h>
#include "Engine.h"
#include "LineReader.h"

class HostPort
{
public:
   explicit HostPort(Engine &e) : E(e) {}

   void SetConnected(bool Connected);       // every loop, before the bytes: DTR, and the bus not suspended
   void Byte(uint8_t c, uint64_t NowUs);    // one byte read from the port
   void Tick(uint64_t NowUs);               // drops a partial line that has gone quiet

private:
   Engine    &E;
   LineReader Reader;
   bool       Conn = false;
};
