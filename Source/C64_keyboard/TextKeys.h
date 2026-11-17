/*
  Typed text for the host protocol's S command: each character, or {name},
  becomes one stroke, the key press that makes it on a C64.
  No Arduino dependencies, so the host tests compile it as is.
*/
#pragma once
#include <stdint.h>

enum : uint8_t
{
   MOD_SHIFT = 1,
   MOD_CTRL  = 2,
   MOD_CBM   = 4,
};

struct TextStroke
{
   uint8_t Key;      // key number (pa*8+pb); ignored for a RESTORE stroke
   uint8_t Mods;     // MOD_* held with the key
   bool    Restore;  // a RESTORE pulse instead of a key
};

// Converts text to strokes.  Returns the stroke count, or -1 when a character
// has no C64 key, a {name} is unknown or unterminated, or there are more than
// MaxStrokes; then *ErrPos is the offset of the offending character.
int TextToStrokes(const char *Text, TextStroke *Out, int MaxStrokes, int *ErrPos);
