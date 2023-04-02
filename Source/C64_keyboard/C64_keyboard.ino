

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

static volatile uint8_t kc,mod, modbuff;
static volatile bool capslock=false,kpress=false; 

void setup() {
  pinMode(INDICATOR_LED, OUTPUT); // show traffic
  digitalWrite(INDICATOR_LED, HIGH);
  
  usb.begin();

  // initialize the pins
  for ( int currentPin = ANALOG_SW_ARRAY_START; currentPin <= ANALOG_SW_ARRAY_END; ++currentPin) pinMode( currentPin, OUTPUT);
  pinMode( ANALOG_SW_STROBE, OUTPUT);  // MT88XX strobe
  pinMode( ANALOG_SW_DATA, OUTPUT);   // MT88XX data
  pinMode( ANALOG_SW_RESET, OUTPUT); // MT88XX reset
  pinMode(ANALOG_SW_DATA, OUTPUT);  //MT88XX data
  pinMode(NMI_PIN, OUTPUT); // C64 NMI
  
  //if (!MT8808){pinMode( ANALOG_SW_AX3, OUTPUT);}  // ANALOG_SW_AX3 (AX3) is separate as it is used only for conversion of X12/X13 into X6/X7
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
  kc = keyboard.getOemKey();
  mod = keyboard.getModifiers();
  kpress = true;
  Serial.print("Press==");
  c64key();
  //digitalWrite(INDICATOR_LED, LOW);
}

void keyReleased() {
  //digitalWrite(INDICATOR_LED, HIGH);
  kc = keyboard.getOemKey();
  mod = keyboard.getModifiers();
  kpress = false;
  Serial.print("Release");
  c64key();
  digitalWrite(INDICATOR_LED, LOW);
}

void resetMT88(void) 
{
  digitalWrite(ANALOG_SW_DATA , LOW);
  digitalWrite(ANALOG_SW_RESET, HIGH);
  digitalWrite( ANALOG_SW_STROBE, HIGH);
  digitalWrite(ANALOG_SW_RESET, LOW);
  digitalWrite( ANALOG_SW_STROBE, LOW);
  digitalWrite(ANALOG_SW_DATA , HIGH);
  capslock = false;
  modbuff =0;
  kpress=false;
}

void setswitch(uint8_t c,bool kstate){
  int bitr;
  bool state;
  digitalWrite(ANALOG_SW_DATA , kstate);
  // Fix logic table hole in MT8812/16
  // Convert x12 & x13 to x6 & x7. AX3 line control
  //if ( !MT8808 &&( (c > 55 && c < 64) || (c > 23 && c < 32 ))) {
  //   c -= 24;
  //  digitalWrite(ANALOG_SW_AX3, HIGH);
  //}
    
  if (c < 64) {
    //set the address:
    for ( int currentPin = ANALOG_SW_ARRAY_START; currentPin <= ANALOG_SW_ARRAY_END; ++currentPin) {
      bitr = (currentPin - ANALOG_SW_ARRAY_START);
      state =  bitRead(c, bitr);
      digitalWrite(currentPin, state);
    }
    digitalWrite( ANALOG_SW_STROBE, HIGH);
    digitalWrite( ANALOG_SW_STROBE, LOW);
  }
  //if (!MT8808){digitalWrite(ANALOG_SW_AX3, LOW);}
  
  // Reset switch state pin to default high state (Key press).
  digitalWrite(ANALOG_SW_DATA , HIGH);
  Serial.printf ("  *Switch: C64 Keycode: %d, State: %d\n", c, kstate);
}
      
void c64key() {
   
   Serial.printf ("=> USB Code: %d=0x%X, mod: 0x%X, Press: %d\n", kc, kc, mod, kpress);
 
   if (mod != modbuff) //process changed modifiers
   {
      for (int i=0;i < 8; i++) if (bitRead(modbuff,i) != bitRead(mod,i)) setswitch(MODKEYS[i], bitRead(mod,i));
      modbuff=mod; 
   }
    
   if (kc == MT88XX_RESET && kpress) 
   {
      resetMT88();  
      Serial.println("  *MT88XX Reset");
      return;
   }   
   
   if (kc == RESTORE_KEY)
   {
     digitalWrite (NMI_PIN, kpress);
     Serial.printf("  *Restore Key state: %d\n", kpress);
     return;
   }
   
   if (kc == CAPSLOCK_KEY && kpress) 
   {
     capslock = !capslock;
     setswitch(C64KP_L_SHIFT, capslock);
     Serial.printf("  *Caps Lock state: %d\n", capslock);
     return;
   }

   // Key Process    
   if(kc >= sizeof(KeyCodeToC64Map)) return; //out  of range?
   uint8_t c = KeyCodeToC64Map[kc]; 
   if (c == C64KP_IGNORE) return; //ignore?
   
   if (c & SHIFT) //Special function to make sure key is shifted to c64
   {
      //if pressing and neither shift currently set, force l-shift
      if (kpress && (mod & 0x22) == 0 && capslock == false) 
      {
         modbuff |= 0x02; //so it will get cleared next key up/dn
         setswitch(39, 1);
      }
      c &= ~SHIFT; //clear shift bit for base button
   }
   
   setswitch(c,kpress);
           
}

