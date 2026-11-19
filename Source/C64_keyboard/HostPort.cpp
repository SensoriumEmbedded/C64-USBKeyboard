#include "HostPort.h"
#include "BuildStamp.h"

void HostPort::SetConnected(bool Connected)
{
   if (Connected == Conn) return;
   Conn = Connected;
   Reader.Reset();
   if (!Connected) E.OnDtrDrop();
}

void HostPort::Byte(uint8_t c, uint64_t NowUs)
{
   if (!Conn) return;   // no session to run it in
   switch (Reader.Feed(c, NowUs))
   {
      case LineReader::Line: E.OnLine(Reader.Text()); break;
      case LineReader::Junk: E.OnJunkLine(); break;
      default: break;
   }
}

void HostPort::Tick(uint64_t NowUs)
{
   Reader.Tick(NowUs);
}
