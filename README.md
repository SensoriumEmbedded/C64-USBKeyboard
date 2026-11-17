# C64-USBKeyboard

## USB keyboard interface for Commodore 64

**Original code and C64 Key Pos Mapping:**
* C64keyboard - Commodore Keyboard library
* Copyright (c) 2022 Hartland PC LLC, Written by Robert VanHazinga
    
    
**Rewritten/updated for Teensy 3.6/4.1:**
* Copyright (c) 2023 Sensorium Embedded, Written by Travis Smith
* Raw keypresss detection for improved emulation
* Ability to Shift special macro keys to c64 (F2/4/6/8, Cursor up/left, etc) 
* Diagnostics loopback emulation
* Symbol mapping for readability and easy editing
* Improved code clarity, fixed bugs
* PCB designed/added
* Host control over the Teensy's USB serial port: press keys, type text, reset the C64, switch a VIC-II Kawari's video standard, pulse TeensyROM buttons, set the TOD clock. See the **[host protocol](Docs/Host_Protocol.md)**.

PCB assembly instructions can be found **[here.](PCB/PCB_Assembly.md)**

**Optional wiring:** the TOD clock (pin 25), TeensyROM menu (pin 32) and ALT (pin 28) buttons, C64 /RESET (pin 24) and the Kawari lines (pins 26, 27) are wired off the PCB, and the board drives only the ones marked fitted. Nothing is marked on a new board, so a board already wired for TOD or the menu button needs it marked once, from any serial terminal on the Teensy's USB port: `F tod 1`, `F menu 1`. Until then F9 and F10 do nothing.

Host protocol tests run on a PC with Node.js and a C++ compiler: `node --test "Source/C64_keyboard/tests/*.test.mjs"`

Questions?  Send e-mail to [Travis @ Sensorium](mailto:travis@sensoriumembedded.com)

Stand-alone and in-system pictures below.  Note that I have sockets above and below the Teensy module in my build. Actual build per BOM using headers will make it sit much lower (similar to the MT8808 chip)
|![As-built](PCB/pics/As-built.jpg)|![In-System](PCB/pics/In-System.jpg)| 
|:--:|:--:|