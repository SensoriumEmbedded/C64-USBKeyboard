# MT8808 Replacement Study

How to replace the end-of-life MT8808 8×8 analog crosspoint (U2) that closes the C64's keyboard switches. Research and planning only: nothing here is built or measured yet. Written 2026-11-16.

Every fact carries a source from the list at the end, or comes from this repo or the Teensyduino 1.61.0 core installed on this machine. Anything not confirmed is marked **unverified**. "Derived" means my own arithmetic from sourced numbers.

## Recommendation

Replace the MT8808 with direct Teensy I/O: two LSF0108 FET level translators between the 16 matrix lines and Teensy GPIO6 bits 16–31, and a GPIO interrupt that pulls the right row lines low whenever the C64 pulls a column line low.

- **Why it works.** The matrix lines idle high on the CIA's pull-ups, and keys, joysticks and the CIA's column drive only ever pull them low. The KERNAL drives the columns (port A) and reads the rows (port B). The worst-case reaction budget is about 2.1 µs (NTSC), and the KERNAL itself uses it: it reads `$DC01` 4 cycles after writing `$DC00` in three places. One interrupt with a 9-cycle GPIO read and two table lookups answers in about 0.1 µs at 816 MHz, about 20 times inside the budget. TeensyROM already does harder work on the same Teensy: an interrupt on every C64 bus cycle, answered in about 300 ns, with the USB host and device running.
- **Hardware.** 2× LSF0108 (TSSOP-20), a 200 kΩ bias resistor and two bypass capacitors per chip. No MT8808, no special-order part. The matrix moves to Teensy pins 14–23, 26, 27 and 38–41, which frees the nine MT8808 control pins. (Prices and lifecycle are in section 2.6.)
- **Firmware.** Forward emulation: sense the 8 column lines, drive the 8 row lines open-drain, with ghost-key paths computed from the 64-bit switch mask. The NVIC priority of the matrix interrupt is 0, and the Teensy runs at 816 MHz as TeensyROM does.
- **Two prerequisites, both from TeensyROM:**
  - Build with TeensyROM PR 103's patch to the core's `usb.c`, which replaces `__disable_irq()` with a BASEPRI mask. Without it, every USB serial packet can hold off the matrix interrupt for up to 2400 cycles (2.9 µs at 816 MHz), which is over budget. The patch needs arduino-cli and a private core copy, as in TeensyROM's `tools/build-firmware.mjs`.
  - No EEPROM write while the matrix is live. Release every line first, because a flash sector erase runs with interrupts off for 45 ms typical and 400 ms worst case.
- **Drop the keyboard passthrough on v0.2, or make it optional.** An active emulation can latch a line low if the same key is held on the original keyboard and on the USB side at once (section 2.1.4). That is the one MT8808 behavior firmware cannot reproduce.
- **Prove it on the v0.1 board first.** Build a small two-chip front end that plugs onto J2's long passthrough pins and wires to the Teensy. Leave the MT8808 fitted with all switches open; it is then invisible. The deciding test is a C64 program that reads `$DC01` 4 cycles after each `$DC00` write, on PAL and NTSC, while USB traffic runs (section 6.3).
- **Fallback if that test fails, or if the passthrough must stay fully transparent: the WCH CH446Q.**
  - It is an 8×16 analog crosspoint with the MT8808's own figures at VDD = 5 V: R_ON 120 Ω typ, 185 Ω max.
  - Its control inputs take 3.3 V logic (V_IH 2.0 V).
  - With one address line tied low, it decodes the 64 switches exactly as the MT8808 does, with the same strobe, data and reset polarities. Today's firmware drives it unchanged on the same 9 pins.
  - It comes in LQFP-44 only and costs about $2 at LCSC. It is single-source, with the Renesas CD22M3494 as a second source at about 7–9× the price (section 2.2).
  - It keeps every MT8808 behavior.

The MT8808 itself is finished:

- Microchip's EOL notice CAAN-23SXZK691 (24 Nov 2025) set the last-time buy at 21 May 2026, which has passed; last shipments are 31 Mar 2027.
- The PDIP MT8808AE1 the BOM calls for is already listed obsolete, and the MT8816 and MT8812 are being discontinued too.

What direct I/O gains over the MT8808:

- The board can see all 16 lines. That gives scan detection ("the C64 is alive and scanning"), a self-test of its own closures, and both joystick ports.
- It can inject joystick 1 and joystick 2 directions and fire through the keyboard connector.
- It needs no address/strobe timing, so no 180 MHz slowdown.

What it gives up:

- Reverse scans (port B driven, port A read). No software found does this, the 586220 diag cart included. The LSF hardware supports a bidirectional firmware mode later.
- Exact behavior with the original keyboard connected in parallel.

## 1. What a replacement must reproduce

### 1.1 How the C64 drives the matrix

CIA1 port A (`$DC00`, PA0–7) is wired to the keyboard columns and port B (`$DC01`, PB0–7) to the rows. The C64 schematic 251138 shows the keyboard connector this way:

| Connector pin | Signal | Connector pin | Signal |
|---|---|---|---|
| 1 | GND | 13, 19, 18, 17, 16, 15, 14, 20 | PA0–PA7 (COL0–7) |
| 3 | RESTORE (not in the matrix; NMI via the 556) | 12, 11, 10, 5, 8, 7, 6, 9 | PB0–PB7 (ROW0–7) |
| 4 | +5 V | 2 | key, no pin |

This matches this board's schematic: J2 pins 13, 19, 18, 17, 16, 15, 14, 20 are X0–X7, and pins 12, 11, 10, 5, 8, 7, 6, 9 are Y0–Y7. X is the PA bit and Y the PB bit, as in the firmware's `C64KP_*` codes. Note that the C64 Service Manual (314001-02) warns that the U1 port labels on the schematics themselves are wrong, and to trust its keyboard-matrix figure instead.

The KERNAL (901227-03) sets the ports up in IOINIT: `$DC02 = $FF` (port A all outputs), `$DC03 = $00` (port B all inputs) and `$DC00 = $7F`.

Every write to `$DC00` that the KERNAL makes, and the read that follows it, in CPU cycles (derived from the 64doc cycle tables):

| Write | Read | Cycles from write to read | Purpose |
|---|---|---|---|
| `EA90 STA $DC00` (`$00`, all columns) | `EA93 LDX $DC01` | **4** | SCNKEY: is any key down? |
| `EAA5 STA $DC00` (`$FE`) | `EAAB LDA $DC01`, `EAAE CMP $DC01` | 9, then 13 | first column, read twice until equal |
| `EAD7 STA $DC00` (columns 1–7) | same pair | 12, then 16 | other columns |
| `F6C9 STX $DC00` (`$BD`, columns 1 and 6) | `F6CC LDX $DC01` | **4** | UDTIM: shift-key check |
| `E93A STA $DC00` (`$7F`) | `E93D LDA $DC01` | **4** | screen editor scroll: CTRL check |

So:

- The KERNAL drives **one or several columns at once**: all eight (`$00`), two (`$BD`) or one. A replacement must pull a row low when any driven column has a closed switch on it.
- The 4-cycle case is in the KERNAL itself, not only in games. If the replacement is too slow, the any-key test at `EA93` sees nothing and the KERNAL skips the scan.
- **4 cycles is the hardware minimum.** A store writes on its last cycle; read-modify-write instructions write on cycles 5 and 6. The next instruction's first three cycles only touch PC, zero page or the stack, so the earliest read of `$DC01` is its cycle 4. VIC-II DMA stalls only reads, so it can lengthen the gap but never shorten it (64doc).
- Prior art agrees. AmigaKeyboard2C64 is an ATmega88 that emulates the matrix with a pin-change interrupt and a 256-entry table. Its source comments say it needs roughly 10 MHz at minimum to satisfy the picky 1541 Ultimate II menu, and that the KERNAL's own scan is far more forgiving.

