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


//#ifndef C64keyboard_h
//#define C64keyboard_h


//MT88xx control pins
#define ANALOG_SW_DATA         0 // Sets selected cross switch on/off (Default 0)
#define ANALOG_SW_STROBE       2 // Strobe timing pulse (Default 2)
#define ANALOG_SW_RESET       10 // Reset all MT88XX cross switches to off (Default 10)
#define ANALOG_SW_ARRAY_START  3 // First pin of 6 bit switch addressing. AY2-0 & AX2-0 (Default 3)
#define ANALOG_SW_ARRAY_END    8 // Last pin of 6 bit switch addressing. AY2-0 & AX2-0 (Default 8)
#define ANALOG_SW_AX3          9 // Pin that controls AX3 (MT8812 & MT8816) (Default 9)
#define NMI_PIN                1 // NMI (RESTORE) KEY PIN (Default 1)
#define INDICATOR_LED         13 // LED on the Teensy

//C64 Key Pos Symbols:
#define C64KP_1               0x38  // 56
#define C64KP_2               0x3E  // 62
#define C64KP_3               0x20  // 32
#define C64KP_4               0x26  // 38
#define C64KP_5               0x10  // 16
#define C64KP_6               0x16  // 22
#define C64KP_7               0x30  // 48
#define C64KP_8               0x36  // 54
#define C64KP_9               0x08  // 8
#define C64KP_A               0x22  // 34
#define C64KP_B               0x31  // 49
#define C64KP_C               0x11  // 17
#define C64KP_D               0x12  // 18
#define C64KP_E               0x23  // 35
#define C64KP_F               0x15  // 21
#define C64KP_G               0x32  // 50
#define C64KP_H               0x35  // 53
#define C64KP_I               0x0C  // 12
#define C64KP_J               0x0A  // 10
#define C64KP_K               0x0D  // 13
#define C64KP_L               0x2A  // 42
#define C64KP_M               0x09  // 9
#define C64KP_N               0x0F  // 15
#define C64KP_O               0x0B  // 11
#define C64KP_P               0x2C  // 44
#define C64KP_Q               0x3B  // 59
#define C64KP_R               0x14  // 20
#define C64KP_S               0x25  // 37
#define C64KP_T               0x13  // 19
#define C64KP_U               0x33  // 51
#define C64KP_V               0x37  // 55
#define C64KP_W               0x24  // 36
#define C64KP_X               0x17  // 23
#define C64KP_Y               0x34  // 52
#define C64KP_Z               0x21  // 33
#define C64KP_AT              0x2B  // 43
#define C64KP_F1              0x01  // 1
#define C64KP_F3              0x05  // 5
#define C64KP_F5              0x03  // 3
#define C64KP_F7              0x06  // 6
#define C64KP_DEL             0x00  // 0
#define C64KP_RTN             0x04  // 4
#define C64KP_CTRL            0x3A  // 58
#define C64KP_HOME            0x1E  // 30
#define C64KP_PLUS            0x28  // 40
#define C64KP_ZERO            0x0E  // 14
#define C64KP_COLON           0x2D  // 45
#define C64KP_COMMA           0x2F  // 47
#define C64KP_EQUAL           0x1D  // 29
#define C64KP_MINUS           0x2E  // 46
#define C64KP_POUND           0x18  // 24
#define C64KP_SLASH           0x1F  // 31
#define C64KP_SPACE           0x39  // 57
#define C64KP_ARR_UP          0x1B  // 27
#define C64KP_PERIOD          0x29  // 41
#define C64KP_L_SHIFT         0x27  // 39
#define C64KP_R_SHIFT         0x19  // 25
#define C64KP_SEMICOL         0x1A  // 26
#define C64KP_ARR_LEFT        0x3C  // 60
#define C64KP_ASTERISK        0x1C  // 28
#define C64KP_HORZ_CSR        0x02  // 2
#define C64KP_RUN_STOP        0x3F  // 63
#define C64KP_VERT_CSR        0x07  // 7
#define C64KP_COMMODORE       0x3D  // 61

//special function c64 keymap codes/mods:
#define C64KP_IGNORE          0x80 // Dec 128: C64 Key map value to ignore key press
#define SHIFT                 0x40 // Dec 64: When bit set, special value that must be shift (f2, etc)

