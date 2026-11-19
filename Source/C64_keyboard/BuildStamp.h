/*
  The firmware's build stamp for `?`'s build=: the newest compile time among
  the source files that include this header, as "yyyy-mm-ddThh:mm:ss".
  Arduino and arduino-cli recompile only the files that changed, so one
  file's __DATE__/__TIME__ goes stale whenever another file is the one that
  changed.  Every source file in the sketch includes this header, and a
  constructor in each notes that file's compile time before setup() runs.
  On the PC (the protocol tests) nothing is noted.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void BuildStamp_Note(const char *Date, const char *Time);
const char *BuildStamp_Get(void);   // "unknown" until a file is noted

#ifdef __cplusplus
}
#endif

#if defined(TEENSYDUINO)
static void __attribute__((constructor, used)) BuildStamp_ThisFile(void)
{
   BuildStamp_Note(__DATE__, __TIME__);
}
#endif