**Reverse scanning** (port B as outputs, port A read):

- The KERNAL never does it.
- The Commodore 586220 diag cart's manual (326070-01 §5.10) says it tests both directions through the keyboard loopback dongle (ROWn tied to COLn). But a disassembly of the ROM (`$8B20–$8B7D`; rev 4.1.1 `$853A–$859A`) shows only port A→B cross-port reads. Port B is driven as an output only for its own read-back test. This came from a research agent's disassembly and is **not bench-confirmed**.
- codebase64's non-KERNAL scan routine (TWW/CTR) scans forward and rejects ghosts arithmetically.
- VICE emulates reverse scans (`read_ciapa`, `rev_keyarr`), but no game, demo, cartridge or GEOS/JiffyDOS routine that relies on them was found (**unverified** that none exists).

Conclusion: **forward emulation (sense PA, drive PB) covers all software identified.** Bidirectional support is a fidelity nicety, not a requirement.

### 1.2 Joysticks share the lines

- Control port 2 is on PA0–4 and control port 1 on PB0–4 (C64 Service Manual; Mapping the C64). The joystick switches ground the line.
- **Joystick 1 (port B)** looks to the KERNAL like keys: right reads as `2`, left as CTRL. This is why most games use port 2.
- **Joystick 2 (port A)** pulls a column low. With a key held in that column, the key's row reads low in every column scan. VICE models it this way; real hardware is **unverified**, but it follows from the wiring.
- **Port 1 fire (PB4) is also the VIC-II light-pen input.** A row-4 key closed in a driven column latches the light pen, on a real keyboard and on any replacement alike.
- PA6/PA7 also drive the 4066 paddle select (U28). That is a CMOS load and doesn't matter here.

What a replacement must do so joysticks and keys coexist:

1. **Never drive a matrix line high.** Lines are wired-AND: the CIA, the joysticks and the keys all only pull low. A push-pull high output would short against a joystick switch. Open-drain only.
2. **Treat a column held low by a joystick exactly like a column the CIA drives.** An emulation that watches line levels gets this for free.
3. Joystick 1 needs nothing from a forward emulation: it pulls row lines the emulation only drives, never reads.

### 1.3 Several keys and ghosting

