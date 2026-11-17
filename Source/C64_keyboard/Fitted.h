/*
  Which optional outputs are wired on this board (F command), and how the
  choice is stored in EEPROM.  No Arduino dependencies.

  EEPROM record, FIT_RECORD_LEN bytes from FIT_EEPROM_ADDR:
    'K' 'B'   signature
    version   FIT_RECORD_VERSION
    flags     FIT_* bits
    check     0x5A ^ the four bytes before it
  A blank EEPROM (all 0xFF), another sketch's data, a newer version or a bad
  check byte all load the defaults: nothing fitted.
*/
#pragma once
#include <stdint.h>

enum : uint8_t
{
   FIT_TOD    = 1,
   FIT_MENU   = 2,
   FIT_ALT    = 4,
   FIT_RESET  = 8,
   FIT_KAWARI = 16,
   FIT_ALL    = 31,
};

static const int     FIT_EEPROM_ADDR    = 0;
static const int     FIT_RECORD_LEN     = 5;
static const uint8_t FIT_RECORD_VERSION = 1;

void FitRecordEncode(uint8_t Flags, uint8_t Out[FIT_RECORD_LEN]);
bool FitRecordDecode(const uint8_t In[FIT_RECORD_LEN], uint8_t *Flags);   // false: use the defaults (0)

// "tod", "menu", "alt", "reset", "kawari": the FIT_* bit, or 0 if unknown.  Case-insensitive.
uint8_t FitFromName(const char *Name, int Len);

class LineBuf;
void FitList(LineBuf &Out, uint8_t Flags);   // "tod,reset", or "-" for none
