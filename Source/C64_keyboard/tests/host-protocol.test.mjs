// The keyboard board's host protocol (Docs/Host_Protocol.md), run on the PC:
// the firmware's own Engine, parser, text table and line reader, compiled with a
// simulated clock and outputs (harness.cpp).
//
//   node --test "Source/C64_keyboard/tests/*.test.mjs"
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const SKETCH = path.resolve(import.meta.dirname, '..');
const REPO = path.resolve(SKETCH, '../..');
const SOURCES = [
  'tests/harness.cpp', 'Engine.cpp', 'HostCmd.cpp', 'HostPort.cpp', 'TextKeys.cpp', 'LineReader.cpp', 'Fitted.cpp', 'Fmt.cpp',
  'PhysKeys.cpp',
];

const onPath = (cmd) => spawnSync(process.platform === 'win32' ? 'where' : 'which', [cmd], { stdio: 'ignore' }).status === 0;
const skip = !onPath('c++') && 'c++ is not on PATH';

let exe;
function harness() {
  if (exe) return exe;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'kb-harness-'));
  exe = path.join(dir, process.platform === 'win32' ? 'harness.exe' : 'harness');
  const built = spawnSync('c++', ['-std=gnu++17', '-Wall', '-Wextra', '-Werror', '-o', exe, ...SOURCES], { cwd: SKETCH, encoding: 'utf8' });
  assert.equal(built.status, 0, built.stderr);
  return exe;
}