The C64 keyboard has no diodes. Three closed switches on three corners of a rectangle make the fourth corner read as closed: holding A, S and D also reads F (codebase64; VICE's `ghosting.prg` readme confirms such combinations on real hardware). The MT8808 reproduces this exactly, because its switches are real switches.

Nothing found depends on ghosting; codebase64's routine rejects it. But to behave like a real keyboard, an emulation must compute it. The rule: a line pulled low pulls its whole connected component low, where the switch mask defines the graph. Section 3.1 turns that into two 256-entry tables.

### 1.4 Electrical levels and drive

MOS 6526 preliminary datasheet, November 1981 (pages 3 and 5):

| Parameter | Value |
|---|---|
| Port input pull-up resistance R_PI | 3.1 kΩ min, 5.0 kΩ typ |
| V_IH | 2.4 V min |
| V_IL | not given; the March 1981 sheet gives V_SS + 0.4 V max |
| V_OH | 2.4 V min at −200 µA |
| I_OH | −200 µA min, −1 mA typ |
| V_OL / I_OL | 0.4 V max at 3.2 mA |
| C_IN, C_OUT | 7 pF typ, 10 pF max |

- The datasheet says the ports have both passive pull-up devices and active pull-ups (p.5). So an output set high drives weakly, and a joystick or key can pull it low. That is how joystick 2 works against the KERNAL's port A outputs. The datasheet gives no short-circuit rating; this design only ever does what a joystick does.
- **The 8521** (HMOS CIA in the C64C): no datasheet found; assumed equivalent (**unverified**).
- No external pull-ups are drawn on CIA1 PA/PB on schematic 251138 (other boards **unverified**).
- VICE's `ciaports` tests on one real C64 found that a single key cannot pull a port B output that is driven high down to a low: PB usually stays high. It takes two or more keys in the column, or SHIFT LOCK. Switch resistance matters at that edge.

Low levels today:

- MT8808 at VDD = 5 V has R_ON 120 Ω typ, 185 Ω max at 25 °C (225 Ω max at 85 °C).
- A row pulled low through it sits at the column's V_OL plus about 1 mA × R_ON. That is roughly 0.2–0.4 V (derived), near the March sheet's 0.4 V V_IL, and it has worked in practice.

The Teensy 4.1's pads are not 5 V tolerant: the absolute maximum is OVDD + 0.3 V (i.MX RT1060 datasheet). So every one of the 16 lines needs translation. The pad thresholds at 3.3 V are V_IH ≥ 2.31 V and V_IL ≤ 0.99 V, with optional hysteresis.

### 1.5 Timing budget

- **Write side.** The CIA changes a port output up to T_PD = 1000 ns after the φ2 falling edge that ends the write cycle.
- **Read side.** It needs the port input valid T_PS = 300 ns before the φ2 rising edge of the read cycle (Nov 1981 sheet, p.5 and timing diagrams).
- With the read 4 cycles after the write, the read's φ2 rising edge comes about 3.5 cycles after the write's falling edge. That assumes a 50 % φ2 duty cycle (**unverified**).

| | PAL (985 248 Hz) | NTSC (1 022 730 Hz) |
|---|---|---|
| Cycle | 1015 ns | 978 ns |
| Write φ2 fall to read φ2 rise (3.5 cycles) | 3552 ns | 3422 ns |
| Minus T_PS (300 ns) | 3252 ns | 3122 ns |
| Minus worst-case T_PD (1000 ns): **budget from the column changing at the pin to the row valid at the pin** | **2.25 µs** | **2.12 µs** |

Real CIAs are probably much faster than the 1 µs T_PD limit (typical T_PD **unverified**), which would leave close to 3 µs. Plan to the 2.1 µs figure.

The budget covers both directions of a column change:

- **Assert.** A newly driven column must pull its rows low. Falling edges are fast.
- **Release.** A column that stops being driven must let its rows rise above V_IH = 2.4 V. A released row rises only through the CIA's 3.1–5 kΩ input pull-up, against the line's capacitance.
  - With 50–100 pF that takes about 0.1–0.35 µs to reach 2.4 V (derived).
  - On a real keyboard the row rises with the column's active pull-up behind it. An emulation adds its reaction time plus this passive rise.
  - The KERNAL's column loop reads twice (`LDA`/`CMP` at 12 and 16 cycles) and retries until the reads agree, which absorbs a slow release. A 4-cycle game loop does not.

## 2. Hardware options

### 2.1 Direct Teensy I/O (the recommended route)

The Teensy watches the column lines and pulls the matching row lines low itself.

#### 2.1.1 Translator choice

| Choice | How it works | Pins | ICs | Verdict |
|---|---|---|---|---|
| **LSF0108** (TI) or **NVT2008** (NXP) | Passive pass FETs; the gates are biased from Vref_B through 200 kΩ. A low on either side passes both ways; highs stay on their own side. No one-shots, no internal pull-ups. | 16 | 2 | **Chosen.** One pin per line, so the Teensy can read and drive every line. R_ON 3 Ω typ at Vref_A 3.3 V / Vref_B 5 V. Pull-ups are required on the 5 V side (the CIA's) and optional on the 3.3 V side. Fast enough: 40 MHz at 50 pF. $1.41 each (TI, qty 1); NVT2008PW $0.90 (NXP). |
| SN74CB3T3245 | 3.3 V FET bus switch. I/O are 5 V tolerant whether powered or not; the output clamps near V_CC. | 16 | 2 | Works the same way for open-drain lows. r_on 5 Ω typ, 8.5 Ω max; Ioff. No bias network, and an /OE pin the firmware could hold off until the tables are ready. The alternative if the LSF misbehaves at power-up. $1.63. |
| TXS0108E | Pass FET plus one-shot edge accelerators (about 30 ns) and internal pull-ups of 4 kΩ / 40 kΩ | 16 | 2 | **Rejected.** TI's app note SCEA056 says to avoid strong external pull-ups below 50 kΩ, and the CIA's own pull-ups are 3.1–5 kΩ. The one-shots also drive the line high on every rising edge. |
| TXB0108 | Push-pull auto-direction buffer | 16 | 2 | **Rejected.** TI's datasheet limits it to push-pull CMOS outputs, requires any pull-up to be above 50 kΩ, and rules out I2C-like lines. |
| 74LVC244A/245A (sense) + 74LVC07A (open-drain drive): TeensyROM's parts | Separate sense and drive per line. LVC244A inputs and LVC07A outputs take 5.5 V at V_CC 3.3 V. | 16 forward-only, 32 for both directions | 3 or 5, plus pull-ups on every LVC07 input so the outputs stay off while the Teensy boots | Works, and the parts are proven on TR/TR+ ($0.64 and $0.47). At 16 pins it is forward-only and cannot see the rows or drive the columns. At 32 pins it uses nearly every free pin and splits the drive across two GPIO ports. |
| 74LVC125A as an open-drain driver (input low, output enable switched) | – | – | – | **Rejected.** Its outputs are rated to V_CC + 0.5 V with no Ioff, so they are not 5 V tolerant even when disabled. |
| Resistor dividers | Sense only | 16 + a driver | many | **Rejected.** A divider cannot pull a line low: a 10 kΩ series resistor against the CIA's ~4 kΩ pull-up leaves the line near 3.6 V. It also loads every line. |
| Series resistor into a Teensy pin | Relies on the pad's clamp diodes | – | – | **Rejected.** The pads are not 5 V tolerant, and the clamp current goes into the 3.3 V rail. |
| Discrete MOSFET per line | – | – | 16 FETs + resistors | Same principle as the LSF, ten times the parts. |

**Is separate sense and drive per line cleaner than one bidirectional path?** No:

- The firmware can only ever see the wire. Whether the sense buffer sits beside the drive transistor or the pad reads its own open-drain output, a low the board itself causes looks like a low the CIA causes. Separate paths do not remove that ambiguity.
- Separate paths do keep the Teensy pin away from the C64 side. But the LSF already limits the Teensy side to about 3.3 V.
- So separate paths cost twice the pins for the same capability, or lose capability at the same pin count.

How the LSF0108 is wired:

- 5 V side (B) to the C64 lines. 3.3 V side (A) to the Teensy pads.
- V_REFA to the Teensy's 3.3 V. V_REFB and EN to +5 V through 200 kΩ.
- An A-side pull-up is optional per TI. Enable the pad's internal 22 kΩ (`PUS(3)`) anyway, so the Teensy reads a clean high.
- The Teensy pads used as outputs run in open-drain mode. `pinMode(…, OUTPUT_OPENDRAIN)` sets `ODE` and also `SION`, so the pad stays readable through `GPIO6_PSR`.
- **Power-up.** The LSF datasheet says EN must be low for high-Z during power-up and power-down. With EN tied to the bias, it is not low. So while the C64's 5 V is up and the Teensy's 3.3 V is not yet, the pass FETs sit near threshold against an unpowered pad.
  - This should only leak microamps against the CIA's milliamp pull-ups, and the C64 holds the CIA in reset for that time anyway.
  - It is bench test 6.2.3. If it shows a problem, switch to SN74CB3T3245.

Pull-down strength: the pad at DSE 7 plus the LSF's 3 Ω against the CIA's ~1 mA pull-up puts the row at a few tens of millivolts (derived). That is lower than the MT8808 gives today.

#### 2.1.2 Behavior compared with the MT8808

| Behavior | MT8808 | Direct I/O, forward mode |
|---|---|---|
| Columns driven, rows read (KERNAL, all software found) | yes | yes |
| Several columns at once | yes | yes (union in the table) |
| Ghost key seen on `$DC01` | yes | yes (components in the table) |
| Ghost path visible on `$DC00` (a column pulled low through keys) | yes | no: the emulation never drives columns |
| Reverse scan (rows driven, columns read) | yes | no; bidirectional mode (section 3.2) adds it |
| Joystick 2 with a key held in its column | row low in every scan | same |
| Joystick 1 with keys held (phantom joystick 2 on `$DC00`) | yes | no |
| Original keyboard in parallel | transparent | works, except the latch in 2.1.4 |
| Row low level | about 0.2–0.4 V | about 0.03 V |
| Response time | 30 ns switch propagation, 100 ns from strobe | about 0.1 µs typical (section 3.5) |
| Firmware hang | switches stay as last set | lines stay as last set; a watchdog releases them |

#### 2.1.3 What it costs

| Item | Cost |
|---|---|
| Parts | 2 × LSF0108PW (TSSOP-20), 2 × 200 kΩ, 4 × 0.1 µF: about $3 at qty 1, $1.30 at 1k |
| Board area | About 2 × 45 mm² plus passives, against about 550 mm² for the DIP-28 MT8808 (derived from package sizes) |
| Teensy pins | 16 matrix pins (14–23, 26, 27, 38–41) instead of 9 control pins |
| Firmware | A real-time interrupt, the PR 103 core patch, a move to an arduino-cli build |

#### 2.1.4 The passthrough latch

J2 is a long-pin header so the original keyboard can stay plugged in (BOM note). With the MT8808 that is harmless, because a switch carries no gain. With any active emulation, the following loop can occur:

1. The CIA drives column 1 low. The emulation sees it and pulls row 2 low, because A is pressed on the USB side.
2. A is also held on the original keyboard. Its switch joins row 2 to column 1.
3. The CIA releases column 1. The column stays low, held through the real key by the emulation's own row drive.
4. The emulation still sees column 1 low and keeps row 2 low. The line is latched.
5. While latched, row 2 reads low in every column, and `$DC00` bit 1 reads low (joystick 2 "down").

The latch clears when either key is released.

- The same loop can form through three real keys plus one emulated key on a rectangle.
- The CPLD and RP2350 routes have it too.
- No firmware fix exists: the emulation cannot tell a column held low by the CIA from one held by its own row drive through an external switch.
- Hence the recommendation: drop the passthrough on v0.2 (already accepted as an option), or keep it with this caveat documented.

### 2.2 Analog crosspoint ICs (the MT8808's own class)

A real analog switch reproduces everything in section 1 with no timing at all, including reverse scans, ghost paths on `$DC00` and a parallel keyboard. Firmware barely changes: the same address/data/strobe write, plus delays so the Teensy can run fast.

| Part | Status (2026-11-16) | Price | Fit for this board |
|---|---|---|---|
| MT8808AE1 / AP1 | EOL. LTB 21 May 2026 (passed), last shipment 31 Mar 2027. PDIP listed obsolete. | broker, PLCC: $10.63 | today's part; brokers only |
| MT8816 / MT8812 / MT8809 | EOL. LTB 15 Feb 2026 / 21 May 2026 / May 2021. | – | no |
| **WCH CH446Q** | Listed by WCH, no lifecycle flag | LCSC C109471: $2.16 at 1, $1.53 at 160, 661 in stock | **Fallback.** Details below. |
| WCH CH446X | same | $1.73 | 5×24, serial only. Five lines on one side cannot cover 8×8. No. |
| Renesas CD22M3494 | Active (Renesas product page, 2026-11-17). The PDIP-40 (EZ) is no longer listed. | Renesas: $14.41 (MQZ, PLCC-44) | 16×8, 4–15 V, MT8816 pinout and address map. V_IH 2.4 V at VDD = 5 V, so 3.3 V logic works, at a higher supply current (datasheet note 5). R_ON is specified only at 10 V and 12 V (75 / 65 Ω max), not at 5 V. Second source for the CH446Q's role at about 7–9× the price. |
| ADI MAX14661 | Production | $5.70 at 1k | 16:2 switch matrix, 1.6–5.5 V supply, passes ±5.5 V signals, R_ON 5.5 Ω, I2C or SPI with simultaneous update. Four make an 8×8 (each column on a COM pin, the rows bussed to all four). Works, at about 10× the CH446Q's cost and four parts. |
| ADI ADG2188 (8×8, I2C) | Production | $6.55 at 1k, LCSC $12.05 | Specified only at ±5 V and single 12 V; single 5 V not tabulated. LFCSP-32. No. |
| ADI ADG2128 (8×12, I2C) | Production | $8.76 at 1k | Signal range V_SS to V_DD − 2 V, so a single 5 V supply passes 0–3 V. A key only has to pass lows, but this is outside the datasheet's tabulated cases. LFCSP-32. No. |
| ADI AD75019 (16×16) | Production | $35.75 at 1k | Cost. No. |

**The CH446Q in detail** (WCH datasheet CH446DS1, v1D):

- **Size:** 8×16, of which this board would use 8×8.
- **Supply:** VDD 4–13.2 V, with VDD = 5 V, GND = VEE = 0 V a recommended combination.
- **Analog behavior:** R_ON5 120 Ω typ, 185 Ω max at 25 °C, 225 Ω max at 85 °C; I_OFF ±500 nA max. Those are the MT8808's numbers at 5 V.
- **Logic levels:** V_IL 0.8 V, V_IH 2.0 V at VDD − GND = 5 V, so the Teensy's 3.3 V outputs drive it with margin. The MT8808 is only characterized at VDD = 12 V and needs V_IH 3.3 V there.
- **Interface:**
  - Parallel mode: 7-bit address, CS, STB, DAT, RST. Data latches on the STB falling edge.
  - Serial mode: 7 clocks on CS/CK, then STB.
  - **Same decode and control as the MT8808.** With AX3 tied low, AY2–AY0 and AX2–AX0 select the same 64 switches as the MT8808's truth table. STB is active high with data latched on its falling edge, DAT high closes a switch, and RST high opens all of them, as on the MT8808. Today's switch-write code works unchanged. WCH's note that the X6–X13 addresses differ compares it with the MT8816, which decodes those lines out of order (as the CD22M3494 does). It does not apply to this board.
- **Package:** LQFP-44, 10 × 10 mm, 0.8 mm pitch (hand-solderable). There is no DIP version.
- **Pins:** 9 Teensy pins in parallel mode, the same as today, with AX3 to GND and CS/CK and P/-S to VDD. Serial mode needs 4–5.
- **v0.1 drop-in (derived, not built):** because the decode and polarities match, a CH446Q on a small DIP-28 adapter (AX3 and VEE to GND, CS/CK and P/-S to VDD) would replace the MT8808 on existing boards with no firmware change. Height and clearance on the v0.1 board are unchecked.
- **Power-up:** the datasheet describes no power-on reset, so the latches are undefined until firmware pulses RST. The MT8808 is the same. A 10 kΩ pull-up from RST to 3.3 V holds every switch open until the Teensy drives the pin.
- **Upsets:** for electrically noisy environments, WCH suggests the MCU rewrite every switch every few seconds, which implies the latches can be upset. Rewriting all 64 once a second costs a few microseconds.
- **Datasheet gaps:** v1D is undated and gives no ESD, latch-up or thermal ratings. The MT8808 sheet gives none of these either. This matters because the joystick ports put these lines on external connectors.
- **Risk:** single source. WCH's product page shows no lifecycle status, so a discontinuation would first show as LCSC stock running out. At about $1.50 each, buying a lifetime stock up front is cheap. The CD22M3494 is a real second source at about 7–9× the price, needing only different X6/X7 wiring.

The datasheet gives timing at VDD = 5 V, VEE = −7 V. Write the switch routine with `delayNanoseconds()` margins rather than CPU speed.

### 2.3 5 V CPLD

A CPLD can implement forward emulation as combinational logic: row j low when any (column i low AND switch ij closed).

- **Response:** about 10 ns.
- **Size:**
  - It needs the 64 switch bits stored on-chip, 16 matrix I/O and a load interface, for at least 64 flip-flops plus the decode.
  - An ATF1504AS (64 macrocells) is full with the 64 bits alone, unless a macrocell can hold a buried register and still drive its pin with other logic (**unverified**). Plan on an ATF1508AS (128 macrocells, PLCC-84 or TQFP-100).
- **Bidirectional logic would latch on its own outputs.** Once it pulls a row low, it sees that row low and pulls the column. Breaking that loop needs per-line timing (blanking counters), the same problem as the firmware's bidirectional mode, in macrocells.
- **Passthrough:** it has the same latch as section 2.1.4.
- **Ghosting:** have the Teensy load the component closure (switch ij closed when column i and row j share a component), not the raw mask. Plain forward logic then reproduces ghost rows.

Parts and status (Microchip Direct, qty 1–24, 2026-11-16):

| Part | Price | Notes |
|---|---|---|
| ATF1508AS | PLCC-84 $12.58, TQFP-100 $16.13 | In production. Per-pin open-drain option. |
| ATF1504AS | $4.47 | Too small. |
| ATF1502AS | $3.26 | Too small. |

The other 5 V-tolerant families are mostly gone:

- XC9500XL discontinued (final orders 29 Jun 2024).
- MAX 3000A discontinued (2018).
- ispMACH 4000B/C discontinued.
- ispMACH 4000ZE is 5 V tolerant, but Lattice limits how many I/Os may sit at 5 V.
- MAX II/V need series resistors and clamp diodes.
- A GreenPAK SLG46826 has 17 flip-flops, far short of 64.

Toolchain:

- **Design:** WinCUPL is free and old. ProChip Designer needs a license, issued on request.
- **Programming:** Microchip's ATDH1150USB cable ($91.07). Or the open-source route: prjbureau's `fuseconv` converts JED to SVF and OpenOCD plays it. prjbureau lists ATF1502AS programming as done and ATF1504AS as untested; ATF1508AS is **unverified**.
- **JTAG levels:** the I/O and JTAG run at 5 V, so TDO needs translation before the Teensy.

Verdict: about 10 ns forward response, but the cost, the board area, a third toolchain and the same fidelity gaps as direct I/O make it worse than either recommended route.

### 2.4 RP2350 co-processor

A second microcontroller dedicated to the matrix, with the Teensy sending it the 64-bit mask over SPI or UART.

- **Wiring and speed.** With 5 V-tolerant GPIOs (but see the power-sequencing catch below), it can connect to the 16 lines directly. One core can then spin in a loop with no interrupts: read the pins, two table loads, write the output enables. That is roughly 15–25 cycles, about 0.1–0.2 µs at 150 MHz.
- **Timing isolation.** It is independent of everything the Teensy does: USB, EEPROM and future features.
- **Cost.** A second firmware, its own boot path (flash, or loading over UART from the Teensy), a crystal, the core regulator, and board area.
- **Fidelity.** It has the same latch and the same forward/bidirectional choice as section 2.1.

Parts and status:

- **RP2040: not 5 V tolerant.** Absolute maximum is IOVDD + 0.5 V.
- **RP2350 (A4 stepping, July 2025): 5 V tolerant only with power on.** Raspberry Pi made it officially 5 V tolerant on the condition that IOVDD is powered at 3.3 V (datasheet §14.8.2.1, as quoted on the Raspberry Pi forum; the current PDF was not read). Raspberry Pi's blog warns that a pad given 5 V while IOVDD is off is damaged.
  - **That condition is the problem here.** The CIA's pull-ups put 5 V on all 16 lines whenever the C64 is on, and the RP2350's 3.3 V would come from a regulator on the same 5 V. So there is always a window with 5 V on the pads and IOVDD still rising. Series resistors or a front end would be needed anyway, which removes the RP2350's main advantage.
  - **Erratum E9** (input leakage, A2 stepping) is fixed on A4 and would not matter with the CIA's pull-ups.
- **Price.** RP2354A is $1.30 with 2 MB flash in the package, so no external flash. RP2350A is $1.10.
- **Boot.** The bootrom can boot from UART (third-party sources), so the Teensy could load its image at power-up.

Verdict: deterministic timing, but it needs a second firmware, plus a 5 V power-sequencing hazard that needs a front end anyway. Only worth it if the Teensy cannot be made to meet the budget.

### 2.5 Discrete analog switches

64 independent switches need either:

- 16 quad switches (74HC4066) plus 64 latched control bits (8 × 74HC259 addressable latches, which also gives an MT8808-style interface): about 25 ICs.
- Or 8 octal serial switches (ADG714/ADG715).

Both are far larger than any other option:

- **ADG714/ADG715** (ADI, production): octal SPST, 2.7–5.5 V, R_ON 2.5 Ω, TSSOP-24, $2.84 / $3.32 at 1k. Eight of them cost about $23 and take about 400 mm². Daisy-chained SPI on the ADG714 is **unverified**.

Rejected.

A discrete-MOSFET crosspoint (one N-FET per switch) does not work at all. Each FET's body diode conducts when its source sits above its drain. So with every switch off, any line pulled low on the drain side would still drag every line on the source side down to about 0.6 V, at or near a valid low. Fixing that needs back-to-back pairs: 128 FETs.

### 2.6 Comparison

| | Direct I/O (2× LSF0108) | CH446Q | MAX14661 ×4 | ATF1508AS | RP2354 co-processor | ADG714 ×8 | MT8808 (today) |
|---|---|---|---|---|---|---|---|
| ICs | 2 (TSSOP-20) | 1 (LQFP-44) | 4 | 1 (PLCC-84/TQFP-100) + JTAG header | 1 + crystal, regulator, inductor, front end | 8 (TSSOP-24) | 1 (DIP-28) |
| Area, approx. (derived) | 100 mm² | 150 mm² | 150–250 mm² | 250–900 mm² | 300+ mm² | 400 mm² | 550 mm² |
| Parts cost, 1k qty | ~$1.30 | ~$1.50 | ~$23 | $12.58–16.13 | ~$2–3 + front end | ~$23 | broker $10+ |
| Teensy pins | 16 | 9, as today (or 4–5 serial) | 2–4 | 4–8 + JTAG | 2–4 | 3 | 9 |
| Forward scan, several columns | yes | yes | yes | yes | yes | yes | yes |
| Reverse scan | firmware option | yes | yes | hard | firmware option | yes | yes |
| Ghost rows on `$DC01` | yes | yes | yes | yes (closure) | yes | yes | yes |
| Joystick coexistence | yes | yes | yes | yes | yes | yes | yes |
| Original keyboard in parallel | latch on same key | transparent | transparent | latch | latch | transparent | transparent |
| Board can sense the lines / inject joysticks | yes / yes | no / no | no / no | possible | yes / yes | no / no | no / no |
| Hard real-time firmware | yes (ISR, PR 103 patch) | no | no | no | in the co-processor | no | no |
| Lifecycle | Active, two vendors (TI LSF0108, NXP NVT2008) | Active, WCH (no lifecycle data published); CD22M3494 active | Production | Production | Active | Production | EOL, LTB passed |
| Main risk | Latency under rare conditions; must be measured | Single source (CD22M3494 at 7–9× the price) | Cost, four parts | Toolchain, size, cost | Second firmware, 5 V sequencing | Size, cost | No supply |

Direct I/O wins on cost, area, supply and capability; the CH446Q wins on fidelity and zero firmware risk. The others lose to one of these two on every row that matters.

## 3. Firmware for the active routes

### 3.1 What the firmware computes

Number the lines L0–L7 = PA0–PA7 (X0–X7) and L8–L15 = PB0–PB7 (Y0–Y7). A closed switch (i, j) joins Li and L(8+j). A line pulled low by anyone pulls its whole connected component low. That one rule covers single keys, several columns, joysticks and ghosting.

When the merged 64-bit mask changes (thread context, never in the fast path):

1. **Components.** Run union-find over 16 nodes and up to 64 edges. That gives `comp[n]`, the set of lines in line n's component.
2. **Lookup tables.**
   - Build two 256-entry tables: `tblA[a]` is the OR of `comp[i]` for each PA bit i set in `a`, and `tblB[b]` is the same for PB bits.
   - Because the result is a union over the driven lines, the lines pulled low for a driven set D are `tblA[D & 0xFF] | tblB[D >> 8]`, exactly.
   - Store the entries already shifted to their GPIO6 bit positions (`uint32_t`), so the fast path does no bit shuffling.
   - With the pin map in section 4, each table is 1 KB in DTCM. Any pin order within a byte works: the tables absorb the permutation, so the PCB can route for convenience.
3. **Publish.** Build into a spare buffer, publish with one pointer store, then pend the matrix interrupt (`NVIC_SET_PENDING(IRQ_GPIO6789)`).
   - The pend matters: a program that holds one column low and polls `$DC01` makes no column edge. A key pressed during that hold would otherwise not appear until the next column change.

**Build cost:** 64 unions plus 512 table entries of at most 8 ORs each, a few microseconds at 816 MHz (derived). It runs at key-event rate.

### 3.2 The fast path

**Forward mode** (default) senses the columns and drives the rows:

```c
// Raw vector on IRQ_GPIO6789 (attachInterruptVector), NVIC priority 0. FASTRUN.
// Columns PA0-7 on GPIO6 bits 16-23, rows PB0-7 on bits 24-31 (section 4).
// Row pads are OUTPUT_OPENDRAIN: DR bit 0 pulls low, 1 releases (Hi-Z).
FASTRUN void matrix_isr(void)
{
    GPIO6_ISR = COL_MASK;                         // clear first: a later edge re-pends
    uint32_t low  = ~GPIO6_PSR;                   // one ~9-cycle read of all 16 lines
    uint32_t want = tbl->A[(low >> 16) & 0xFF];   // rows to pull low, ghost paths included
    GPIO6_DR_CLEAR = want & ROW_MASK;             // pull low
    GPIO6_DR_SET   = ROW_MASK & ~want;            // release
    asm volatile ("dsb" ::: "memory");            // let the ISR-flag clear land before return
}
```

- `GPIO6_EDGE_SEL` sets both-edge detection on the eight column bits.
- The sensed set (columns) and the driven set (rows) are disjoint, so the firmware can never latch on its own outputs.
- A joystick in port 2 is just another column driver.
- Use a raw vector, not `attachInterrupt()`. The core's dispatcher scans all four fast ports and calls through a table, which adds cycles. Nothing else in this firmware uses pin interrupts.

**Bidirectional mode** (optional, later) senses and drives all 16 lines:

```c
    uint32_t low  = ~GPIO6_PSR & LINE_MASK;
    uint32_t ext  = low & ~own & ~blanked;                         // driven low by someone else
    uint32_t want = (tbl->A[(ext >> 16) & 0xFF] | tbl->B[ext >> 24]) & ~ext;
    // lines leaving `want` are released and added to `blanked` for t_blank;
    // a one-shot timer re-runs the evaluation when t_blank ends
```

- **Own drive (`own`).** The firmware cannot tell its own pull-down from an external one on the same wire, so it excludes the lines it drives.
- **Blanking (`blanked`).** After releasing a line, it ignores that line until it has had time to rise. Set `t_blank` from the measured rise time, about 1 µs.
- **Re-evaluation.** A line that stays low because the CIA holds it makes no new edge, so a timer must re-run the evaluation when the blank window ends.
- Build this only if bench work or a real program shows a need. Section 1.1 found none.

### 3.3 Why an interrupt, not polling, DMA or logic blocks

- **Polling in `loop()`** is out: `usb.Task()` and the host-protocol work make the loop period microseconds to milliseconds.
- **A dedicated polling core** doesn't exist; the RT1062 has one Cortex-M7.
- **DMA** cannot reach GPIO6–9, and pin edges cannot request DMA except through XBAR. The pins chosen here are AD_B1 pads, which have no XBAR path, and their FlexIO block (FlexIO3) has no DMA.
- **XBAR/AOI** is 2 blocks × 4 outputs, each a sum of products of 4 inputs, and the AOI inputs cannot come from pins at all.
- **A GPIO interrupt at the top NVIC priority** preempts everything this firmware runs. Only code that masks all interrupts can delay it, and section 3.4 deals with that.

### 3.4 Interrupt audit (Teensyduino 1.61.0, read from the installed core)

| Source | Priority or masking | Effect on the matrix interrupt |
|---|---|---|
| USB host (USBHost_t36), IRQ 112 | 128 | Preempted. The keyboard callbacks (`OnRawPress`) run here, in interrupt context. |
| USB device (CDC serial), IRQ 113 | 128 | Preempted. |
| IntervalTimer (PIT), IRQ 122 | 128 (shared; takes the highest of the active timers) | Preempted. TOD and the µs action edges gain at most one matrix ISR of jitter (about 0.1 µs). |
| SysTick, PendSV | 32 | Preempted. |
| `usb.c` `schedule_transfer()`, on every USB packet queued | `__disable_irq()` around an ATDTW loop bounded by 2400 cycles | **Over budget.** Worst case 2.9 µs at 816 MHz, 4.0 µs at 600 MHz, 13.3 µs at 180 MHz. TeensyROM PR 103 fixes it (below). |
| `usb.c` `usb_start_sof_interrupts()` | `__disable_irq()`, short | Fixed by the same patch. |
| `micros()`, USBHost timer-list updates | short `__disable_irq()` | Tens of cycles. |
| `eeprom.c` flash write and sector erase | `__disable_irq()` until the flash reports done | 0.4–3 ms per page program, 45–400 ms per 4 KB sector erase (Winbond W25Q64JV). The matrix freezes in whatever state it had. |

**TeensyROM PR 103** (merged 2026-11-15) builds against a private copy of the core:

- In that copy, `schedule_transfer()` and `usb_start_sof_interrupts()` raise BASEPRI to 32 instead of setting PRIMASK.
- That still keeps out USB, SysTick and the timers, but not an interrupt at a higher priority (TeensyROM's `isrPHI2` at 16; the matrix at 0 here).
- The ATDTW loop becomes 32 tries instead of 2400 cycles, because a preemption could otherwise end it after one try.
- The build fails if the core text it patches has drifted.
- `schedule_transfer()` is static, so a sketch cannot override it; the patch must be applied at build time.
- On hardware it took TeensyROM from 167 missed register reads to 0.
- The patch is only safe while nothing the matrix ISR calls reaches USB. That is true here: the ISR touches only GPIO and two tables.

Rules that follow for this firmware:

1. Matrix interrupt at priority 0. The only other user of 0 is the core's temperature-panic interrupt.
2. Build with the PR 103 patch (reuse TeensyROM's `tools/lib/usb-irq-mask.mjs`).
3. Own critical sections use BASEPRI (masking priority 16 and up), never `__disable_irq()`.
4. Before any EEPROM write (the plan's `F`), release all lines, mask the matrix interrupt, write, unmask and pend it. The C64 then sees no keys for the write's duration instead of a frozen column, and only `F` writes the EEPROM.
5. Keep the ISR, its tables and the vector table in RAM: FASTRUN, the default placement. No `FLASHMEM` or `PROGMEM` in the fast path.
   - TeensyROM found that even thread-mode code layout matters: its `isrPHI2` started 18–40 ns late on about 1 entry in 85 in a bad layout, 1 in 6000 in a good one. That is negligible against a 2.1 µs budget.
6. Enable the RT1062 watchdog, fed from `loop()`. A hang then resets the Teensy, whose pads come back as inputs, releasing every line. Without it, a hung firmware leaves its last rows pulled low: a stuck key, as a hung MT8808 setup would leave a closed switch.

### 3.5 Latency estimate

Derived at 816 MHz (1.23 ns per cycle), from the sources noted:

| Step | Cycles | ns |
|---|---|---|
| Pad input synchronization (3 cycles, NXP AN12078) | 3 | 4 |
| Exception entry, lazy FP stacking on (10–12, AN12078 / Arm) | 12 | 15 |
| Clear flag, read `GPIO6_PSR` (about 9 cycles, PJRC forum measurements) | 10 | 12 |
| Two table loads, logic | 6 | 7 |
| Two `DR` writes (posted) and the pad output stage | 3 | about 10 |
| Row line falls through the pad and LSF (about 30 Ω × 100 pF) | – | about 10 |
| **Assert, typical** | **≈ 35** | **≈ 60 ns** |
| Release: the same, plus the row's passive rise to 2.4 V | – | ≈ 60 ns + 100–350 ns |

- **Cross-check:** NXP measured 75.6 ns pin-to-pin from a GPIO interrupt on an RT1050 at 600 MHz (AN12078).
- **Worst case without masking:** add an outstanding load from device memory or a flash fetch that cannot be abandoned. That is likely well under 0.5 µs (**to be measured**).
- **Margin:** with the PR 103 patch and the EEPROM rule, nothing in this firmware masks interrupts for longer than tens of cycles. That is about 20× against the 2.12 µs NTSC budget, typical, and comfortably more than 2× worst case (**to be measured**, section 6.3).

### 3.6 Clock speed

- **Run at 816 MHz**, as TeensyROM does: `set_arm_clock(816000000)`, which its code comment calls a slight overclock needing no cooling. TeensyROM's ControlComms doc shows about 59 °C at that speed.
- **Where the 180 MHz setting came from.** The code comment says 600 MHz was too fast for the MT8808. The likely cause is the MT8808's 10 ns setup/hold and 20 ns strobe-width minimums, which back-to-back pin writes at 600 MHz can violate (an inference, not measured).
- **Which backend still needs delays.** The direct route has no strobe. A crosspoint fallback needs `delayNanoseconds()` in the write routine instead of a slow CPU.
- **Power.** A Teensy 4.x draws about 100 mA at 600 MHz (PJRC). 816 MHz draws more (**not measured here**). Measure the board's total current from the C64's 5 V with a USB keyboard attached.
- **Lower speeds work, with less margin.** 600 MHz gives about 80 ns typical.

### 3.7 What the rest of the firmware still gets

- **KERNAL scanning:** 9 writes per jiffy, about 20 interrupt runs, roughly 1200 per second, about 0.01 % of the CPU.
- **Worst-case program:** a loop that changes `$DC00` every 4 cycles is about 250 000 writes and up to 500 000 edges per second, which costs 3–5 % CPU (derived).
- **Effect on other work:** each interrupt delays `loop()`, the USB stacks and the timed host actions by about 0.1 µs. µs-timed `IntervalTimer` edges gain at most that much jitter.

## 4. Pin budget (Teensy 4.1)

### 4.1 Fast GPIO on the header

The core switches every pin to the fast GPIO6–9 at startup (`IOMUXC_GPR_GPR26..29 = 0xFFFFFFFF`). One read of `GPIO6_PSR` returns every GPIO6 pin, and `GPIO6_DR_SET`/`DR_CLEAR` change any subset atomically. Teensy 4.1 pins by port (`core_pins.h`):

| Port | GPIO bit → Teensy pin |
|---|---|
| GPIO6 | 2→1, 3→0, 12→24, 13→25, **16→19, 17→18, 18→14, 19→15, 20→40, 21→41, 22→17, 23→16, 24→22, 25→23, 26→20, 27→21, 28→38, 29→39, 30→26, 31→27** |
| GPIO7 | 0→10, 1→12, 2→11, 3→13, 10→6, 11→9, 12→32, 16→8, 17→7, 18→36, 19→37, 28→35, 29→34 |
| GPIO8 | 18→28, 22→31, 23→30 (plus SD pads 42–47) |
| GPIO9 | 4→2, 5→3, 6→4, 7→33, 8→5, 31→29 (plus bottom pads 48–54) |

GPIO6 bits 16–31 are the only 16 contiguous bits on the header, and they hold the whole matrix in one register:

- X0–X7 (PA0–7) on bits 16–23: pins 19, 18, 14, 15, 40, 41, 17, 16
- Y0–Y7 (PB0–7) on bits 24–31: pins 22, 23, 20, 21, 38, 39, 26, 27

The trade-off: these AD_B1 pads have no XBAR or DMA path, so this choice commits to the CPU interrupt, which is the plan anyway. They reset to a GPIO input with the keeper on (i.MX RT1060 datasheet), so they do not drive the lines before `setup()`.

### 4.2 Budget per option

| Function | v0.1 today | TReadmill plan | Direct I/O | Crosspoint fallback |
|---|---|---|---|---|
| Matrix | 0, 2–8, 10 (9) | same | 14–23, 26, 27, 38–41 (16) | 0, 2–8, 10 (9), as today |
| RESTORE | 1 | 1 | 1 | 1 |
| TOD | 25 | 25 | 25 | 25 |
| /RESET | – | 24 | 24 | 24 |
| Kawari J1, CRESET_N | – | 26, 27 | **move to 29, 30** | 26, 27 |
| TR menu, TR ALT | 32 | 32, 28 | 32, 28 | 32, 28 |
| LED | 13 | 13 | 13 | 13 |
| USB host | 5-pin header (not GPIO) | same | same | same |
| GPIO used / free (of 42 header pins) | 12 / 30 | 17 / 25 | 24 / 18 | 17 / 25 |

Direct I/O frees pins 0 and 2–8, 10 and costs 14–23, 26, 27 and 38–41. The only collision is the TReadmill plan's Kawari pair on 26 and 27. Pins 29 and 30 are on the same header row as 24–28 and 32, on the 3.6 too. Move them in the plan now, before anything is hand-wired. (Raised as open question 4.)

## 5. Impact on the TReadmill Support Plan

The host protocol and the matrix merge already work on an abstract 64-switch mask, so the protocol does not change. What changes:

- **`Matrix.cpp`: one API, two backends.**
  - The per-source masks (physical, host) and their OR stay as planned.
  - On a change, the merged mask goes to a backend:
    - `Mt8808Backend` (v0.1 boards): writes the switches that changed, as planned, with `delayNanoseconds()` around the strobe so the CPU can run fast.
    - `DirectBackend` (prototype, v0.2): rebuilds the tables and pends the interrupt.
  - Choose the backend at build time, or from a board strap read at boot.
  - Keep both: boards with an MT8808 stay in the field.
- **F12.** "Clear the physical mask, reset the MT8808, rewrite the host bits" becomes the backend's reset. For direct I/O that means release all lines and rebuild.
- **`D` (loopback diagonal).** Still a mask pattern. The 586220 diag cart should pass in forward mode, per the disassembly in 1.1; section 6.4 confirms it.
- **Corrections to send TReadmill.** "The board can't sense the matrix" stops being true on direct-I/O boards. Proposed additions for `Host_Protocol.md`, not yet decided:
  - `M ?`: line states and scan statistics (columns seen, scans per second), so TReadmill can see that the C64 is alive and scanning.
  - `J 1|2 <bits>`: joystick injection through the shared lines. Port 1 uses row drive, which forward mode already has. Port 2 pulls columns low, which the forward ISR treats as a joystick, as a real one would be.
- **Boot.**
  - The matrix pads reset to input with keeper. The LSF should pass no meaningful drive while the Teensy is unpowered (bench test 6.2.3).
  - In `startup_middle_hook`, set the row pads open-drain released and the column pads input with pull-up. Start the interrupt only after the first (all-open) table is published.
- **`F` (EEPROM).** Add rule 4 from section 3.4: a few ms of no keys, up to 400 ms on a sector erase.
- **Timing.** Add priority 0 for the matrix interrupt to the plan's priorities. The µs-timed `IntervalTimer` edges gain at most about 0.1 µs of jitter.
- **Clock.** 816 MHz; delete the 180 MHz `set_arm_clock()` call.
- **Build.** arduino-cli with a private, patched core, as in TeensyROM. Add it to "Order of work" step 3, ahead of the host link, because CDC traffic is exactly what the patch protects against.
- **Pins.** Kawari J1 and CRESET_N move to 29 and 30.
- **Prototype.** Add the front end (section 6.1) as a new step between plan steps 3 and 4. Then TReadmill's station tests run on the real replacement, not the MT8808.
- **v0.2 PCB.**
  - Remove U2 and add 2× LSF0108 with the bias network.
  - Add do-not-stuff pads for 4.7 kΩ pull-ups from each row to +5 V, in case the bench shows the passive rise is too slow.
  - Decide the passthrough (open question 1).
  - Plus the plan's /RESET, Kawari and ALT circuits and the 100 kΩ base pull-down.

## 6. Bench test plan

### 6.1 Prototype on a v0.1 board

The front end:

- Two LSF0108s on TSSOP adapters, or four BSS138 4-channel breakout boards, which use the same pass-FET principle. Their 10 kΩ pull-ups add about 0.5 mA per line and are fine for a test.
- It sits on a small perfboard with a 20-pin female header that plugs onto J2's long pins. That gives all 16 lines plus +5 V and GND, and keeps the original keyboard off.
- Wire the 16 A-side lines to Teensy pins 14–23, 26, 27, 38–41, and the 3.3 V reference to the Teensy's 3.3 V.

Leave the MT8808 fitted and reset: an open MT8808 leaks about 1 nA, so it is invisible. The firmware selects the direct backend.

### 6.2 Electrical

Run on a PAL and an NTSC C64 (or a Kawari switched both ways), with the original keyboard unplugged unless a test says otherwise.

1. **DC levels** at the READY prompt: a released line ≥ 4.5 V; a line the board pulls low ≤ 0.1 V. Compare a row pulled low through the MT8808.
2. **Edges** on X0 and Y0: fall and rise when the CIA changes a column, when the board pulls and releases a row, and with a joystick switch.
   - Record the released row's time to 2.4 V. That decides whether the row pull-ups get fitted, and sets `t_blank` for a bidirectional mode.
3. **Power sequencing:** C64 power on and off, Teensy reflash with the C64 on (VIN pad cut), C64 reset. No line may go low while the Teensy is in reset, in the bootloader or before `setup()`.
4. **Supply current** from the C64's 5 V at 816 MHz with a USB keyboard attached.

### 6.3 Timing margin (the deciding test)

A test program in machine code, run with interrupts off and the screen blanked, then again with the screen on:

- **Sweep:** for N = 4 to 16 cycles, write a column to `$DC00`, pad with NOP/BIT to N cycles, read `$DC01` and compare with the expected row byte. Run 10 000 passes per N and count errors.
- **Three patterns:**
  - assert: one column from `$FF`
  - release: move from a column with held keys to one without
  - all columns: `$00`, as the KERNAL does
- **Host keys held via `K`:** one key, eight keys in one column, then a ghost rectangle.
- **Pass:** zero errors at N = 4, PAL and NTSC.

**Scope:** trigger on the column's falling edge and measure to the row crossing 0.8 V. Add a debug pin set at ISR entry and cleared at exit. Run persistence for an hour under the stress below. **Pass:** worst case under half the budget (≤ 1 µs).

Stress while measuring:

- USB keyboard typing at full rate, and hot-plugging it (enumeration).
- CDC traffic both ways: `EV` lines on, the host sending `?` in a tight loop.
- TOD running, a timed `X` sequence repeating.
- An `F` write during scanning: the lines must go fully released and come back; no frozen column.
- A/B the core patch: run once without it, to see the stock `usb.c` failure rate, and once with it.

### 6.4 Function

1. **Matrix walk:** each of the 64 keys alone via `K`, read by a forward-scanning program. **Pass:** exactly the right bit every time. RESTORE separately.
2. **Combinations:** SHIFT and CTRL with each key, C= with SHIFT, and a three-key ghost rectangle. **Pass:** `$DC01` matches a real keyboard, including the ghost.
3. **KERNAL typing:**
   - `S` 10 000 characters into a BASIC loop that echoes and counts.
   - RUN/STOP+RESTORE.
   - SHIFT LOCK through Caps Lock.
   - **Pass:** zero dropped or doubled characters.
4. **Joystick coexistence:**
   - Port 2 held up while typing: column 0's keys appear in every column, as on a real keyboard.
   - Port 1 fire held at READY: the real effect.
   - A game that reads keyboard and port 2.
   - **Pass:** same as a real keyboard, and no line carries more than the CIA's pull-up current.
5. **Tight scan loops in real software:**
   - The 1541 Ultimate II menu, the one AmigaKeyboard2C64 found picky, if a U2 is available.
   - A game that reads keys mid-frame (to be picked from TReadmill's library).
   - The section 6.3 program covers the worst case; these confirm it in practice.
6. **Diag cart 586220** with `D` on. **Pass:** the keyboard test passes in forward mode. If it fails, the disassembly in 1.1 was wrong and bidirectional mode becomes a requirement.
7. **Original keyboard in parallel** (only if the passthrough stays): type on both. Then hold the same key on both, confirm the latch of 2.1.4, and confirm it clears on release.
8. **Hang:** stop the Teensy with interrupts off. Record what the C64 sees, then confirm the watchdog releases all lines.

## 7. Open questions

1. **Passthrough:** drop it on v0.2, keep it with the latch documented, or make it a build option?
2. **Bidirectional mode:** worth building only if the diag cart or a real program needs it. Agree to defer it?
3. **Matrix readback (`M ?`) and joystick injection (`J`):** wanted in `Host_Protocol.md` for TReadmill?
4. **Kawari pins:** move J1 and CRESET_N to 29 and 30 in the TReadmill plan now?
5. **Build:** move this sketch to an arduino-cli build with a private, patched core, sharing TeensyROM's patch script?
6. **Thermal:** 816 MHz inside the C64 near the keyboard connector. Any concern, or measure on the bench?
7. **v0.1 boards:** keep the MT8808 backend in the firmware indefinitely?

## Sources

- C64 USB Keyboard repo: `Source/C64_keyboard/*`, `PCB/v0.1 archive/C64toUSBKeyboard.sch`, `.brd`, BOM, `Docs/TReadmill_Support_Plan.md`.
- MT8808 datasheet (Zarlink, Sep 2011) and MT88xx measurement appendix: `ref/Mt8808_DataSheet_Sep11.pdf`, `ref/appendix.pdf` (outside the repo).
- Teensyduino 1.61.0 core: `cores/teensy4/core_pins.h`, `digital.c`, `interrupt.c`, `startup.c`, `usb.c`, `eeprom.c`, `delay.c`, `IntervalTimer.cpp`; `libraries/USBHost_t36/ehci.cpp`, `keyboard.cpp`.
- TeensyROM (SensoriumEmbedded/TeensyROM): PR 103 and `tools/lib/usb-irq-mask.mjs`; `docs/Architecture/Constraints.md`, `Teensy-Firmware.md`, `DMA-Timing-Known-Issues.md`; `Source/Teensy/Teensy.ino`.
- MOS 6526 CIA preliminary datasheets, Mar and Nov 1981: https://6502.org/documents/datasheets/mos/
- C64 schematic 251138: http://www.zimmers.net/anonftp/pub/cbm/schematics/computers/c64/ ; C64 Service Manual 314001-02 (text): https://sx64.net/downloads/C64%20Service%20Manual.txt
- KERNAL disassembly: https://www.pagetable.com/c64ref/c64disasm/ ; Mapping the C64: https://github.com/mist64/c64ref
- 64doc (6502 cycle tables): http://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/64doc
- 586220 diagnostic: manual 326070-01 (http://www.zimmers.net/anonftp/pub/cbm/firmware/misc/c64diag/), dongle schematic (https://ist.uwaterloo.ca/~schepers/diagnostic.html)
- VICE: `src/c64/c64cia1.c`, `src/c64/c64.h`, `testprogs/CIA/ciaports/readme.txt` (https://github.com/VICE-Team/svn-mirror, https://svn.code.sf.net/p/vice-emu/code/)
- codebase64, "Scanning the Keyboard the correct and non-KERNAL way": https://codebase.c64.org/doku.php?id=base:scanning_the_keyboard_the_correct_and_non_kernal_way
- c64-wiki, Control Port: https://www.c64-wiki.com/wiki/Control_Port
- AmigaKeyboard2C64: https://github.com/patrikaxelsson/AmigaKeyboard2C64 ; kernelcrash keyboard-matrix-emulator: https://github.com/kernelcrash/keyboard-matrix-emulator
- NXP i.MX RT1060 datasheet IMXRT1060CEC Rev. 4; AN12078, AN12240: https://www.nxp.com
- Arm Cortex-M7 TRM r1p1: https://documentation-service.arm.com/static/5e906da3c8052b1608760e3d
- PJRC forum threads on fast GPIO, DMA and EEPROM (https://forum.pjrc.com): "Teensy 4.1 how to start using DMA", "Simultaneously reading 8 GPIO pins", "Teensy 4.1 reset EEPROM", "The power consumption of Teensy's processor"; PJRC Teensy 4.1 page; Winbond W25Q64JV datasheet (https://www.pjrc.com/teensy/winbond_w25q64jvxgim.pdf)
- KurtE FlexIO_t4 README: https://github.com/KurtE/FlexIO_t4
- Microchip EOL notices (https://www.microchip.com/product-change-notifications/): CAAN-23SXZK691 (MT8808AP1, MT8812AP1), MFOL-05GIFW725 (MT8816), GBNG-31KWIJ108 (MT8809); DigiKey MT8808AE1 listing (obsolete); OnlineComponents MT8808AP1 listing
- WCH CH446 datasheet CH446DS1 v1D (http://bitsavers.mirrorservice.org/components/wch/_dataSheets/CH446DS1.PDF), https://www.wch.cn/products/CH446.html, LCSC C109471 and C3018882
- Renesas CD22M3494: https://www.renesas.com/en/products/cd22m3494 and datasheet FN2793 Rev 8.00 (May 2014), https://www.renesas.com/en/document/dst/cd22m3494-datasheet ; GreenPAK SLG46826: https://www.renesas.com/en/products/slg46826
- Analog Devices product pages for ADG2188, ADG2128, AD75019, MAX14661, ADG714, ADG715 (https://www.analog.com)
- Microchip ATF1502AS, ATF1504AS (DS20006580A), ATF1508AS (DS20006682A), WinCUPL, ProChip Designer, ATDH1150USB product pages; prjbureau: https://whitequark.github.io/prjbureau/
- Lattice discontinued devices and Answer 913; AMD XCN23009; Intel PDN1709, PDN2041 Rev 2, PDN2401
- Raspberry Pi: RP2040 datasheet; "RP2350 A4, RP2354 and a new hacking challenge" (https://www.raspberrypi.com/news/); forum threads t=398543 and t=375118
- TI datasheets: LSF0108 (Rev M), TXS0108E (Rev L), app note SCEA056, TXB0108 (Rev L), SN74CB3T3245, SN74CBTLV3245A, SN74LVC07A, SN74LVC1G07, SN74LVC125A (Rev T), SN74LVC244A, SN74LVC541A (https://www.ti.com); NXP NVT2008PW (https://www.nxp.com/part/NVT2008PW)
