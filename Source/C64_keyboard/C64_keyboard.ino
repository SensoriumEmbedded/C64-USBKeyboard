

/*
  C64 USB keyboard - Commodore USB Keyboard Interface

  Rewritten for Teensy 3.6/4.1 and added:
    Raw keypresss detection, Shift special keys, symbol mapping, clarity, & bug fixes
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

USBHost usb;
KeyboardController keyboard(usb);

bool CapsLockState=false; 

void setup() 
{
   usb.begin();
   
   for ( uint8_t AddrBit = 0; AddrBit < 6; AddrBit++) pinMode(MT8808_ADDRESS_PINS[AddrBit], OUTPUT);
   pinMode(MT8808_STROBE_PIN, OUTPUT);
   pinMode(MT8808_DATA_PIN, OUTPUT);  
   pinMode(MT8808_RESET_PIN, OUTPUT); 
   pinMode(NMI_RESTORE_PIN, OUTPUT);  
   pinMode(INDICATOR_LED_PIN, OUTPUT);
   
   digitalWrite(NMI_RESTORE_PIN, LOW);
   digitalWrite(MT8808_STROBE_PIN, LOW);
   digitalWrite(INDICATOR_LED_PIN, HIGH);
   ResetMT8808();
   
   keyboard.attachRawPress(OnRawPress);
   keyboard.attachRawRelease(OnRawRelease);
   
   Serial.begin(9600);
   Serial.println("\n\nUSB Keyboard -> Teensy3.6 -> MT8808 -> C64 keyboard input");
   digitalWrite(INDICATOR_LED_PIN, LOW);
}

void loop() 
{  
   usb.Task(); // Process USB tasks
}

void OnRawPress(uint8_t keycode) 
{
   digitalWrite(INDICATOR_LED_PIN, HIGH);
   Serial.print("Press==");
   ConvToC64Key(keycode, true);
}

void OnRawRelease(uint8_t keycode) 
{
   Serial.print("Release");
   ConvToC64Key(keycode, false);
   digitalWrite(INDICATOR_LED_PIN, LOW);
}

void ResetMT8808(void) 
{
   digitalWrite(MT8808_RESET_PIN, HIGH);
   digitalWrite(MT8808_RESET_PIN,  LOW);
   CapsLockState = false;
}

void ConvToC64Key(uint8_t KeyCode, bool KeyIsPressed) 
{
   Serial.printf ("=> USB Code: %d=0x%X, Press: %d\n", KeyCode, KeyCode, KeyIsPressed);
 
   // check for special USB scan codes
   if (KeyCode == F12_MT8808_RESET_KEY && KeyIsPressed) 
   {
      ResetMT8808();  
      Serial.println("  *MT8808 Reset");
      return;
   }   
   
   if (KeyCode == TAB_RESTORE_KEY)
   {
     digitalWrite (NMI_RESTORE_PIN, KeyIsPressed);
     Serial.printf("  *Restore Key state: %d\n", KeyIsPressed);
     return;
   }
   
   if (KeyCode == CAPSLOCK_TOG_KEY && KeyIsPressed) 
   {
     CapsLockState = !CapsLockState;
     SetSwitch(C64KP_L_SHIFT, CapsLockState);
     Serial.printf("  *Caps Lock state: %d\n", CapsLockState);
     return;
   }

   // Process Key     
   if(KeyCode >= sizeof(KeyCodeToC64Map)) return; //out  of range?
   
   uint8_t C64KeyMap = KeyCodeToC64Map[KeyCode]; 
   if (C64KeyMap == C64KP_IGNORE) return; //ignore?
   
   if (C64KeyMap & SHIFT) //Special function to make sure key is shifted to c64
   {
      if (KeyIsPressed) SetSwitch(C64KP_R_SHIFT, 1); // press before...
      SetSwitch(C64KeyMap, KeyIsPressed); 
      if (!KeyIsPressed) SetSwitch(C64KP_R_SHIFT, 0); // ...release after
      return;
   }
   
   SetSwitch(C64KeyMap, KeyIsPressed); //normal case
}

void SetSwitch(uint8_t C64KeyMap, bool KeyState)
{
   C64KeyMap &= 63;  //strip upper bits
   
   digitalWrite(MT8808_DATA_PIN , KeyState);
   for ( uint8_t AddrBit = 0; AddrBit < 6; AddrBit++) digitalWrite(MT8808_ADDRESS_PINS[AddrBit], C64KeyMap & (1<<AddrBit));
   digitalWrite( MT8808_STROBE_PIN, HIGH);
   digitalWrite( MT8808_STROBE_PIN, LOW);
   
   Serial.printf ("  *Switch: C64 KeyMap: %d, State: %d\n", C64KeyMap, KeyState);
}
      

