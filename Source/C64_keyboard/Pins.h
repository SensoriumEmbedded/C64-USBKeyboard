/*
  Teensy pin mapping.  Pins 24-28 and 32 are wired off the PCB, and each is
  optional: the board drives only the outputs marked fitted (F command, see
  Docs/Host_Protocol.md).
*/
#pragma once
#include <stdint.h>

// On the PCB
#define MT8808_DATA_PIN        0 // Sets selected cross switch on/off
#define NMI_RESTORE_PIN        1 // NMI (RESTORE KEY) through Q1, high = pressed
#define MT8808_STROBE_PIN      2 // Strobe timing pulse (active high)
#define MT8808_RESET_PIN      10 // Reset all MT8808 cross switches to off (active high)
#define INDICATOR_LED_PIN     13 // LED on the Teensy

static const uint8_t MT8808_ADDRESS_PINS[] =
{
                               3,  // AY2
                               4,  // AY1
                               5,  // AY0
                               6,  // AX2
                               7,  // AX1
                               8,  // AX0
};

// Wired off the PCB, each optional
#define C64_RESET_OUT_PIN     24 // C64 /RESET through a 2N3904 like Q1, high = reset   (fitted: reset)
#define EXT_TOD_CLK_OUT_PIN   25 // 50/60Hz TOD clock output to inject into CIAs       (fitted: tod)
#define KAWARI_J1_PIN         26 // Kawari video standard switch J1, open-drain 3.3 V   (fitted: kawari)
#define KAWARI_CRESET_PIN     27 // Kawari FPGA CRESET_N, open-drain 3.3 V              (fitted: kawari)
#define EXT_ALT_OUT_PIN       28 // TeensyROM ALT button, open-drain 3.3 V              (fitted: alt)
#define EXT_PULSE_OUT_PIN     32 // TeensyROM menu button, open-drain 3.3 V             (fitted: menu)

#define IntervaluS_50Hz    10000 //  1/50/2*1000000
#define IntervaluS_60Hz     8333 //  1/60/2*1000000
