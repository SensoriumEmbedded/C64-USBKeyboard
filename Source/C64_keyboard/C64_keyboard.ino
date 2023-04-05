

/*
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

static volatile uint8_t KeyCode,mod, modbuff;
static volatile bool capslock=false,KeyIsPressed=false; 

void setup() {
  
  usb.begin();

  // initialize the pins
  for ( int currentPin = ANALOG_SW_ARRAY_START; currentPin <= ANALOG_SW_ARRAY_END; ++currentPin) pinMode( currentPin, OUTPUT);
  pinMode( ANALOG_SW_STROBE, OUTPUT);// MT88XX strobe
  pinMode( ANALOG_SW_DATA, OUTPUT);  // MT88XX data
  pinMode( ANALOG_SW_RESET, OUTPUT); // MT88XX reset
  pinMode( NMI_PIN, OUTPUT);         // C64 NMI (RESTORE)
  pinMode(INDICATOR_LED, OUTPUT);
  digitalWrite(INDICATOR_LED, HIGH);
  
  resetMT88();

  keyboard.attachPress(keyPressed);
  keyboard.attachRelease(keyReleased);
  
  Serial.begin(9600);
  Serial.println("\n\nUSB Keyboard -> Teensy3.6 -> MT8808 -> C64 keyboard input");
  digitalWrite(INDICATOR_LED, LOW);
}

void loop() 
{  
   usb.Task(); // Process USB tasks
}

void keyPressed() {
  digitalWrite(INDICATOR_LED, HIGH);
  KeyCode = keyboard.getOemKey();
  mod = keyboard.getModifiers();
  KeyIsPressed = true;
  Serial.print("Press==");
  ConvToC64Key();
  //digitalWrite(INDICATOR_LED, LOW);
}

void keyReleased() {
  //digitalWrite(INDICATOR_LED, HIGH);
  KeyCode = keyboard.getOemKey();
  mod = keyboard.getModifiers();
  KeyIsPressed = false;
  Serial.print("Release");
  ConvToC64Key();
  digitalWrite(INDICATOR_LED, LOW);
}

void resetMT88(void) 
{
  digitalWrite(ANALOG_SW_DATA  ,  LOW);
  digitalWrite(ANALOG_SW_RESET , HIGH);
  digitalWrite(ANALOG_SW_STROBE, HIGH);
  digitalWrite(ANALOG_SW_RESET ,  LOW);
  digitalWrite(ANALOG_SW_STROBE,  LOW);
  digitalWrite(ANALOG_SW_DATA  , HIGH);
  capslock = false;
  modbuff =0;
  KeyIsPressed=false;
}

void setswitch(uint8_t C64KeyMap,bool KeyState){
  bool BitState;
  digitalWrite(ANALOG_SW_DATA , KeyState);
    
  if (C64KeyMap < 64) {
    //set the address:
    for ( int currentPin = ANALOG_SW_ARRAY_START; currentPin <= ANALOG_SW_ARRAY_END; ++currentPin) 
    {
      BitState = bitRead(C64KeyMap, (currentPin - ANALOG_SW_ARRAY_START));
      digitalWrite(currentPin, BitState);
    }
    digitalWrite( ANALOG_SW_STROBE, HIGH);
    digitalWrite( ANALOG_SW_STROBE, LOW);
  }
  
  // Reset switch state pin to default high state (Key press).
  digitalWrite(ANALOG_SW_DATA , HIGH);
  Serial.printf ("  *Switch: C64 KeyMap: %d, State: %d\n", C64KeyMap, KeyState);
}
      
void ConvToC64Key() 
{
   Serial.printf ("=> USB Code: %d=0x%X, mod: 0x%X, Press: %d\n", KeyCode, KeyCode, mod, KeyIsPressed);
 
   if (mod != modbuff) //process changed modifiers
   {
      for (int i=0;i < 8; i++) if (bitRead(modbuff,i) != bitRead(mod,i)) setswitch(MODKEYS[i], bitRead(mod,i));
      modbuff=mod; 
   }
    
   if (KeyCode == MT88XX_RESET && KeyIsPressed) 
   {
      resetMT88();  
      Serial.println("  *MT88XX Reset");
      return;
   }   
   
   if (KeyCode == RESTORE_KEY)
   {
     digitalWrite (NMI_PIN, KeyIsPressed);
     Serial.printf("  *Restore Key state: %d\n", KeyIsPressed);
     return;
   }
   
   if (KeyCode == CAPSLOCK_KEY && KeyIsPressed) 
   {
     capslock = !capslock;
     setswitch(C64KP_L_SHIFT, capslock);
     Serial.printf("  *Caps Lock state: %d\n", capslock);
     return;
   }

   // Key Process    
   if(KeyCode >= sizeof(KeyCodeToC64Map)) return; //out  of range?
   uint8_t C64KeyMap = KeyCodeToC64Map[KeyCode]; 
   if (C64KeyMap == C64KP_IGNORE) return; //ignore?
   
   if (C64KeyMap & SHIFT) //Special function to make sure key is shifted to c64
   {
      //if pressing and neither shift currently set, force l-shift
      if (KeyIsPressed && (mod & 0x22) == 0 && capslock == false) 
      {
         modbuff |= 0x02; //so it will get cleared next key up/dn
         setswitch(C64KP_L_SHIFT, 1);
      }
      C64KeyMap &= ~SHIFT; //clear shift bit for base button
   }
   
   setswitch(C64KeyMap,KeyIsPressed);
           
}