function run(lines) {
  const ran = spawnSync(harness(), { input: lines.join('\n') + '\n', encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stdout + ran.stderr);
  return ran.stdout.split(/\r?\n/).filter(Boolean);
}

const replies = (out) => out.filter((l) => l.startsWith('< ')).map((l) => l.slice(2));
const edges = (out) => out.filter((l) => l.startsWith('= ')).map((l) => l.slice(2));
const field = (line, key) => line.match(new RegExp(`(?:^| )${key}=(\\S+)`))?.[1];
const T = 1000000; // the harness clock's start

// --- lines -------------------------------------------------------------------

test('? reports identity and state, with - for what is not fitted', { skip }, () => {
  assert.deepEqual(replies(run(['boot 0', '> ?'])), [
    'OK ? proto=1 name=C64USBKeyboard fw=1.0.0 build=2026-01-02T03:04:05 chip=12345678 mcu=teensy41 ' +
      'fitted=- held=- reset=- tod=- j1=- loop=off e=1',
  ]);
});

test("the TR's dv token with no LF is dropped after 500 ms, so the next command works", { skip }, () => {
  assert.deepEqual(replies(run(['boot 0', 'raw 64 76', '+500000', '> TIME'])), [`OK TIME us=${T + 500000}`]);
  // inside the 500 ms the bytes are still there, and spoil the next line
  assert.deepEqual(replies(run(['boot 0', 'raw 64 76', '+100000', '> TIME'])), ['ERR * syntax']);
});

test('empty lines and CRs are ignored', { skip }, () => {
  assert.deepEqual(replies(run(['boot 0', '>', 'raw 0d 0a', 'raw 3f 0d 0a'])), [replies(run(['boot 0', '> ?']))[0]]);
});

test('a non-printable byte or a line over 120 characters is dropped with ERR * syntax', { skip }, () => {
  assert.deepEqual(replies(run(['boot 0', 'raw 3f 01 0a'])), ['ERR * syntax']);
  assert.deepEqual(replies(run(['boot 0', `> K ${'1'.repeat(119)}`])), ['ERR * syntax']);
  const longest = `S ${'a'.repeat(118)}`; // 120 characters
  assert.equal(longest.length, 120);
  assert.equal(field(replies(run(['boot 0', `> ${longest}`, '+12000000']))[0], 'n'), '118');
});

test('bad commands get syntax or range errors, and do nothing', { skip }, () => {
  const out = run([
    'boot 8', '> HELLO', '> K 18 d', '> K 1 d', '> K 12 x', '> P 12 0', '> R 5', '> N 10001',
    '> X', '> X w 6000;w 5000', `> X ${Array(17).fill('w 1').join(';')}`, '> X d 12;', '> ? extra',
    `> K ${Array(17).fill('12').join('+')} d`, '> T 55', '> F foo 1', '> V open 5',
  ]);
  assert.deepEqual(replies(out), [
    'ERR * syntax', 'ERR K range', 'ERR K syntax', 'ERR K syntax', 'ERR P range', 'ERR R range', 'ERR N range',
    'ERR X syntax', 'ERR X range', 'ERR X range', 'ERR X syntax', 'ERR ? syntax',
    'ERR K range', 'ERR T syntax', 'ERR F syntax', 'ERR V range',
  ]);
  assert.deepEqual(edges(out), ['fitted reset @1000000']);
});

test('command names and keywords are case-insensitive', { skip }, () => {
  assert.deepEqual(replies(run(['boot 8', '> k 12 D', '> r HOLD', '> time'])), [
    `OK K t=${T}`, `OK R t=${T}`, `OK TIME us=${T}`,
  ]);
});

// --- keys --------------------------------------------------------------------

test('K closes a + list together and releases it; releasing an open key is fine', { skip }, () => {
  const out = run(['boot 0', '> K 17+12 d', '> K 17+12 u', '> K 12 u']);
  assert.deepEqual(edges(out).slice(1), [`m 12+17 @${T}`, `m - @${T}`]);
  assert.deepEqual(replies(out), [`OK K t=${T}`, `OK K t=${T}`, `OK K t=${T}`]);
});

test('a K hold times out at max_ms with an EV line, even with E 0', { skip }, () => {
  const out = run(['boot 0', '> E 0', '> K 77 d 100', '+99900', 'active', '+200', 'active']);
  assert.deepEqual(out.filter((l) => l.startsWith('active')), ['active 1', 'active 0']);
  assert.deepEqual(edges(out).slice(1), [`m 77 @${T}`, `m - @${T + 100000}`]);
  assert.deepEqual(replies(out).slice(-1), ['EV K 77 timeout']);
});

test('P holds for hold_ms, reports the closing and opening edges, and replies after the gap', { skip }, () => {
  const out = run(['boot 0', '> P 12 100 20', '+100000', 'active', '+20000']);
  assert.deepEqual(edges(out).slice(1), [`m 12 @${T}`, `m - @${T + 100000}`]);
  const ok = out.indexOf(`< OK P t0=${T} t1=${T + 100000}`);
  assert.ok(ok > out.indexOf('active 1'), out.join('\n'));
});

test('host keys are OR-ed: P and S do not open a key that K holds', { skip }, () => {
  const out = run(['boot 0', '> K 12 d', '> P 12 10 10', '+20000', '> S a', '+100000']);
  assert.deepEqual(edges(out), [`fitted - @${T}`, `m 12 @${T}`]);
  assert.equal(replies(out).length, 3);
});

// --- busy, abort and safety ----------------------------------------------------

test('while a timed action runs, output commands are busy and queries are answered', { skip }, () => {
  const out = replies(run(['boot 0', '> P 12 1000', '> K 01 d', '> ?', '> TIME', '> E 0', '> F ?', '+1100000']));
  assert.equal(out[0], 'ERR K busy');
  assert.match(out[1], /^OK \? .* held=- /);
  assert.deepEqual(out.slice(2), [`OK TIME us=${T}`, 'OK E e=0', 'OK F fitted=-', `OK P t0=${T} t1=${T + 1000000}`]);
});

test('A aborts a timed action: its ERR comes first, then OK A', { skip }, () => {
  const out = run(['boot 0', '> P 12 1000', '+500000', '> A']);
  assert.deepEqual(replies(out), ['ERR P aborted', 'OK A']);
  assert.deepEqual(edges(out).slice(1), [`m 12 @${T}`, `m - @${T + 500000}`]);
});

test('A releases K holds, R hold and the loopback, and nothing else', { skip }, () => {
  const out = run(['boot 9', '> K 12 d', '> D on', '> R hold', '> A', '> ?']);
  assert.deepEqual(edges(out), [
    `fitted tod,reset @${T}`, `tod 50 @${T}`,
    `m 12 @${T}`, `m 00+11+12+22+33+44+55+66+77 @${T}`, `reset 1 @${T}`, `m - @${T}`, `reset 0 @${T}`,
  ]);
  const ident = replies(out).at(-1);
  assert.deepEqual([field(ident, 'held'), field(ident, 'loop'), field(ident, 'reset'), field(ident, 'tod')], ['-', 'off', '0', '50']);
});

test('a dropped DTR releases every hold, aborts the action and turns E back on', { skip }, () => {
  const out = run(['boot 8', '> K 12 d', '> R hold', '> E 0', '> P 34 1000', 'dtr0', 'dtr1', '> ?']);
  assert.deepEqual(edges(out), [`fitted reset @${T}`, `m 12 @${T}`, `reset 1 @${T}`, `m 12+34 @${T}`, `m 34 @${T}`, `reset 0 @${T}`, `m - @${T}`]);
  const r = replies(out);
  assert.ok(!r.includes('ERR P aborted'), 'the closed port gets no reply');
  assert.deepEqual([field(r.at(-1), 'e'), field(r.at(-1), 'held'), field(r.at(-1), 'reset')], ['1', '-', '0']);
});

test('bytes that arrive while the port is closed are discarded, not run', { skip }, () => {
  // a host that wrote these and closed at once, or never asserted DTR
  const out = run(['boot 0', 'dtr0', '> D on', '> K 12 d', '>> P 1', 'dtr1', '> ?']);
  assert.deepEqual(edges(out).slice(1), []);
  const r = replies(out);
  assert.equal(r.length, 1, r.join('\n'));
  assert.deepEqual([field(r[0], 'loop'), field(r[0], 'held')], ['off', '-']);
});

test('a hold that times out never aborts a timed action', { skip }, () => {
  const r = replies(run(['boot 0', '> K 12 d 10', '> P 77 100 0', '+200000']));
  assert.deepEqual(r, [`OK K t=${T}`, 'EV K 12 timeout', `OK P t0=${T} t1=${T + 100000}`]);
});

test('? is not cut short with all 64 keys held', { skip }, () => {
  const all = Array.from({ length: 64 }, (_, k) => `${k >> 3}${k & 7}`);
  const lines = ['boot 31'];
  for (let i = 0; i < 64; i += 16) lines.push(`> K ${all.slice(i, i + 16).join('+')} d`);
  const ident = replies(run([...lines, '> ?'])).at(-1);
  assert.equal(field(ident, 'held'), all.join('+'));
  assert.match(ident, / loop=off e=1$/);
});

test('E 0 silences key and debug EV lines only', { skip }, () => {
  const out = replies(run(['boot 0', 'ev key 0x04 d', '> E 0', 'ev key 0x04 u', '> E 1', 'ev f12 mt8808-reset']));
  assert.deepEqual(out, ['EV key 0x04 d', 'OK E e=0', 'OK E e=1', 'EV f12 mt8808-reset']);
});

// --- typed text ----------------------------------------------------------------

test('S maps characters and names to C64 keys', { skip }, () => {
  assert.deepEqual(run(['strokes a"A{return}{clr}{ctrl-9}{c=-1}{shift-a}{restore}\\^_{RETURN}']), [
    '12 73s 12s 01 63s 40c 70k 12s restore 60 66 71 01',
  ]);
  const printable = Array.from({ length: 95 }, (_, i) => String.fromCharCode(32 + i)).filter((c) => !'`{|}~'.includes(c)).join('');
  assert.equal(run([`strokes ${printable}`])[0].split(' ').length, 90);
  for (const [text, pos] of [['a|b', 1], ['{nope}', 0], ['{return', 0], ['{ctrl-restore}', 0], ['ab~', 2], ['}', 0]]) {
    assert.deepEqual(run([`strokes ${text}`]), [`err ${pos}`], text);
  }
});

test('S closes SHIFT 20 ms before its key, opens it 20 ms after, and replies after the last gap', { skip }, () => {
  const out = run(['boot 0', '> S a"', '+239900', 'active', '+100']);
  assert.deepEqual(edges(out).slice(1), [
    `m 12 @${T}`, `m - @${T + 50000}`,
    `m 17 @${T + 100000}`, `m 17+73 @${T + 120000}`, `m 17 @${T + 170000}`, `m - @${T + 190000}`,
  ]);
  assert.deepEqual(out.slice(-2), ['active 1', `< OK S n=2 t0=${T} t1=${T + 190000}`]);
  // CTRL and C= the same way
  assert.deepEqual(edges(run(['boot 0', '> S {ctrl-9}{c=-1}', '+300000'])).slice(1), [
    `m 72 @${T}`, `m 40+72 @${T + 20000}`, `m 72 @${T + 70000}`, `m - @${T + 90000}`,
    `m 75 @${T + 140000}`, `m 70+75 @${T + 160000}`, `m 75 @${T + 210000}`, `m - @${T + 230000}`,
  ]);
});

test('S checks the whole line before typing, keeps spaces, and caps its total time', { skip }, () => {
  const bad = run(['boot 0', '> S ab|c']);
  assert.deepEqual(replies(bad), ['ERR S range']);
  assert.deepEqual(edges(bad).slice(1), []);
  assert.equal(field(replies(run(['boot 0', '> S a  b', '+1000000']))[0], 'n'), '4');
  assert.deepEqual(replies(run(['boot 0', '> ST 10000 10000 abcd'])), ['ERR ST range']);
  assert.deepEqual(replies(run(['boot 0', '> S'])), ['ERR S syntax']);
  // the modifier lead counts toward the 60 s: 3 x (10000 + 9960 + 40) is exactly 60 s
  assert.deepEqual(replies(run(['boot 0', '> ST 10000 9960 AAA'])), []);
  assert.deepEqual(replies(run(['boot 0', '> ST 10000 9961 AAA'])), ['ERR ST range']);
});

test('ST sets its own hold and gap, each at least 20 ms, so a repeated key is seen open between', { skip }, () => {
  const out = run(['boot 0', '> ST 20 20 aa', '+80000']);
  assert.deepEqual(edges(out).slice(1), [`m 12 @${T}`, `m - @${T + 20000}`, `m 12 @${T + 40000}`, `m - @${T + 60000}`]);
  assert.deepEqual(replies(out), [`OK ST n=2 t0=${T} t1=${T + 60000}`]);
  assert.deepEqual(replies(run(['boot 0', '> ST 19 20 a', '> ST 20 19 a', '> ST 20 0 a'])), [
    'ERR ST range', 'ERR ST range', 'ERR ST range',
  ]);
});

// --- fitted outputs ------------------------------------------------------------

test('commands for unfitted outputs get notfitted; queries answer with -', { skip }, () => {
  assert.deepEqual(replies(run([
    'boot 0', '> R', '> R hold', '> B menu', '> T 60', '> V open', '> X r 100', '> X b alt 10', '> T ?', '> V ?',
  ])), [
    'ERR R notfitted', 'ERR R notfitted', 'ERR B notfitted', 'ERR T notfitted', 'ERR V notfitted',
    'ERR X notfitted', 'ERR X notfitted', 'OK T tod=-', 'OK V j1=- creset=-',
  ]);
  assert.deepEqual(replies(run(['boot 16', '> V open'])), ['ERR V notfitted']); // V needs reset too
});

test('F applies and saves only on a change', { skip }, () => {
  const out = run(['boot 0', '> F reset 1', '> F reset 1', '> F ?', '> F tod 1', '> F TOD 0']);
  assert.deepEqual(edges(out), [
    `fitted - @${T}`,
    `fitted reset @${T}`, `save reset @${T}`,
    `fitted tod,reset @${T}`, `tod 50 @${T}`, `save tod,reset @${T}`,
    `fitted reset @${T}`, `save reset @${T}`,
  ]);
  assert.deepEqual(replies(out), [
    'OK F fitted=reset', 'OK F fitted=reset', 'OK F fitted=reset', 'OK F fitted=tod,reset', 'OK F fitted=reset',
  ]);
});

test('the EEPROM record: blank, foreign, newer or damaged data loads the defaults', { skip }, () => {
  assert.deepEqual(run([
    'fitenc 9', 'fitrec 4b 42 01 09 5b', 'fitrec ff ff ff ff ff', 'fitrec 4b 42 01 09 5c', 'fitrec 4b 42 02 09 58', 'fitrec 4b 42 01 29 7b',
  ]), ['rec 4b 42 01 09 5b', 'fit tod,reset', 'fit defaults', 'fit defaults', 'fit defaults', 'fit defaults']);
});

// --- reset, buttons, TOD, Kawari -------------------------------------------------

test('R pulses /RESET for exactly the asked width, default 50 ms', { skip }, () => {
  const out = run(['boot 8', '> R 150', '+1000', '> R', '+60000']);
  assert.deepEqual(edges(out).slice(1), [
    `reset 1 @${T}`, `reset 0 @${T + 150}`, `reset 1 @${T + 1000}`, `reset 0 @${T + 51000}`,
  ]);
  assert.deepEqual(replies(out), [`OK R t0=${T} t1=${T + 150}`, `OK R t0=${T + 1000} t1=${T + 51000}`]);
});

test('R hold releases on R release, or at its max with an EV line', { skip }, () => {
  const held = run(['boot 8', '> R hold', '+1000', '> R release']);
  assert.deepEqual(edges(held).slice(1), [`reset 1 @${T}`, `reset 0 @${T + 1000}`]);
  const timed = run(['boot 8', '> R hold 50', '+60000']);
  assert.deepEqual(edges(timed).slice(1), [`reset 1 @${T}`, `reset 0 @${T + 50000}`]);
  assert.equal(replies(timed).at(-1), 'EV R timeout');
});

test('B pulses the fitted TR button', { skip }, () => {
  const out = run(['boot 2', '> B menu 20', '+30000', '> B alt']);
  assert.deepEqual(edges(out).slice(1), [`menu 1 @${T}`, `menu 0 @${T + 20000}`]);
  assert.deepEqual(replies(out), [`OK B t0=${T} t1=${T + 20000}`, 'ERR B notfitted']);
});

test('T sets the TOD clock, F9 toggles 50/60, and both need tod fitted', { skip }, () => {
  const out = run(['boot 1', '> T 60', 'f9', '> T off', '> T ?']);
  assert.deepEqual(edges(out), [`fitted tod @${T}`, `tod 50 @${T}`, `tod 60 @${T}`, `tod 50 @${T}`, `tod off @${T}`]);
  assert.deepEqual(replies(out), ['OK T tod=60', 'EV f9 tod=50', 'OK T tod=off', 'OK T tod=off']);
  assert.deepEqual(replies(run(['boot 0', 'f9'])), ['EV f9 tod notfitted']);
});

test('V holds /RESET, sets J1, pulses CRESET_N, waits restart_ms, releases', { skip }, () => {
  const out = run(['boot 24', '> V closed 100', '+110000', '> V ?', '> ?']);
  assert.deepEqual(edges(out).slice(1), [
    `reset 1 @${T}`, `j1 closed @${T}`, `creset 1 @${T + 1000}`, `creset 0 @${T + 2000}`, `reset 0 @${T + 102000}`,
  ]);
  const r = replies(out);
  assert.equal(r[0], `OK V j1=closed t0=${T} t1=${T + 102000}`);
  assert.equal(r[1], 'OK V j1=closed creset=released');
  assert.deepEqual([field(r[2], 'j1'), field(r[2], 'reset')], ['closed', '0']);
});

test('V is never cut short once CRESET_N is pulsed: A waits for the end', { skip }, () => {
  for (const wait of [1500, 50000]) {
    const out = run(['boot 24', '> V open 100', `+${wait}`, '> A', 'active', '+110000']);
    assert.equal(edges(out).at(-1), `reset 0 @${T + 102000}`, `A after ${wait} µs`);
    assert.deepEqual(replies(out), ['ERR V aborted', 'OK A']);
    assert.ok(out.indexOf('active 1') < out.indexOf('< ERR V aborted'));
  }
});

test('V aborted before the CRESET_N pulse releases /RESET at once and puts J1 back', { skip }, () => {
  const out = run(['boot 24', '> V closed', '> A', '> V ?']);
  assert.deepEqual(edges(out).slice(1), [`reset 1 @${T}`, `j1 closed @${T}`, `j1 open @${T}`, `reset 0 @${T}`]);
  assert.deepEqual(replies(out), ['ERR V aborted', 'OK A', 'OK V j1=open creset=released']);
});

test('a DTR drop during V sends nothing from the old session to the next one', { skip }, () => {
  const out = run(['boot 24', '> V open 100', '+1500', '> A', 'dtr0', 'dtr1', '> TIME', '> A', '+110000']);
  assert.equal(edges(out).at(-1), `reset 0 @${T + 102000}`);
  // the new session's A still gets its OK A, after V ends; nothing else arrives
  assert.deepEqual(replies(out), [`OK TIME us=${T + 1500}`, 'OK A']);
});

test('a hold timeout during V does not release /RESET early', { skip }, () => {
  const out = run(['boot 24', '> R hold 1', '> R release', '> R hold 5', '+1000', '> V open 100', '+110000']);
  assert.ok(out.includes('< EV R timeout'));
  assert.equal(edges(out).filter((e) => e.startsWith('reset 0')).at(-1), `reset 0 @${T + 103000}`);
});

// --- sequences -------------------------------------------------------------------

test('X runs RUN/STOP+RESTORE as one action', { skip }, () => {
  const out = run(['boot 0', '> X d 77;w 50;n 50;w 50;u 77', '+200000']);
  assert.deepEqual(edges(out).slice(1), [
    `m 77 @${T}`, `restore 1 @${T + 50000}`, `restore 0 @${T + 100000}`, `m - @${T + 150000}`,
  ]);
  assert.deepEqual(replies(out), [`OK X steps=5 t0=${T} t1=${T + 150000}`]);
});

test('X releases whatever it left down when it ends', { skip }, () => {
  const out = run(['boot 0', '> X d 12;w 10', '+20000']);
  assert.deepEqual(edges(out).slice(1), [`m 12 @${T}`, `m - @${T + 10000}`]);
  assert.deepEqual(replies(out), [`OK X steps=2 t0=${T} t1=${T + 10000}`]);
});

test('X can hold keys through a reset', { skip }, () => {
  const out = run(['boot 8', '> X d 75;r 50000;w 2000;u 75', '+2100000']);
  assert.deepEqual(edges(out).slice(1), [
    `m 75 @${T}`, `reset 1 @${T}`, `reset 0 @${T + 50000}`, `m - @${T + 2050000}`,
  ]);
});

// --- the USB keyboard and the matrix ------------------------------------------------

test('a C64 switch that two USB keys close stays closed until both are up', { skip }, () => {
  // CapsLock latches L-SHIFT; tapping L-Shift (USB 0x68) must not open it
  assert.deepEqual(run(['usb caps', 'usb 68 d', 'usb 68 u', 'usb caps']), ['phys 17', 'phys 17', 'phys 17', 'phys -']);
  // R-Shift held, cursor up (0x52, typed as SHIFT+CRSR) tapped: R-SHIFT stays closed
  assert.deepEqual(run(['usb 6c d', 'usb 52 d', 'usb 52 u', 'usb 6c u']), ['phys 64', 'phys 07+64', 'phys 64', 'phys -']);
  // Backspace and Delete are both INST/DEL
  assert.deepEqual(run(['usb 2a d', 'usb 4c d', 'usb 2a u', 'usb 4c u']), ['phys 00', 'phys 00', 'phys 00', 'phys -']);
  assert.deepEqual(run(['usb 2b d']), ['not a C64 key', 'phys -']); // TAB is RESTORE
});

test('F11 holds the loopback diagonal until F12, which drops every USB keyboard closure', { skip }, () => {
  const diag = 'phys 00+11+22+33+44+55+66+77';
  assert.deepEqual(run(['usb 04 d', 'usb f11', 'usb 1a d', 'usb 1a u', 'usb caps', 'usb f12', 'usb 04 u']), [
    'phys 12', diag, diag, diag, 'phys 00+11+17+22+33+44+55+66+77', 'phys -', 'phys -',
  ]);
});

test('the matrix closes SHIFT, CTRL and C= before other keys and opens them after', { skip }, () => {
  const mask = (...keys) => keys.reduce((m, k) => m | (1n << BigInt(parseInt(k, 8))), 0n).toString(16);
  assert.deepEqual(run([
    `order 0 ${mask('07', '64')}`, `order ${mask('07', '64')} 0`, `order ${mask('17', '12')} ${mask('34', '72')}`,
  ]), ['order 64+ 07+', 'order 07- 64-', 'order 12- 17- 72+ 34+']);
});

// --- key numbers -------------------------------------------------------------------

test("the firmware's C64KP codes match the protocol's key table", { skip }, () => {
  assert.deepEqual(run(['roundtrip']), ['roundtrip ok']);

  // The 8x8 table in Docs/Host_Protocol.md, by cell text
  const doc = fs.readFileSync(path.join(REPO, 'Docs/Host_Protocol.md'), 'utf8');
  const cell = new Map();
  for (const row of doc.matchAll(/^\| PA(\d) \|(.*)\|\r?$/gm)) {
    row[2].split('|').map((s) => s.trim()).forEach((name, pb) => cell.set(name, `${row[1]}${pb}`));
  }
  assert.equal(cell.size, 64);

  const names = {
    DEL: 'DEL', RTN: 'RETURN', HORZ_CSR: 'CRSR ⇔', VERT_CSR: 'CRSR ⇕', L_SHIFT: 'L-SHIFT', R_SHIFT: 'R-SHIFT',
    HOME: 'CLR/HOME', PLUS: '+', MINUS: '−', PERIOD: '.', COLON: ':', AT: '@', COMMA: ',', POUND: '£',
    ASTERISK: '*', SEMICOL: ';', EQUAL: '=', ARR_UP: '↑', SLASH: '/', ARR_LEFT: '←', CTRL: 'CTRL', SPACE: 'SPACE',
    COMMODORE: 'C=', RUN_STOP: 'RUN/STOP', ZERO: '0',
  };
  const header = fs.readFileSync(path.join(SKETCH, 'C64keyboard.h'), 'utf8');
  const codes = [...header.matchAll(/^#define C64KP_(\w+)\s+0x([0-9A-F]{2})\s/gm)].filter(([, n]) => n !== 'IGNORE');
  assert.equal(codes.length, 64);
  const got = run(codes.map(([, , hex]) => `c64kp ${hex}`)).map((l) => l.replace('key ', ''));
  codes.forEach(([, name], i) => {
    const want = cell.get(names[name] ?? name);
    assert.equal(got[i], want, `C64KP_${name}`);
  });
});

test('build stamps are formatted without spaces', { skip }, () => {
  assert.deepEqual(run(['build Nov 16 2026|14:41:00', 'build Jan  5 2027|09:03:07', 'build Foo 16 2026|14:41:00']), [
    'build ok 2026-11-16T14:41:00', 'build ok 2027-01-05T09:03:07', 'build bad unknown',
  ]);
});
