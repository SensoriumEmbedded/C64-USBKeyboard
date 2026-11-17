# TReadmill Support Plan

How TReadmill (the TeensyROM hardware-in-the-loop test suite) will drive this board: keys over the Teensy's USB serial port, plus C64 /RESET, the VIC-II Kawari's video-standard lines, the TR buttons and the TOD clock. Requirements came from the TReadmill session on 2026-11-16. Nothing here is built yet.

## Summary

- Hand-wire a prototype on the v0.1 board: one transistor for C64 /RESET, two open-drain wires to the Kawari, and an optional wire to the TR's ALT button. Pins 25 (TOD) and 32 (TR menu button) stay as they are.
- Add a line-based text command protocol on the Teensy's USB serial port. The physical USB keyboard keeps working, and host key presses are OR'd with it.
- Keys go in as raw matrix positions (what TReadmill uses) or as typed text (`S`), for other host programs.
- Every output wired off the PCB (TOD, /RESET, Kawari, TR buttons) is optional. The board drives only the outputs marked fitted, and none is fitted by default.
- The prototype's Teensy is a 4.1, and the firmware targets it. The 3.6 keeps building.
- Move the wiring onto a v0.2 PCB once the prototype passes TReadmill's station tests.

## Decisions

| # | Decision | Answer |
|---|----------|--------|
| 1 | Matrix positions or characters | Both. TReadmill sends matrix positions and keeps its own PETSCII table. The board also types text (`S`) for other applications. |
| 2 | C128 support | Out of scope. C64 keyboard connector only. |
| 3 | Reset pulse resolution | µs, 10 µs to 10 s, default 50 ms |
| 4 | Spare outputs | TR Program pin: not needed. TR ALT button: nice to have. |
| 5 | TOD follows the video standard | No. The host sets it; a board with TOD fitted boots at 50 Hz. |
| 6 | Hardware | Hand-wired prototype on v0.1, v0.2 PCB later |
| 7 | USB names | Serial `C64USBKeyboard-<chipid>`, product `C64 USB Keyboard` |
| 8 | Off-PCB outputs | Optional per board, marked fitted in the board's EEPROM (`F`). None is fitted by default; the TOD and TR button wires aren't normally connected. |

## Bench checks before wiring

1. **Reminder: check that the board Teensy's VUSB–VIN pad is cut** ([PCB_Assembly.md](../PCB/PCB_Assembly.md), option 1). TReadmill holds the board's USB port open even while the C64 is off. With the pad uncut, the PC back-powers the C64 and the Kawari through the board. With it cut, the board's USB port disappears whenever the C64 is off. TReadmill treats that like the TR+ being off.
2. Find the Kawari's CRESET_N pad. Check continuity to the T20 FPGA's CRESET_N pin, which has a 3.3 V pull-up. It's probably one of the programming pads.
3. Identify the signal pin of the Kawari's J1 header (the video-standard switch), meaning the pin that isn't GND.
4. Once `V` works (step 5 under "Order of work"), measure the Kawari's restart time on the scope in both standards. Set the restart hold to about twice the longer one.

**Flashing with a TR attached.** The board and the TR+ are both Teensy 4.1s, so either hex loads onto either board without complaint (a 3.6 used to refuse). Pick the keyboard board's port by its serial string. With two Teensys attached and only `-port` given, `teensy_post_compile` can reboot the other one.

## Prototype wiring

| Function | Teensy pin | Circuit | Other end |
|----------|-----------:|---------|-----------|
| TOD clock (optional) | 25 | existing | CIA TOD inputs; not normally connected |
| TR menu button (optional) | 32 | existing, open-drain by pin mode | TR menu button; not normally connected |
| C64 /RESET | 24 | 10k to the base of a 2N3904, emitter to GND (a copy of Q1/R1 on RESTORE) | Collector to /RESET. User port pin 3 is the easiest point. |
| Kawari J1 | 26 | direct, `OUTPUT_OPENDRAIN` | J1 signal pin |
| Kawari CRESET_N | 27 | direct, `OUTPUT_OPENDRAIN` | CRESET_N pad |
| TR ALT button (optional) | 28 | direct, open-drain like pin 32 | TR ALT button; not normally connected |

