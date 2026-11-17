# Host Protocol

The text command protocol on the keyboard board's USB serial port. A host program (TReadmill, a terminal, a script) uses it to press C64 keys, type text, reset the C64, switch the Kawari's video standard, pulse the TeensyROM's buttons and set the TOD clock. This document is the contract: the firmware and every host are tested against it.

Protocol version: **1** (`proto=1` in the `?` reply).

## Connection

- USB CDC serial on the Teensy's micro-USB port. The baud rate is ignored, with one exception: never open the port at 134 baud, which reboots a Teensy into its bootloader.
- USB serial string `C64USBKeyboard-<chipid>`, with the chip id in decimal. Product string `C64 USB Keyboard`. Find the board by its serial string without opening any port.
- The host must assert DTR, which pyserial and most terminals do by default. When DTR drops (the host closes the port), or the USB bus suspends (the cable is pulled or the PC sleeps), the board releases everything the host holds (see "Safety"). Both count as a DTR drop below.
- Bytes that arrive while DTR is low, or the bus is suspended, are discarded, never run. A command written just before the port closes may be dropped; wait for its reply before closing.
- The board powers from the C64. Its port disappears whenever the C64 is off and appears within about a second of power-on.

## Lines

- Host to board: printable ASCII (0x20 to 0x7E), one command per line, ending in LF. CR is ignored. At most 120 characters, not counting the LF.
- A line longer than 120 characters, or holding any other byte, is dropped whole and answered with `ERR * syntax` when its LF arrives.
- A partial line is discarded with no reply after 500 ms with no new byte, or when DTR drops. TR tools send the bytes `dv` with no LF to every Teensy port they find; this rule drops them.
- An empty line is ignored with no reply. A host can send a bare LF to resync before `?`.
- Fields are separated by one or more spaces. Command names and keywords are case-insensitive.
- `S` and `ST` take their text from just after the one space that follows the command (for `ST`, the gap), exactly as sent, spaces included.

## Replies

- Every command line gets exactly one reply line: `OK <CMD> [fields]` or `ERR <CMD> <reason> [fields]`. Lines from the board end in LF.
- `<CMD>` is the command name in upper case (`?`, `TIME`, `K` and so on), or `*` when the line has no recognizable command.
- Reasons:

  | Reason | Meaning |
  |--------|---------|
  | `syntax` | Unknown command, bad keyword or bad number |
  | `range` | A value outside its range, a key outside 0-7, a character `S` can't type, a sequence too long |
  | `busy` | A timed action is running and this command would change an output |
  | `notfitted` | The command needs an output this board doesn't have marked fitted |
  | `aborted` | The command was stopped by `A` or a DTR drop |

- Fields are `key=value`, with no spaces in values. Hosts ignore fields they don't know, so new fields can be added without a new protocol version.
- Times are µs since the board started, as decimal integers (64-bit).

## Events

