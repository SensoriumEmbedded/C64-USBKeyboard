/*
  Typed text for the host protocol's S command.  See TextKeys.h and
  Docs/Host_Protocol.md ("Typed text").
*/
#include "TextKeys.h"
#include "BuildStamp.h"
#include "Keys.h"
#include "Fmt.h"
#include <string.h>

#define NOKEY 0xFF
#define SH    0x80   // stroke needs SHIFT

// ASCII 0x20..0x7E: key number, | SH for SHIFT, or NOKEY
static const uint8_t AsciiKeys[95] =
{
   /* ' ' */ 074,     /* !   */ 070 | SH, /* "   */ 073 | SH, /* #   */ 010 | SH,
   /* $   */ 013 | SH, /* %   */ 020 | SH, /* &   */ 023 | SH, /* '   */ 030 | SH,
   /* (   */ 033 | SH, /* )   */ 040 | SH, /* *   */ 061,     /* +   */ 050,
   /* ,   */ 057,     /* -   */ 053,     /* .   */ 054,     /* /   */ 067,
   /* 0   */ 043,     /* 1   */ 070,     /* 2   */ 073,     /* 3   */ 010,
   /* 4   */ 013,     /* 5   */ 020,     /* 6   */ 023,     /* 7   */ 030,
   /* 8   */ 033,     /* 9   */ 040,     /* :   */ 055,     /* ;   */ 062,
   /* <   */ 057 | SH, /* =   */ 065,     /* >   */ 054 | SH, /* ?   */ 067 | SH,
   /* @   */ 056,
   /* A-Z */ 012 | SH, 034 | SH, 024 | SH, 022 | SH, 016 | SH, 025 | SH, 032 | SH, 035 | SH,
             041 | SH, 042 | SH, 045 | SH, 052 | SH, 044 | SH, 047 | SH, 046 | SH, 051 | SH,
             076 | SH, 021 | SH, 015 | SH, 026 | SH, 036 | SH, 037 | SH, 011 | SH, 027 | SH,
             031 | SH, 014 | SH,
   /* [   */ 055 | SH, /* \ £ */ 060,     /* ]   */ 062 | SH, /* ^ ↑ */ 066,
   /* _ ← */ 071,     /* `   */ NOKEY,
   /* a-z */ 012, 034, 024, 022, 016, 025, 032, 035,
             041, 042, 045, 052, 044, 047, 046, 051,
             076, 021, 015, 026, 036, 037, 011, 027,
             031, 014,
   /* {   */ NOKEY,   /* |   */ NOKEY,   /* }   */ NOKEY,   /* ~   */ NOKEY,
};

struct KeyName
{
   const char *Name;
   uint8_t     Key;    // key number | SH, or NOKEY for RESTORE
};

static const KeyName KeyNames[] =
{
   { "return",  C64K_RETURN },
   { "home",    C64K_HOME },
   { "clr",     C64K_HOME | SH },
   { "del",     C64K_DEL },
   { "inst",    C64K_DEL | SH },
   { "down",    C64K_CRSR_UD },
   { "up",      C64K_CRSR_UD | SH },
   { "right",   C64K_CRSR_LR },
   { "left",    C64K_CRSR_LR | SH },
   { "f1",      C64K_F1 },
   { "f2",      C64K_F1 | SH },
   { "f3",      C64K_F3 },
   { "f4",      C64K_F3 | SH },
   { "f5",      C64K_F5 },
   { "f6",      C64K_F5 | SH },
   { "f7",      C64K_F7 },
   { "f8",      C64K_F7 | SH },
   { "stop",    C64K_RUNSTOP },
   { "run",     C64K_RUNSTOP | SH },
   { "restore", NOKEY },
};

static bool CharStroke(char c, TextStroke *Out)
{
   if (c < 0x20 || c > 0x7E) return false;
   uint8_t v = AsciiKeys[c - 0x20];
   if (v == NOKEY) return false;
   Out->Key = v & 63;
   Out->Mods = (v & SH) ? MOD_SHIFT : 0;
   Out->Restore = false;
   return true;
}

// A name without braces: "return", or a single character
static bool NameStroke(const char *Text, int Len, TextStroke *Out)
{
   if (Len == 1 && Text[0] != '{' && Text[0] != '}') return CharStroke(Text[0], Out);
   for (const KeyName &n : KeyNames)
   {
      if (!SameWord(Text, Len, n.Name)) continue;
      if (n.Key == NOKEY)
      {
         Out->Key = 0;
         Out->Mods = 0;
         Out->Restore = true;
      }
      else
      {
         Out->Key = n.Key & 63;
         Out->Mods = (n.Key & SH) ? MOD_SHIFT : 0;
         Out->Restore = false;
      }
      return true;
   }
   return false;
}

// The text between braces: a name, or a modifier and a name/character
static bool BracedStroke(const char *Text, int Len, TextStroke *Out)
{
   static const struct { const char *Prefix; uint8_t Mod; } Mods[] =
   {
      { "ctrl-", MOD_CTRL }, { "c=-", MOD_CBM }, { "shift-", MOD_SHIFT },
   };
   for (const auto &m : Mods)
   {
      int pl = (int)strlen(m.Prefix);
      if (Len > pl && SameWord(Text, pl, m.Prefix))
      {
         if (!NameStroke(Text + pl, Len - pl, Out) || Out->Restore) return false;
         Out->Mods |= m.Mod;
         return true;
      }
   }
   return NameStroke(Text, Len, Out);
}

int TextToStrokes(const char *Text, TextStroke *Out, int MaxStrokes, int *ErrPos)
{
   int n = 0;
   for (int i = 0; Text[i]; )
   {
      if (n >= MaxStrokes) { *ErrPos = i; return -1; }
      if (Text[i] == '{')
      {
         int end = i + 1;
         while (Text[end] && Text[end] != '}') end++;
         if (!Text[end] || !BracedStroke(Text + i + 1, end - i - 1, &Out[n])) { *ErrPos = i; return -1; }
         n++;
         i = end + 1;
         continue;
      }
      if (!CharStroke(Text[i], &Out[n])) { *ErrPos = i; return -1; }
      n++;
      i++;
   }
   return n;
}