- Pins 24–28 sit on the same header row as 25 and 32 on both the 3.6 and the 4.1, and the firmware doesn't use them.
- The Kawari and TR lines are direct wires at 3.3 V logic. The Teensy only pulls them low (open-drain), and the target's own 3.3 V pull-up sets the high level, so they never see more than 3.3 V. Never drive them high: a high output can back-power an unpowered part. A series resistor of a few hundred ohms is optional protection against a wiring mistake.
- /RESET goes through the transistor because the C64 pulls it up to 5 V, which the Teensy can't tolerate.
- Every line in this table except the matrix is optional. A board drives only the outputs marked fitted (see `F`).
- Outputs that may drive a transistor get the Teensy's internal pull-down whenever they're Hi-Z: RESTORE (pin 1), /RESET (pin 24) and TOD (pin 25, in case it's buffered by one). That covers boot until the firmware sets the pin up, and a board where the transistor is wired but not marked fitted. The transistor stays off in both cases.
- The open-drain lines (Kawari and TR buttons) get no pull-down. It would fight the target's own pull-up and could make the line read low. For J1 that means the wrong video standard.
- The firmware can't hold a pin before it runs, for example while the Teensy is being flashed with the C64 on. Add a 100k resistor from the /RESET transistor's base to GND so the C64 isn't reset then. Q1 on the PCB has no such resistor; add one in v0.2.
- The C64 keyboard connector has no /RESET pin. The schematic's `RESET` net is the MT8808's reset. So /RESET needs its own lead on any PCB revision.

## Firmware

All files go in `Source/C64_keyboard/`. Arduino builds every `.cpp` in the sketch folder.

| File | Contents |
|------|----------|
| `C64_keyboard.ino` | setup/loop and the USB host keyboard path, as now |
| `Matrix.cpp/.h` | MT8808 driver with a switch mask per source |
| `HostCmd.cpp/.h` | Line parser with no Arduino dependencies, so it can be tested on a PC |
| `TextKeys.cpp/.h` | Character and `{name}` to matrix key table for `S`, also free of Arduino dependencies |
| `LineReader.cpp/.h` | Serial line reader: length cap, junk, the 500 ms partial-line drop |
| `HostPort.cpp/.h` | The port's open/closed state and bytes into the Engine; bytes while closed are discarded |
| `PhysKeys.cpp/.h` | The USB keyboard's closures, worked out from the USB keys held, CapsLock and F11 |
| `Engine.cpp/.h` | Commands, timed actions, holds, timeouts, aborts and replies, over a `Hal` interface, so the host tests run it with a simulated clock |
| `Fitted.cpp/.h`, `Fmt.cpp/.h`, `Keys.h` | Fitted flags and their EEPROM record, reply formatting, key numbers |
| `Outputs.cpp/.h` | /RESET, Kawari, TR buttons, TOD, activity LED; open-drain helpers; the pulse timer; boot state |
| `UsbNames.c`, `ChipSerial.h`, `BootHook.cpp` | USB product and serial strings with the chip id; the Teensy 4 boot hook |
| `Pins.h` | Pin numbers |

As built (2026-11-17): `HostLink` and `Actions` became `LineReader` and `Engine`, and `Config.h` became `Pins.h`, with the protocol's defaults in `HostCmd.h`. After review, `HostPort` and `PhysKeys` were added.

- **Matrix merge.** Keep two 64-bit masks. The physical mask holds the USB keyboard, the Caps Lock latch and the F11 loopback. The host mask holds host commands. A switch is closed when either mask has it, and the MT8808 is written only when that OR changes. F12 clears the physical mask, resets the MT8808, then rewrites the host bits. RESTORE merges the same way: TAB key or host.
- **Timing.** `loop()` keeps calling `usb.Task()`. Key steps are in ms and run from loop timing. Edges that need µs accuracy, at the end of an `R`, `N` or `B` pulse or the CRESET_N pulse, end from a one-shot `IntervalTimer`, so loop latency can't stretch them. TOD keeps its own `IntervalTimer`.
- **Timestamps.** Keep a 64-bit µs clock extended from `micros()`, which wraps every ~71 minutes. Replies carry `t0` and `t1` from it.
- **Boot.** Reset the MT8808 and release every output (Hi-Z, with pull-downs per the wiring rules) in `startup_middle_hook`. That runs before the core's ~300 ms USB startup delay, so the matrix is open and the outputs are Hi-Z within milliseconds of power-on. Today `setup()` resets the MT8808 only after that delay. Check that GPIO is usable that early; if not, the next choice is `startup_late_hook`. If TOD is fitted, it starts at 50 Hz.
- **Safety.** A host hold releases after its maximum (default 10 s) or when DTR drops (`Serial.dtr()` goes false when the host closes the port). Both run the same release-all as `A`.
- **Debug output.** Today's Serial prints become `EV` lines, switched by `E 0|1`.
- **Activity LED.** The Teensy's LED (pin 13) lights for host commands as it does for the USB keyboard.
  - It stays on while anything is active: a physical key down, a host key held, RESTORE or /RESET held, or a timed action running (`P`, `S`, `X`, `N`, `R`, `B`, `V`).
  - A command that finishes at once (`?`, `TIME`, `T`, `F`, `E`, `D`, `K ... u`, `A`) blinks it for 50 ms so it's visible.
  - A rejected line (`ERR`) doesn't light it, so a blink means the board acted.
  - Today the LED goes off at any key release, even with another key still down. Counting what's active fixes that.
  - The PCB's own LED (D1) is wired to 3.3 V as a power light and stays as it is.
- **Fitted outputs.** Stored in the Teensy's EEPROM and set with `F`, so one firmware build serves every board. An output that isn't fitted stays Hi-Z, with the pull-down where the wiring rules above call for one. A host command for it gets `ERR notfitted`, the matching physical key (F9 for TOD, F10 for the TR menu button) does nothing, and `?` leaves it out of the fitted list.
- **One-time setup for boards already wired.** Today's firmware always runs TOD and always pulses pin 32 on F10. The new firmware starts with nothing fitted, so on a board that already has the TOD or TR menu wire, TOD stops and F10 does nothing after the update. To turn them back on, open the board's USB serial port in a terminal (Arduino Serial Monitor, PuTTY or TReadmill) and send `F tod 1` and `F menu 1`.
- **Where the flags are kept.** In the Teensy's EEPROM, so a change made with `F` survives power-off and later firmware updates. The record is a 2-byte signature, a layout version, the flags byte and a checksum. At boot the firmware loads the defaults (nothing fitted) when:
  - the signature doesn't match. That covers a blank EEPROM (every byte 0xFF, as on every board today) and data left by another sketch;
  - the version is newer than the firmware knows;
  - the checksum fails, as after a power loss in the middle of a write.

  A firmware that adds a field reads the older versions it knows and fills the new field with its default. Only `F` writes the record, so loading defaults never writes to the EEPROM. A 15-second Teensy restore wipes the EEPROM and puts the board back to defaults.
- **TOD.** `T` changes the interval with `IntervalTimer::update()`, which takes effect at the next period, so there's no runt pulse. `off` ends the timer and leaves the pin level steady.
- **Identity.** Adapt TeensyROM's `Source/Teensy/MinimalBoot/Common/UsbNames.c` (fills the strings from the chip id before `usb_init()`). The Teensy 3.6 has no `startup_middle_hook`, so it keeps PJRC's default strings unless a 3.6 board is ever put on a station.

## Host protocol (draft)

This draft is superseded by [Host_Protocol.md](Host_Protocol.md), the contract TReadmill builds and tests against. Where the two differ, `Host_Protocol.md` wins.

### Rules

- Every time argument is optional. Without one, a command uses the default in the table below: `R` alone is a 50 ms reset pulse.
- ASCII, one command per line, ending in `\n`; `\r` is ignored; at most 120 characters. An overlong line, or a line with bytes outside printable ASCII, is dropped with `ERR ? syntax`.
- A partial line is discarded with no reply when DTR drops, or after 500 ms with no byte (the TR's own inter-byte timeout). An empty line is ignored with no reply, so a host can send `\n` to resync before `?`. This matters because every TR flash, and TReadmill's port search, sends the TR's VersionInfo token `dv` (0x64 0x76: printable, no newline) to every Teensy port it finds.
- Every command gets exactly one reply: `OK <cmd> [fields]` or `ERR <cmd> <reason>`. The reasons are `syntax`, `range`, `busy`, `notfitted` and `aborted`.
- The timed actions are `P`, `S`, `ST`, `X`, `N`, `R <us>`, `B` and `V`. Each replies once its action has finished, with `t0=<µs> t1=<µs>` on the board's clock. Both are taken at the first and last physical edges (for `R`, assert and release; for `V`, /RESET assert and release), not at parse or reply. TReadmill's pulse-width sweep reads the width from `t1-t0`.
- Holds (`K ... d`, `R hold`) and releases reply at once; they aren't timed actions. So `R hold`, `K 75 d`, `R release` works, as a hold must when it waits on the screen or outlasts `X`'s 10 s.
- Only one timed action runs at a time. Any command that changes an output while one is running gets `ERR <cmd> busy`. `?`, `TIME`, `T ?`, `V ?` and `A` are always answered.
- `A` aborts a running action. The aborted command's `ERR <cmd> aborted` is sent before `OK A`.
- `V` is never cut short. An `A`, a DTR drop or a hold reaching its max during `V` keeps /RESET down until `restart_ms` after the CRESET_N pulse, then releases it and replies `ERR V aborted`. Releasing earlier would let the 6510 leave reset with PHI0 stopped while the FPGA reloads.
- `X` releases everything it pressed when it ends or is aborted. Open-ended holds use `K`.
- Lines the board sends unprompted start with `EV `, and are never written in the middle of a reply. A hold that reaches its max is released and reported with `EV <cmd> <keys> timeout`, whatever `E` says. `E 0` silences only key and debug lines. `E` returns to on when DTR drops, so the next terminal sees the lines; TReadmill sends `E 0` after it opens the port.
- A key is two digits `<pa><pb>`: the CIA1 port A bit ($DC00, 0–7), then the port B bit ($DC01, 0–7). For example RUN/STOP = `77`, A = `12`, left SHIFT = `17`, C= = `75`. Join several keys with `+`. These match the firmware's `C64KP_*` codes, whose X is the PA bit and Y the PB bit, with the 3-bit fields stored bit-reversed.
- Read as an octal number, `<pa><pb>` is the KERNAL's own key number, the `$CB` value: RETURN 1, A 10, SPACE 60, RUN/STOP 63, 64 for none. A key table can be generated from, and tested against, the KERNAL's decode table at `$EB81`.

### Commands

| Command | Action | Default |
|---------|--------|---------|
| `?` | `key=value` fields, no spaces in values: `proto=1`, name, firmware version, build, chip id (decimal, as in the serial string), Teensy model, fitted outputs. State: host keys held, reset, TOD, J1. A host refuses a `proto` it doesn't speak. | |
| `TIME` | Board µs clock | |
| `K <keys> d [max_ms]` / `K <keys> u` | Hold or release switches. Several keys close together. `max_ms` 1 to 10000. | max 10000 |
| `P <keys> [hold_ms] [gap_ms]` | Close the keys together, hold, open together, wait the gap | 50, 50 |
| `S <text>` | Type characters and `{names}` up to the end of the line (see "Typed text") | 50, 50 per key |
| `ST <hold_ms> <gap_ms> <text>` | `S` with its own key timing | |
| `N [ms]` | RESTORE pulse | 50 |
| `A` | Release everything the host holds (keys, RESTORE, /RESET, buttons); abort a running action | |
| `R [us]` | /RESET pulse, 10 to 10000000 µs | 50000 |
| `R hold [max_ms]` / `R release` | Hold the C64 in reset, or release it. `max_ms` 1 to 10000. | max 10000 |
| `B menu\|alt [ms]` | Pulse a TR button | 50 |
| `T 50\|60\|off\|?` | Set the TOD clock to 50 Hz, 60 Hz or stopped; `?` reports it. F9 still toggles 50/60. | boots at 50 when fitted |
| `V open\|closed [restart_ms]` | Kawari: assert /RESET, set J1, pulse CRESET_N for 1 ms, hold reset for `restart_ms` (10 to 10000), release. Needs both `kawari` and `reset` fitted. | 1000 |
| `V ?` | J1 and CRESET_N line state | |
| `D on\|off` | MT8808 loopback diagonal (the host's F11) | |
| `X <step>;<step>;...` | Sequence, up to 16 steps and 10 s in total | |
| `E 0\|1` | `EV` lines off or on | on |
| `F <output> 0\|1` / `F ?` | Mark an off-PCB output fitted or not: `tod`, `reset`, `kawari`, `menu`, `alt`. Stored in EEPROM. | none fitted |

Sequence steps are `d <keys>`, `u <keys>`, `w <ms>`, `n <ms>`, `r <us>` and `b menu|alt <ms>`. Examples:

- RUN/STOP+RESTORE: `X d 77;w 50;n 50;w 50;u 77`
- Keys held through a reset: `X d 75;r 50000;w 2000;u 75`

### Typed text (`S`)

`S` types each character with the key press that makes it on a C64, from the board's own table.

- Letters type as they would on the USB keyboard: lowercase is the plain key, uppercase is SHIFT+key. In the C64's default character set, `S load` shows LOAD on screen, and `S LOAD` shows graphics characters.
- Symbols type as their C64 key, with SHIFT where the C64 needs it: `"` is SHIFT+2, `$` is SHIFT+4, `[` is SHIFT+`:`. `*`, `+`, `=`, `@`, `:` and `;` are their own keys. `^` types ↑, `_` types ←, and `\` types £.
- Keys with no ASCII character go in braces: `{return}`, `{home}`, `{clr}`, `{del}`, `{inst}`, `{up}`, `{down}`, `{left}`, `{right}`, `{f1}` to `{f8}`, `{stop}`, `{restore}`. A modifier combines with one key: `{ctrl-9}`, `{c=-1}`, `{shift-a}`. The full list goes in `Host_Protocol.md`.
- A character with no C64 key (`` ` ``, `|`, `~`) or an unknown name rejects the whole line with `ERR S range`, before anything is typed.
- The text runs to the end of the line, so `"` needs no escaping: `S load"*",8,1{return}`.
- Each key is held 50 ms with a 50 ms gap (`ST` changes that), about 10 characters a second. The KERNAL buffers only 10 keys, so text typed while the C64 is busy beyond that is lost. `A` stops a long string.

## Kawari standard: what persists

- The Kawari's saved model (CHIP_MODEL) is in its own EEPROM and survives power-off. It's changed by register writes (TReadmill's register route).
- J1 stores nothing. The FPGA reads it once each time it configures: open gives the saved model, low gives the other standard. A jumper on J1 only seems persistent because it stays fitted.
- `V closed` holds J1 low from a Teensy pin. The Teensy loses power with the C64 (VIN pad cut), so at the next power-on J1 reads open and the Kawari comes up in its saved model.
- For a change that survives power-off, change the saved model by register, then run `V open`. Its CRESET_N pulse reloads the FPGA with the new saved model, so no mains cycle is needed.
- After writing the saved model with the persist flag ($40), the host waits for the Kawari's busy bit ($10) to clear before `V open`. A CRESET_N pulse during the Kawari's EEPROM write could leave its saved settings half-written. TReadmill does the waiting (register details from TReadmill's review).
- Not planned: the board remembering the last `V` and driving J1 at power-on. It would race the FPGA's configuration, and a board that boots into a hidden state is harder for TReadmill to trust. The host sets the standard each session anyway, as it does TOD.

## Tests

In this repo:

- Add a host-compiled parser harness and a `node:test` file in `Source/C64_keyboard/tests/`, as TeensyROM does in `Source/Teensy/tests/`. They cover every command, range limits, junk and TR binary tokens rejected, overlong lines dropped, a `dv` with no newline discarded after 500 ms, empty lines ignored, and the reply order on abort. They also cover the `S` table: every printable ASCII character and every `{name}`. Arduino compiles only the sketch folder and `src/`, so `tests/` stays out of the firmware build.

On a TReadmill station (tier 1, from TReadmill's list):

- identity
- matrix walk: all 64 positions plus RESTORE, read by a CIA-scanning C64 program
- KERNAL typing, and the menu tests with board keys
- combos
- NMI count
- reset: back to the menu, pulse-width sweep, hold and release
- TOD rate at 50, 60 and off
- Kawari both ways, plus a 20-switch soak
- TR menu button
- safety: port closed mid-hold, TR tokens
- physical keyboard during a session

## Work on the TReadmill side

- Move `[keys]` from the rig to the station, keyed by the board's chip id in the station files, instead of by port.
- Add `treadmill/station/keys_hw.py` on `open_serial()`, and derive capabilities from `?` instead of config (`treadmill/station/model.py` `capabilities()`):

  | Board reports | TReadmill capability |
  |---|---|
  | board present (matrix and RESTORE are on the PCB) | `keys_hw`, `keys_kernal`, `nmi` |
  | `tod` | `tod_select` |
  | `menu` | `tr_button` |
  | `alt` | `tr_alt` |
  | `reset` | `ext_reset`, also used by mfg's `ExternalReset` and the recovery ladder |
  | `kawari` and `reset` | `region_switch` (with a switchable machine) |

  A station whose board was never marked fails safe: its tests skip, and the skip reason names the `F <output> 1` to send. TReadmill only reads `?` and `F ?`; it never writes `F`.
- Skip `C64USBKeyboard-*` serial strings in `find_port()`'s fallback (`treadmill/link/discover.py`), matched case-insensitively. Never probe the board with TR tokens or 134 baud.
- Map mfg's `ExternalReset` (`treadmill/mfg.py`) to `R` with its default. `kawari.py` re-applies `V` after every power cycle, since `V` doesn't persist. Set TOD at session start.
- Add a fake board in `selftest/`, and a test pinned to `Docs/Host_Protocol.md`.
- Add the board-less open (BACKLOG item 13).
- Extend the rig lock to cover the keyboard port.

## Corrections sent to TReadmill (all accepted, 2026-11-16)

- The board can't sense the matrix. `D` only closes the loopback diagonal, and a C64 program or the diag cart reads it. There's no "MT8808 fault" the board could report.
- On a board with TOD fitted, TOD isn't idle at boot. It runs at 50 Hz because it's the C64's TOD source. Everything else boots released.
- Keys are addressed as `<pa><pb>` CIA1 bits, not row/column.
- A `V` setting lasts until the C64 powers off. With the VIN pad cut, the board powers down with the C64, and J1 is Hi-Z again (the saved model) at the next power-on.
- Changing the saved model doesn't need a mains cycle. After the register write, `V open` reloads the FPGA through CRESET_N.
- The PCB has no /RESET or Kawari circuitry, and the TR button wires aren't normally connected. `?` reports what's fitted from the board's `F` settings, and nothing is fitted by default. A station board needs each wired line marked once with `F`.
- The board also types text (`S`). TReadmill can keep matrix positions and its own table, and use `S` where plain typing is enough.

## Order of work

1. Bench checks 1–3.
2. Write `Docs/Host_Protocol.md`.
3. Firmware: matrix merge, host link, `?`, `TIME`, `K`, `P`, `S`, `N`, `A`, `X`, `D`, `E`, `F`, safety, activity LED, USB names, boot hook. Parser and `S` table tests. Mark the prototype's TOD and TR menu wires fitted with `F`. After this step TReadmill can start `keys_hw.py`.
4. Wire /RESET and the TR ALT line; add `R`, `B` and `T`; mark them fitted with `F`.
5. Wire the Kawari lines; add `V`; mark it fitted; measure the restart time (bench check 4).
6. TReadmill station tests.
7. v0.2 PCB with the proven circuits.

## Open items

- The CRESET_N pad location, and the Kawari restart time.
- Not planned: answering the TR's `dv` identify token with a TR-style banner. The board stays silent to TR tools, and their port is picked by serial string or `--uid`/`--port` (user's decision, 2026-11-16).
- Not planned: an arbitrary TOD frequency (`T 49.50`) for detection-tolerance tests. TReadmill rated it low priority.