//mod keys to C64 key map:
const uint8_t MODKEYS[8] = 
{
   //USB Host mod bits 0-7: // bit x: USB mod key
   C64KP_CTRL,              // bit 0: L-Ctrl
   C64KP_L_SHIFT,           // bit 1: L-Shift
   C64KP_RUN_STOP,          // bit 2: L-Alt
   C64KP_COMMODORE,         // bit 3: L-Windows
   C64KP_CTRL,              // bit 4: R-Ctrl
   C64KP_R_SHIFT,           // bit 5: R-Shift
   C64KP_RUN_STOP,          // bit 6: R-Alt
   C64KP_COMMODORE          // bit 7: R-Windows
};

//Special function USB key codes:
#define RESTORE_KEY           0x2B // Dec 43: Tab acts as Restore key
#define CAPSLOCK_KEY          0x39 // Dec 57: CapsLock key function toggle
#define MT88XX_RESET          0x45 // Dec 69: F12 resets MT88XX 

//regular keys to C64 key map:
const uint8_t KeyCodeToC64Map[] = 
{
   C64KP_IGNORE           , //      0 0x00  Reserved
   C64KP_IGNORE           , //      1 0x01  Keyboard ErrorRollOver
   C64KP_IGNORE           , //      2 0x02  Keyboard POSTFail
   C64KP_IGNORE           , //      3 0x03  Keyboard ErrorUndefined
   C64KP_A                , //      4 0x04  Keyboard a and A
   C64KP_B                , //      5 0x05  Keyboard b and B
   C64KP_C                , //      6 0x06  Keyboard c and C
   C64KP_D                , //      7 0x07  Keyboard d and D
   C64KP_E                , //      8 0x08  Keyboard e and E
   C64KP_F                , //      9 0x09  Keyboard f and F
   C64KP_G                , //     10 0x0A  Keyboard g and G
   C64KP_H                , //     11 0x0B  Keyboard h and H
   C64KP_I                , //     12 0x0C  Keyboard i and I
   C64KP_J                , //     13 0x0D  Keyboard j and J
   C64KP_K                , //     14 0x0E  Keyboard k and K
   C64KP_L                , //     15 0x0F  Keyboard l and L
   C64KP_M                , //     16 0x10  Keyboard m and M
   C64KP_N                , //     17 0x11  Keyboard n and N
   C64KP_O                , //     18 0x12  Keyboard o and O
   C64KP_P                , //     19 0x13  Keyboard p and P
   C64KP_Q                , //     20 0x14  Keyboard q and Q
   C64KP_R                , //     21 0x15  Keyboard r and R
   C64KP_S                , //     22 0x16  Keyboard s and S
   C64KP_T                , //     23 0x17  Keyboard t and T
   C64KP_U                , //     24 0x18  Keyboard u and U
   C64KP_V                , //     25 0x19  Keyboard v and V
   C64KP_W                , //     26 0x1A  Keyboard w and W
   C64KP_X                , //     27 0x1B  Keyboard x and X
   C64KP_Y                , //     28 0x1C  Keyboard y and Y
   C64KP_Z                , //     29 0x1D  Keyboard z and Z
   C64KP_1                , //     30 0x1E  Keyboard 1 and !
   C64KP_2                , //     31 0x1F  Keyboard 2 and @
   C64KP_3                , //     32 0x20  Keyboard 3 and #
   C64KP_4                , //     33 0x21  Keyboard 4 and $
   C64KP_5                , //     34 0x22  Keyboard 5 and %
   C64KP_6                , //     35 0x23  Keyboard 6 and ^
   C64KP_7                , //     36 0x24  Keyboard 7 and &
   C64KP_8                , //     37 0x25  Keyboard 8 and *
   C64KP_9                , //     38 0x26  Keyboard 9 and (
   C64KP_ZERO             , //     39 0x27  Keyboard 0 and )
   C64KP_RTN              , //     40 0x28  Keyboard Return
   C64KP_IGNORE           , //     41 0x29  Keyboard Escape
   C64KP_DEL              , //     42 0x2A  Keyboard Delete (Backspace)
   C64KP_IGNORE           , //*    43 0x2B  Keyboard Tab
   C64KP_SPACE            , //     44 0x2C  Keyboard Spacebar
   C64KP_IGNORE           , //     45 0x2D  Keyboard - and _
   C64KP_EQUAL            , //     46 0x2E  Keyboard = and +
   C64KP_IGNORE           , //     47 0x2F  Keyboard [ and {
   C64KP_IGNORE           , //     48 0x30  Keyboard ] and }
   C64KP_POUND            , //     49 0x31  Keyboard \ and |
   C64KP_AT               , //     50 0x32  Keyboard Int' # and ~
   C64KP_SEMICOL          , //     51 0x33  Keyboard ; and :
   C64KP_COLON            , //     52 0x34  Keyboard ‘ and “
   C64KP_AT               , //     53 0x35  Keyboard Grave Accent and Tilde
   C64KP_COMMA            , //     54 0x36  Keyboard , and <
   C64KP_PERIOD           , //     55 0x37  Keyboard . and >
   C64KP_SLASH            , //     56 0x38  Keyboard / and ?
   C64KP_IGNORE           , //*    57 0x39  Keyboard Caps Lock
   C64KP_F1               , //     58 0x3A  Keyboard F1
   C64KP_F1+SHIFT         , //     59 0x3B  Keyboard F2
   C64KP_F3               , //     60 0x3C  Keyboard F3
   C64KP_F3+SHIFT         , //     61 0x3D  Keyboard F4
   C64KP_F5               , //     62 0x3E  Keyboard F5
   C64KP_F5+SHIFT         , //     63 0x3F  Keyboard F6
   C64KP_F7               , //     64 0x40  Keyboard F7
   C64KP_F7+SHIFT         , //     65 0x41  Keyboard F8
   C64KP_IGNORE           , //     66 0x42  Keyboard F9
   C64KP_IGNORE           , //     67 0x43  Keyboard F10
   C64KP_IGNORE           , //     68 0x44  Keyboard F11
   C64KP_IGNORE           , //*    69 0x45  Keyboard F12
   C64KP_IGNORE           , //     70 0x46  Keyboard PrintScreen
   C64KP_IGNORE           , //     71 0x47  Keyboard Scroll Lock
   C64KP_IGNORE           , //     72 0x48  Keyboard Pause
   C64KP_IGNORE           , //     73 0x49  Keyboard Insert
   C64KP_HOME             , //     74 0x4A  Keyboard Home
   C64KP_IGNORE           , //     75 0x4B  Keyboard PageUp
   C64KP_DEL              , //     76 0x4C  Keyboard Delete(forward)
   C64KP_IGNORE           , //     77 0x4D  Keyboard End
   C64KP_IGNORE           , //     78 0x4E  Keyboard PageDown
   C64KP_HORZ_CSR         , //     79 0x4F  Keyboard RightArrow
   C64KP_HORZ_CSR+SHIFT   , //     80 0x50  Keyboard LeftArrow
   C64KP_VERT_CSR         , //     81 0x51  Keyboard DownArrow
   C64KP_VERT_CSR+SHIFT   , //     82 0x52  Keyboard UpArrow
   C64KP_IGNORE           , //     83 0x53  Keypad Num Lock and Clear
   C64KP_IGNORE           , //     84 0x54  Keypad /
   C64KP_ASTERISK         , //     85 0x55  Keypad *
   C64KP_MINUS            , //     86 0x56  Keypad -
   C64KP_PLUS             , //     87 0x57  Keypad +
   C64KP_IGNORE           , //     88 0x58  Keypad ENTER
   C64KP_IGNORE           , //     89 0x59  Keypad 1 and End
   C64KP_IGNORE           , //     90 0x5A  Keypad 2 and Down Arrow
   C64KP_IGNORE           , //     91 0x5B  Keypad 3 and PageDn
   C64KP_ARR_LEFT         , //     92 0x5C  Keypad 4 and Left Arrow
   C64KP_IGNORE           , //     93 0x5D  Keypad 5
   C64KP_IGNORE           , //     94 0x5E  Keypad 6 and Right Arrow
   C64KP_IGNORE           , //     95 0x5F  Keypad 7 and Home
   C64KP_ARR_UP           , //     96 0x60  Keypad 8 and Up Arrow
   
   C64KP_IGNORE           , //     97 0x61  Keypad 9 and PageUp
   C64KP_IGNORE           , //     98 0x62  Keypad 0 and Insert
   C64KP_IGNORE           , //     99 0x63  Keypad . and Delete
   C64KP_IGNORE           , //    100 0x64  Keyboard Int \ and | 
   C64KP_IGNORE           , //    101 0x65  Keyboard Application (Menu)
   C64KP_IGNORE           , //    102 0x66  Keyboard Power[Notes 4]
   C64KP_IGNORE           , //    103 0x67  Keypad =
   C64KP_IGNORE           , //    104 0x68  Keyboard F13
   C64KP_IGNORE           , //    105 0x69  Keyboard F14
   C64KP_IGNORE           , //    106 0x6A  Keyboard F15
   C64KP_IGNORE           , //    107 0x6B  Keyboard F16
   C64KP_IGNORE           , //    108 0x6C  Keyboard F17
   C64KP_IGNORE           , //    109 0x6D  Keyboard F18
   C64KP_IGNORE           , //    110 0x6E  Keyboard F19
   C64KP_IGNORE           , //    111 0x6F  Keyboard F20
   C64KP_IGNORE           , //    112 0x70  Keyboard F21
   C64KP_IGNORE           , //    113 0x71  Keyboard F22
   C64KP_IGNORE           , //    114 0x72  Keyboard F23
   C64KP_IGNORE           , //    115 0x73  Keyboard F24
   C64KP_IGNORE           , //    116 0x74  Keyboard Execute
   C64KP_IGNORE           , //    117 0x75  Keyboard Help
   C64KP_IGNORE           , //    118 0x76  Keyboard Menu
   C64KP_IGNORE           , //    119 0x77  Keyboard Select
   C64KP_IGNORE           , //    120 0x78  Keyboard Stop
   C64KP_IGNORE           , //    121 0x79  Keyboard Again
   C64KP_IGNORE           , //    122 0x7A  Keyboard Undo
   C64KP_IGNORE           , //    123 0x7B  Keyboard Cut
   C64KP_IGNORE           , //    124 0x7C  Keyboard Copy
   C64KP_IGNORE           , //    125 0x7D  Keyboard Paste
   C64KP_IGNORE           , //    126 0x7E  Keyboard Find
   C64KP_IGNORE           , //    127 0x7F  Keyboard Mute
   C64KP_IGNORE           , //    128 0x80  Keyboard Volume Up
   C64KP_IGNORE           , //    129 0x81  Keyboard Volume Down
   C64KP_IGNORE           , //    130 0x82  Locking Caps Lock
   C64KP_IGNORE           , //    131 0x83  Locking Num Lock
   C64KP_IGNORE           , //    132 0x84  Locking Scroll Lock
   C64KP_IGNORE           , //    133 0x85  Keypad ,[Notes 9]
   C64KP_IGNORE           , //    134 0x86  Keypad Equals Sign
   C64KP_IGNORE           , //    135 0x87  Keyboard INT1
   C64KP_IGNORE           , //    136 0x88  Keyboard INT2
   C64KP_IGNORE           , //    137 0x89  Keyboard INT3
   C64KP_IGNORE           , //    138 0x8A  Keyboard INT4
   C64KP_IGNORE           , //    139 0x8B  Keyboard INT5
   C64KP_IGNORE           , //    140 0x8C  Keyboard INT6
   C64KP_IGNORE           , //    141 0x8D  Keyboard INT7
   C64KP_IGNORE           , //    142 0x8E  Keyboard INT8
   C64KP_IGNORE           , //    143 0x8F  Keyboard INT9
   C64KP_IGNORE           , //    144 0x90  Keyboard LANG1
   C64KP_IGNORE           , //    145 0x91  Keyboard LANG2
   C64KP_IGNORE           , //    146 0x92  Keyboard LANG3
   C64KP_IGNORE           , //    147 0x93  Keyboard LANG4
   C64KP_IGNORE           , //    148 0x94  Keyboard LANG5
   C64KP_IGNORE           , //    149 0x95  Keyboard LANG6
   C64KP_IGNORE           , //    150 0x96  Keyboard LANG7
   C64KP_IGNORE           , //    151 0x97  Keyboard LANG8
   C64KP_IGNORE           , //    152 0x98  Keyboard LANG9
   C64KP_IGNORE           , //    153 0x99  Keyboard Alternative Erase
   C64KP_IGNORE           , //    154 0x9A  Keyboard SysReq/Attention
   C64KP_IGNORE           , //    155 0x9B  Keyboard Cancel
   C64KP_IGNORE           , //    156 0x9C  Keyboard Clear
   C64KP_IGNORE           , //    157 0x9D  Keyboard Prior
   C64KP_IGNORE           , //    158 0x9E  Keyboard Return
   C64KP_IGNORE           , //    159 0x9F  Keyboard Separator
   C64KP_IGNORE           , //    160 0xA0  Keyboard Out
   C64KP_IGNORE           , //    161 0xA1  Keyboard Oper
   C64KP_IGNORE           , //    162 0xA2  Keyboard Clear/Again
   C64KP_IGNORE           , //    163 0xA3  Keyboard ClSel/Props
   C64KP_IGNORE           , //    164 0xA4  Keyboard ExSel
   C64KP_IGNORE           , //    165 0xA5  Reserved
   C64KP_IGNORE           , //    166 0xA6  Reserved
   C64KP_IGNORE           , //    167 0xA7  Reserved
   C64KP_IGNORE           , //    168 0xA8  Reserved
   C64KP_IGNORE           , //    169 0xA9  Reserved
   C64KP_IGNORE           , //    170 0xAA  Reserved
   C64KP_IGNORE           , //    171 0xAB  Reserved
   C64KP_IGNORE           , //    172 0xAC  Reserved
   C64KP_IGNORE           , //    173 0xAD  Reserved
   C64KP_IGNORE           , //    174 0xAE  Reserved
   C64KP_IGNORE           , //    175 0xAF  Reserved
   C64KP_IGNORE           , //    176 0xB0  Keypad 00
   C64KP_IGNORE           , //    177 0xB1  Keypad 000
   C64KP_IGNORE           , //    178 0xB2  Thousands Separator
   C64KP_IGNORE           , //    179 0xB3  Decimal Separator
   C64KP_IGNORE           , //    180 0xB4  Currency Unit
   C64KP_IGNORE           , //    181 0xB5  Currency Sub-unit
   C64KP_IGNORE           , //    182 0xB6  Keypad (
   C64KP_IGNORE           , //    183 0xB7  Keypad )
   C64KP_IGNORE           , //    184 0xB8  Keypad {
   C64KP_IGNORE           , //    185 0xB9  Keypad }
   C64KP_IGNORE           , //    186 0xBA  Keypad Tab
   C64KP_IGNORE           , //    187 0xBB  Keypad Backspace
   C64KP_IGNORE           , //    188 0xBC  Keypad A
   C64KP_IGNORE           , //    189 0xBD  Keypad B
   C64KP_IGNORE           , //    190 0xBE  Keypad C
   C64KP_IGNORE           , //    191 0xBF  Keypad D
   C64KP_IGNORE           , //    192 0xC0  Keypad E
   C64KP_IGNORE           , //    193 0xC1  Keypad F
   C64KP_IGNORE           , //    194 0xC2  Keypad XOR
   C64KP_IGNORE           , //    195 0xC3  Keypad ^
   C64KP_IGNORE           , //    196 0xC4  Keypad %
   C64KP_IGNORE           , //    197 0xC5  Keypad <
   C64KP_IGNORE           , //    198 0xC6  Keypad >
   C64KP_IGNORE           , //    199 0xC7  Keypad &
   C64KP_IGNORE           , //    200 0xC8  Keypad &&
   C64KP_IGNORE           , //    201 0xC9  Keypad |
   C64KP_IGNORE           , //    202 0xCA  Keypad ||
   C64KP_IGNORE           , //    203 0xCB  Keypad :
   C64KP_IGNORE           , //    204 0xCC  Keypad #
   C64KP_IGNORE           , //    205 0xCD  Keypad Space
   C64KP_IGNORE           , //    206 0xCE  Keypad @
   C64KP_IGNORE           , //    207 0xCF  Keypad !
   C64KP_IGNORE           , //    208 0xD0  Keypad Memory Store
   C64KP_IGNORE           , //    209 0xD1  Keypad Memory Recall
   C64KP_IGNORE           , //    210 0xD2  Keypad Memory Clear
   C64KP_IGNORE           , //    211 0xD3  Keypad Memory Add
   C64KP_IGNORE           , //    212 0xD4  Keypad Memory Subtract
   C64KP_IGNORE           , //    213 0xD5  Keypad Memory Multiply
   C64KP_IGNORE           , //    214 0xD6  Keypad Memory Divide
   C64KP_IGNORE           , //    215 0xD7  Keypad +/-
   C64KP_IGNORE           , //    216 0xD8  Keypad Clear
   C64KP_IGNORE           , //    217 0xD9  Keypad Clear Entry
   C64KP_IGNORE           , //    218 0xDA  Keypad Binary
   C64KP_IGNORE           , //    219 0xDB  Keypad Octal
   C64KP_IGNORE           , //    220 0xDC  Keypad Decimal
   C64KP_IGNORE           , //    221 0xDD  Keypad Hexadecimal
   C64KP_IGNORE           , //    222 0xDE  Reserved
   C64KP_IGNORE           , //    223 0xDF  Reserved
   C64KP_IGNORE           , //    224 0xE0  Keyboard Left Control
   C64KP_IGNORE           , //    225 0xE1  Keyboard Left Shift
   C64KP_IGNORE           , //    226 0xE2  Keyboard Left Alt
   C64KP_IGNORE           , //    227 0xE3  Keyboard Left GUI
   C64KP_IGNORE           , //    228 0xE4  Keyboard Right Control
   C64KP_IGNORE           , //    229 0xE5  Keyboard Right Shift
   C64KP_IGNORE           , //    230 0xE6  Keyboard Right Alt
   C64KP_IGNORE           , //    231 0xE7  Keyboard Right GUI
   C64KP_IGNORE           , //    232 0xE8  E8 to FFFF: Reserved
};