- Lines the board sends on its own start with `EV `. They are never written in the middle of a reply line. Hosts ignore `EV` lines they don't know.
- `EV <CMD> [keys] timeout`: a hold reached its maximum and was released, for example `EV K 17+12 timeout` or `EV R timeout`. Always sent.
- Everything else is a key or debug line from the USB keyboard. `E 0` silences these; it never silences `timeout` lines. `E` returns to 1 when DTR drops. They are for people reading a terminal, and their wording may change:

  | Line | When |
  |------|------|
  | `EV key 0x04 d`, `EV key 0x04 u` | A USB key (HID usage code) goes down or up |
  | `EV restore 1`, `EV restore 0` | TAB (RESTORE) goes down or up |
  | `EV caps 1` | Caps Lock latches SHIFT on or off |
  | `EV f9 tod=60` | F9 toggled TOD (`EV f9 tod notfitted` if it isn't fitted) |
  | `EV f10 menu pulse` | F10 pulsed the TR menu button (`EV f10 menu notfitted`) |
  | `EV f11 loopback` | F11 set the loopback diagonal |
  | `EV f12 mt8808-reset` | F12 reset the MT8808; host closures are restored |

## Keys

- A key is two digits `<pa><pb>`: the CIA1 port A bit ($DC00, 0 to 7), then the port B bit ($DC01, 0 to 7). Join several keys with `+`, up to 16: `17+12` is left SHIFT with A.
- Read as an octal number, `<pa><pb>` is the KERNAL's own key number, the value it keeps at `$CB` (64 means no key). A key table can be generated from, and checked against, the KERNAL's decode table at `$EB81`.
- RESTORE isn't in the matrix. It has its own command, `N`.

|     | PB0 | PB1 | PB2 | PB3 | PB4 | PB5 | PB6 | PB7 |
|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| PA0 | DEL | RETURN | CRSR ⇔ | F7 | F1 | F3 | F5 | CRSR ⇕ |
| PA1 | 3 | W | A | 4 | Z | S | E | L-SHIFT |
| PA2 | 5 | R | D | 6 | C | F | T | X |
| PA3 | 7 | Y | G | 8 | B | H | U | V |
| PA4 | 9 | I | J | 0 | M | K | O | N |
| PA5 | + | P | L | − | . | : | @ | , |
| PA6 | £ | * | ; | CLR/HOME | R-SHIFT | = | ↑ | / |
| PA7 | 1 | ← | CTRL | 2 | SPACE | C= | Q | RUN/STOP |

So RETURN is `01`, A is `12`, SPACE is `74`, C= is `75` and RUN/STOP is `77`.

## Timed actions and holds

- The timed actions are `P`, `S`, `ST`, `X`, `N`, `R <us>`, `B` and `V`. Each replies when its action has finished, with `t0` and `t1` taken at its first and last physical edges, not when the line was parsed or the reply sent. For `R` they are the assert and release of /RESET, so `t1-t0` is the pulse width.
- Holds (`K ... d`, `R hold`) and releases (`K ... u`, `R release`) reply as soon as the outputs have changed, with the time `t`. They aren't timed actions.
- Only one timed action runs at a time. While one runs, any command that would change an output gets `ERR <CMD> busy`. Queries (`?`, `TIME`, `T ?`, `V ?`, `F ?`), `E` and `A` are always answered.
- A switch closed by `K` stays closed until `K ... u`, `A`, its timeout or a DTR drop, whatever `P`, `S` or `X` do with the same switch. `P`, `S` and `X` release everything they pressed when they end or are aborted.
- Host closures are OR'd with the physical USB keyboard's. A switch is closed if either closes it, and the USB keyboard keeps working during a host session.
- When several switches change at once, SHIFT, CTRL and C= close before the other keys and open after them, microseconds apart. A keyboard scan already under way can still see a key without its modifier, so `S` closes modifiers 20 ms ahead of their key. To do the same with `P`, `K` or `X`, close the modifier first: `X d 17;w 20;d 12;w 50;u 12;w 20;u 17`.

## Safety

- A hold is released when it reaches its maximum (`max_ms`, default 10000), with an `EV ... timeout` line.
- When DTR drops, the board does everything `A` does, discards any partial line and sets `E` back to 1. Nothing from that session reaches the next one: an action it aborts sends no `ERR`, and no `OK A` it owed is sent.
- A hold that times out releases only that hold. It never aborts a timed action.
- `A` stops a running timed action, which replies `ERR <CMD> aborted` before `A` replies `OK A`.
- Once its CRESET_N pulse has started, `V` is never cut short. An `A` or a DTR drop after that keeps /RESET held until `restart_ms` after the pulse, then releases it, and `V` replies `ERR V aborted`, followed by one `OK A` per `A`. Releasing earlier would start the 6510 with PHI0 stopped while the Kawari reloads. A hold that times out during `V` can't release /RESET either, since `V` holds it itself.
- An abort in the 1 ms before the pulse stops `V` at once: J1 goes back to what it was before `V`, /RESET is released, and `V` replies `ERR V aborted`. The Kawari never reloaded, so its standard is unchanged.
- At power-on the matrix is open, every output is released, and TOD runs at 50 Hz if `tod` is fitted. The board presses nothing until a host or the USB keyboard tells it to.

## Fitted outputs

Every line wired off the PCB is optional, and a board drives only the ones marked fitted. The marks are stored in the board's EEPROM, survive power-off and firmware updates, and are set with `F`.

| Output | What it drives | Commands that need it |
|--------|----------------|-----------------------|
| `tod` | TOD clock into the CIAs (pin 25) | `T 50\|60\|off`, physical F9 |
| `menu` | TR menu button (pin 32) | `B menu`, `X` step `b menu`, physical F10 |
| `alt` | TR ALT button (pin 28) | `B alt`, `X` step `b alt` |
| `reset` | C64 /RESET through a transistor (pin 24) | `R`, `X` step `r`, `V` |
| `kawari` | Kawari J1 (pin 26) and CRESET_N (pin 27) | `V` (also needs `reset`) |

None is fitted on a new board. The matrix and RESTORE are on the PCB and always present.

`F <output> 0|1` writes flash only when the value changes. A write that needs a flash sector erased turns interrupts off for about 45 ms typically, and up to 400 ms. During that time the board stops answering, and the TOD clock misses edges. Send `F` while setting a board up, not during a test.

## Commands

Every time argument is optional; without one the default applies.

| Command | Action | Range | Default |
|---------|--------|-------|---------|
| `?` | Identity and state | | |
| `TIME` | Board clock | | |
| `K <keys> d [max_ms]` | Hold switches; several close together | max_ms 1 to 10000 | 10000 |
| `K <keys> u` | Release switches (already open is fine) | | |
| `P <keys> [hold_ms] [gap_ms]` | Close together, hold, open together, wait the gap | hold 1 to 10000, gap 0 to 10000 | 50, 50 |
| `S <text>` | Type text (see "Typed text") | total 60 s | 50, 50 per key |
| `ST <hold_ms> <gap_ms> <text>` | `S` with its own key timing | hold 20 to 10000, gap 20 to 10000 | |
| `N [ms]` | RESTORE pulse | 1 to 10000 | 50 |
| `A` | Release every host hold (`K` keys, `R hold`, `D on`) and abort a running action | | |
| `R [us]` | /RESET pulse | 10 to 10000000 | 50000 |
| `R hold [max_ms]` | Hold the C64 in reset | max_ms 1 to 10000 | 10000 |
| `R release` | Release a held reset | | |
| `B menu\|alt [ms]` | Pulse a TR button | 1 to 10000 | 50 |
| `T 50\|60\|off` | Set the TOD clock; the change lands on the next edge | | boots at 50 when fitted |
| `T ?` | TOD state | | |
| `V open\|closed [restart_ms]` | Kawari video standard (see "Kawari") | 10 to 10000 | 1000 |
| `V ?` | Kawari line state | | |
| `D on\|off` | Close or open the MT8808 loopback diagonal (the host's F11) | | |
| `X <step>;<step>;...` | Sequence (see "Sequences") | 16 steps, 10 s in total | |
| `E 0\|1` | Key and debug `EV` lines off or on | | 1 |
| `F <output> 0\|1` | Mark an output fitted or not, and save it | | none fitted |
| `F ?` | Fitted outputs | | |

### Replies

| Command | Reply |
|---------|-------|
| `?` | `OK ? proto=1 name=C64USBKeyboard fw=<x.y.z> build=<yyyy-mm-ddThh:mm:ss> chip=<decimal> mcu=teensy41 fitted=<list> held=<keys> reset=<0\|1\|-> tod=<50\|60\|off\|-> j1=<open\|closed\|-> loop=<on\|off> e=<0\|1>` |
| `TIME` | `OK TIME us=<µs>` |
| `K`, `R hold`, `R release` | `OK K t=<µs>`, `OK R t=<µs>` |
| `P`, `N`, `R <us>`, `B` | `OK <CMD> t0=<µs> t1=<µs>` |
| `S`, `ST` | `OK <CMD> n=<keys typed> t0=<µs> t1=<µs>` |
| `X` | `OK X steps=<n> t0=<µs> t1=<µs>` |
| `T`, `T ?` | `OK T tod=<50\|60\|off\|->` |
| `V open\|closed` | `OK V j1=<open\|closed> t0=<µs> t1=<µs>` |
| `V ?` | `OK V j1=<open\|closed\|-> creset=<released\|asserted\|->` |
| `D` | `OK D loop=<on\|off>` |
| `E` | `OK E e=<0\|1>` |
| `F`, `F ?` | `OK F fitted=<list>` |
| `A` | `OK A` |

A list is comma-separated (`fitted=tod,menu,reset`), keys are `+`-joined (`held=17+12`), and `-` means none or not fitted. Queries about an output that isn't fitted still answer `OK`, with `-` for its values. `held` lists the keys held by `K`.

### Typed text

`S` types each character with the key press that makes it on a C64. The whole line is checked before anything is typed: one character with no C64 key, or one unknown name, rejects it with `ERR S range`.

- Lowercase letters are the plain key and uppercase letters are SHIFT+key, as on the USB keyboard. In the C64's default character set `S load` shows LOAD, and `S LOAD` shows graphics characters.
- Digits, space and `+ - @ * : ; = , . /` are their own keys. `\` types £, `^` types ↑ and `_` types ←.
- With SHIFT: `!` `"` `#` `$` `%` `&` `'` `(` `)` are SHIFT+1 to SHIFT+9, `[` is SHIFT+`:`, `]` is SHIFT+`;`, `<` is SHIFT+`,`, `>` is SHIFT+`.` and `?` is SHIFT+`/`.
- `` ` ``, `|` and `~` have no C64 key. `{` and `}` only delimit names.
- The text runs to the end of the line, so `"` needs no escaping: `S load"*",8,1{return}`.
- Each key is held 50 ms with a 50 ms gap; `ST` sets other values, down to 20 ms each. A key must be seen down in one keyboard scan and up in another, and the KERNAL scans every 1/60 s, many programs once a frame (1/50 s on PAL). Shorter times can miss a key, or merge it with the same key typed next.
- SHIFT, CTRL and C= go down 20 ms before their key and up 20 ms after it, so no scan sees the key without them.
- That's about 10 keys a second, or 7 with a modifier. The KERNAL buffers only 10 keys, so text typed while the C64 isn't reading input beyond that is lost.

Names, in braces:

| Name | Key |
|------|-----|
| `{return}` | RETURN |
| `{home}`, `{clr}` | CLR/HOME, SHIFT+CLR/HOME |
| `{del}`, `{inst}` | INST/DEL, SHIFT+INST/DEL |
| `{down}`, `{up}` | CRSR ⇕, SHIFT+CRSR ⇕ |
| `{right}`, `{left}` | CRSR ⇔, SHIFT+CRSR ⇔ |
| `{f1}`, `{f3}`, `{f5}`, `{f7}` | F1, F3, F5, F7 |
| `{f2}`, `{f4}`, `{f6}`, `{f8}` | SHIFT+F1, SHIFT+F3, SHIFT+F5, SHIFT+F7 |
| `{stop}`, `{run}` | RUN/STOP, SHIFT+RUN/STOP |
| `{restore}` | A 50 ms RESTORE pulse |
| `{ctrl-<k>}`, `{c=-<k>}`, `{shift-<k>}` | CTRL, C= or SHIFT held with `<k>`: one character or a name without braces, as in `{ctrl-9}`, `{c=-1}`, `{shift-return}` |

### Sequences

`X` runs up to 16 steps, separated by `;`, as one action, so their relative timing doesn't depend on USB. The whole line is checked first, including that the waits and pulses add up to no more than 10 s.

| Step | Action |
|------|--------|
| `d <keys>` | Close switches |
| `u <keys>` | Open switches |
| `w <ms>` | Wait, 0 to 10000 |
| `n <ms>` | RESTORE pulse |
| `r <us>` | /RESET pulse (needs `reset`) |
| `b menu\|alt <ms>` | TR button pulse (needs the button fitted) |

Examples:

- RUN/STOP+RESTORE: `X d 77;w 50;n 50;w 50;u 77`
- Keys held through a reset: `X d 75;r 50000;w 2000;u 75`

### Kawari

`V open|closed` switches the Kawari's video standard without a power cycle:

1. Assert /RESET (`t0`).
2. Set J1: `open` releases it, which gives the Kawari's saved model; `closed` pulls it low, which gives the other standard.
3. Pulse CRESET_N low for 1 ms. The Kawari reloads from flash and reads J1 and its saved model.
4. Hold /RESET for `restart_ms` after the pulse, then release it (`t1`).

The J1 setting lasts until the C64 powers off. The board powers down with it, and J1 is released at the next power-on. To change the saved model, write the Kawari's registers with the persist flag ($40), wait for its busy bit ($10) to clear, then send `V open`. Pulsing CRESET_N during the Kawari's EEPROM write could leave its settings half-written.

## Example

```
> ?
< OK ? proto=1 name=C64USBKeyboard fw=1.0.0 build=2026-11-20T10:12:44 chip=14470230 mcu=teensy41 fitted=tod,menu,reset held=- reset=0 tod=50 j1=- loop=off e=1
> E 0
< OK E e=0
> R
< OK R t0=81234012 t1=81284019
> S load"$",8{return}
< OK S n=10 t0=83011200 t1=84081320
> K 77 d
< OK K t=85000004
> N
< OK N t0=85100010 t1=85150012
> K 77 u
< OK K t=85300020
> V closed
< ERR V notfitted
```

## Versioning

New commands, fields, `EV` kinds and names keep `proto=1`. A change that would break an existing host raises it, and a host refuses a `proto` it doesn't speak.
