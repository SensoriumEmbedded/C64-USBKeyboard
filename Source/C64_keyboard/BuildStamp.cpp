#include "BuildStamp.h"
#include "Fmt.h"
#include <string.h>

// Zeroed before the constructors run, so it needs no constructor of its own
static char Newest[20];   // "yyyy-mm-ddThh:mm:ss"

void BuildStamp_Note(const char *Date, const char *Time)
{
   char s[sizeof Newest];
   if (FormatBuild(Date, Time, s, sizeof s) && strcmp(s, Newest) > 0) memcpy(Newest, s, sizeof s);
}

const char *BuildStamp_Get(void)
{
   return Newest[0] ? Newest : "unknown";
}
