

/*
  C64 USB keyboard - Commodore USB Keyboard Interface

  Rewritten for Teensy 3.6/4.1 and added:
    Raw keypresss detection, Shift special keys, symbol mapping, clarity, & bug fixes
    Host protocol on the USB serial port (Docs/Host_Protocol.md)
    Copyright (c) 2023 Sensorium Embedded
    Written by Travis Smith

  Original code and C64 Key Pos Mapping:
    C64keyboard - Commodore Keyboard library
    Copyright (c) 2022 Hartland PC LLC
    Written by Robert VanHazinga

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include "C64keyboard.h"
#include <USBHost_t36.h>
#include <EEPROM.h>
#include <stdarg.h>
#include "Keys.h"
#include "Engine.h"
#include "HostPort.h"
#include "PhysKeys.h"
#include "Matrix.h"
#include "Outputs.h"
#include "Fitted.h"
#include "Fmt.h"
#include "ChipSerial.h"

#define KB_FW_VERSION "1.0.0"

USBHost usb;
USBHub hub1(usb);
KeyboardController keyboard(usb);
USBHIDParser hid1(usb);
USBHIDParser hid2(usb);

PhysKeys Phys;            //the USB keyboard's matrix closures, from the USB keys held
uint8_t PhysKeysDown=0;   //USB keyboard keys held, for the LED
bool MenuPulsing=false;   //F10's TR menu button pulse, ended from loop()
uint32_t MenuReleaseAt=0;

// USBHost_t36 calls the raw key handlers from its interrupt, so they only queue
// the event; loop() handles it, in the same context as the host protocol, which
// shares the matrix, the serial port and the TOD timer with it.
static const uint8_t KEYQ_LEN = 64;
static volatile uint16_t KeyQueue[KEYQ_LEN];   //keycode, | 0x100 when pressed
static volatile uint8_t KeyQHead = 0, KeyQTail = 0;
static volatile bool KeyQOverflow = false;

extern "C" uint32_t set_arm_clock(uint32_t frequency);

// --- host protocol ----------------------------------------------------------------

// The host has the port open: DTR, and on a Teensy 4 the bus not suspended.
// The core never clears DTR when the cable is pulled or the PC sleeps; the
// bus suspends then, so that counts as the host closing the port.
static bool HostConnected()
{
   if (!Serial.dtr()) return false;
#if defined(__IMXRT1062__)
   if (USB1_PORTSC1 & USB_PORTSC1_SUSP) return false;
#endif
   return true;
}

static uint8_t LoadFitted()
{
   uint8_t rec[FIT_RECORD_LEN];
   for (int i = 0; i < FIT_RECORD_LEN; i++) rec[i] = EEPROM.read(FIT_EEPROM_ADDR + i);
   uint8_t flags;
   return FitRecordDecode(rec, &flags) ? flags : 0;
}

class TeensyHal : public Hal
{
public:
   uint64_t NowUs() override { return Now64(); }
   void SetHostMatrix(uint64_t Mask) override { Matrix_SetHost(Mask); }
   void SetLine(OutLine Line, bool Asserted) override { Outputs_SetHost(Line, Asserted); }
   void StartPulse(OutLine Line, uint32_t Us) override { Outputs_StartPulse(Line, Us); }
   bool PulseDone(uint64_t *T0, uint64_t *T1) override { return Outputs_PulseDone(T0, T1); }
   void StopPulse() override { Outputs_StopPulse(); }
   void SetJ1(bool Closed) override { Outputs_SetJ1(Closed); }
   void SetTod(uint8_t Mode) override { Outputs_SetTod(Mode); }
   void ApplyFitted(uint8_t Flags) override { Outputs_ApplyFitted(Flags); }
   void SaveFitted(uint8_t Flags) override
   {
      // Only changed bytes are written; a flash erase can stall the board for up to 400 ms
      uint8_t rec[FIT_RECORD_LEN];
      FitRecordEncode(Flags, rec);
      for (int i = 0; i < FIT_RECORD_LEN; i++) EEPROM.update(FIT_EEPROM_ADDR + i, rec[i]);
   }
   void WriteLine(const char *Line) override
   {
      if (!HostConnected()) return;   // nobody listening
      Serial.print(Line);
      Serial.print('\n');
   }
   void Activity() override { Outputs_LedBlink(); }
};

static TeensyHal  TheHal;
static Identity   Id;
static Engine     Host(TheHal, Id);
static HostPort   Port(Host);
static char       BuildStr[24];

static void HostSerialTask()
{
   Port.SetConnected(HostConnected());   // a drop releases everything; bytes while closed are discarded
   uint64_t now = Now64();
   for (int n = 0; n < 64 && Serial.available() > 0; n++)   // a bounded batch, so usb.Task() keeps running
   {
      int c = Serial.read();
      if (c < 0) break;
      Port.Byte((uint8_t)c, now);
   }
   Port.Tick(now);
}

// A key or debug EV line, sent only while E is 1
static void PhysEvent(const char *Format, ...)
{
   if (!Host.EventsOn()) return;
   char buf[64];
   va_list args;
   va_start(args, Format);
   vsnprintf(buf, sizeof buf, Format, args);
   va_end(args);
   Host.Event(buf);
}

// USB host interrupt: queue the key event for loop()
static void QueueKey(uint8_t KeyCode, bool Pressed)
{
   uint8_t next = (uint8_t)((KeyQHead + 1) % KEYQ_LEN);
   if (next == KeyQTail)
   {
      KeyQOverflow = true;
      return;
   }
   KeyQueue[KeyQHead] = (uint16_t)(KeyCode | (Pressed ? 0x100 : 0));
   KeyQHead = next;
}

// Main loop: handle queued key events, and end F10's menu button pulse
static void KeyTask()
{
   while (KeyQTail != KeyQHead)
   {
      uint16_t e = KeyQueue[KeyQTail];
      KeyQTail = (uint8_t)((KeyQTail + 1) % KEYQ_LEN);
      bool pressed = (e & 0x100) != 0;
      if (pressed) PhysKeysDown++;
      else if (PhysKeysDown) PhysKeysDown--;
      ConvToC64Key((uint8_t)e, pressed);
   }
   if (KeyQOverflow)
   {
      // A lost release would leave a key stuck: drop every USB keyboard closure
      KeyQOverflow = false;
      Phys.Clear();
      Matrix_ResetChip();
      PhysKeysDown = 0;
      PhysEvent("key queue overflow, mt8808-reset");
   }
   if (MenuPulsing && (int32_t)(millis() - MenuReleaseAt) >= 0)
   {
      MenuPulsing = false;
      Outputs_SetPhys(OutLine::Menu, false);
   }
}

// --- setup and loop ---------------------------------------------------------------

void setup()
{
#if !defined(__IMXRT1062__)
   Matrix_EarlyInit();     //Teensy 4 does these in its boot hook (BootHook.cpp)
   Outputs_EarlyInit();
#endif
#ifdef ARDUINO_TEENSY41
   set_arm_clock(180000000);  //slow down Teensy 4.1 to match 3.6 default, otherwise too fast for 8808.
      //Could add delays to SetSwitch, but slower CPU runs cooler and uses less power
#endif
   Matrix_Begin();

   FormatBuild(__DATE__, __TIME__, BuildStr, sizeof BuildStr);
   Id.Fw = KB_FW_VERSION;
   Id.Build = BuildStr;
   Id.Chip = KB_ChipSerialNum();
   Id.Mcu = KB_MCU;
   Host.Begin(LoadFitted());

   usb.begin();
   keyboard.attachRawPress(OnRawPress);
   keyboard.attachRawRelease(OnRawRelease);

   Serial.begin(9600);   //USB CDC: the rate is ignored.  Never 134, which reboots into the bootloader.
   Outputs_LedUpdate(false);
}

void loop()
{
   usb.Task(); // Process USB tasks
   KeyTask();
   HostSerialTask();
   Host.Poll();
   Outputs_LedUpdate(PhysKeysDown > 0 || Host.Active());
}

// --- USB keyboard -----------------------------------------------------------------

void OnRawPress(uint8_t keycode)   //USB host interrupt
{
   QueueKey(keycode, true);
}

void OnRawRelease(uint8_t keycode)   //USB host interrupt
{
   QueueKey(keycode, false);
}

void ConvToC64Key(uint8_t KeyCode, bool KeyIsPressed)   //main loop, from KeyTask()
{
   PhysEvent("key 0x%02X %c", KeyCode, KeyIsPressed ? 'd' : 'u');

   // check for special USB scan codes
   if (KeyCode == F12_MT8808_RESET_KEY && KeyIsPressed)
   {
      Phys.Clear();         //keys still down stay open until pressed again
      Matrix_ResetChip();   //the host's closures stay
      PhysEvent("f12 mt8808-reset");
      return;
   }

   if (KeyCode == F11_LOOPBACK_KEY && KeyIsPressed)
   {
      Phys.Clear();
      Phys.Loopback();      //PAn to PBn: 00, 11, ... 77, until F12
      Matrix_ResetChip();
      Matrix_SetPhys(Phys.Mask());
      PhysEvent("f11 loopback");
      return;
   }

   if (KeyCode == F10_PULSE_OUTPUT && KeyIsPressed)
   {
      if (!(Host.Fitted() & FIT_MENU))
      {
         PhysEvent("f10 menu notfitted");
         return;
      }
      Outputs_SetPhys(OutLine::Menu, true);   //TeensyROM menu button, released 50 ms later by KeyTask()
      MenuPulsing = true;
      MenuReleaseAt = millis() + 50;
      PhysEvent("f10 menu pulse");
      return;
   }

   if (KeyCode == F9_TOD_50_60HzTOGGLE && KeyIsPressed)
   {
      Host.PhysTodToggle();   //toggle TOD output between 50 and 60Hz
      return;
   }

   if (KeyCode == TAB_RESTORE_KEY)
   {
     Outputs_SetPhys(OutLine::Restore, KeyIsPressed);
     PhysEvent("restore %d", KeyIsPressed);
     return;
   }

   if (KeyCode == CAPSLOCK_TOG_KEY && KeyIsPressed)
   {
     Phys.ToggleCaps();
     Matrix_SetPhys(Phys.Mask());
     PhysEvent("caps %d", Phys.Caps());
     return;
   }

   // Process Key: KeyCodeToC64Map, with SHIFT added for F2, cursor up/left and INST.
   // The matrix closes SHIFT before the key and opens it after.
   if (Phys.Key(KeyCode, KeyIsPressed)) Matrix_SetPhys(Phys.Mask());
}
