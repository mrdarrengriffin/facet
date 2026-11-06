// The Backlight slider's whole range is visible: ESPHome maps a light level v (0..1) to the PWM output as
// min_power + (max_power - min_power) * v^gamma_correct, and a backlight driver stays dark below some output. Each
// board's measured floor is boards/<id>.json backlight.floor (an output value, after the light's gamma); its board
// package must use it as min_power, so 1 % on the slider is just visible, the lowest Idle brightness is visible, and
// 0 / off stays fully off (zero_means_zero) (#153).
//   node --test renderer/tools/backlight.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (p) => readFileSync(join(ROOT, p), 'utf8');

// the backlight_pwm output block and the light block of a board package (flat YAML keys, enough for these two)
function backlight(yaml, board) {
  const block = (re) => { const m = yaml.match(re); assert.ok(m, `found ${re}`); return m[0]; };
  const out = block(/- platform: ledc\n\s+id: backlight_pwm\n(?:\s{4}.*\n)+/);
  const light = block(/- platform: monochromatic\n\s+id: backlight\n(?:\s{4}.*\n)+/);
  const val = (s, k, d) => {
    const m = s.match(new RegExp(`^\\s+${k}: (\\S+)`, 'm'));
    if (!m) return d;
    if (m[1] === '${board.backlight.floor}') return board.backlight.floor;
    return +m[1];
  };
  return {
    minRaw: (out.match(/^\s+min_power: (\S+)/m) || [])[1],
    min: val(out, 'min_power', 0), max: val(out, 'max_power', 1), gamma: val(light, 'gamma_correct', 2.8),
    zero: /^\s+zero_means_zero: true/m.test(out),
  };
}

const idleMin = JSON.parse(read('renderer/tokens/settings.json')).settings
  ?.find?.((s) => s.id === 'idleBrightness')?.min;

for (const f of readdirSync(join(ROOT, 'boards')).filter((n) => n.endsWith('.json'))) {
  const board = JSON.parse(read(`boards/${f}`));
  test(`${board.id}: the whole Backlight range lights the glass`, () => {
    const floor = board.backlight?.floor;
    assert.ok(typeof floor === 'number' && floor >= 0 && floor < 1, `boards/${f} backlight.floor (0..1)`);
    const b = backlight(read(`esphome/packages/board_${board.id}.yaml`), board);
    assert.equal(b.minRaw, '${board.backlight.floor}', 'min_power comes from boards/<id>.json backlight.floor');
    assert.ok(b.zero, 'zero_means_zero: 0 / off is fully off');
    const pwm = (pct) => b.min + (b.max - b.min) * (pct / 100) ** b.gamma;
    assert.ok(b.max > b.min, `max_power ${b.max} above min_power ${b.min}`);
    assert.ok(pwm(1) >= floor && pwm(1) < floor + 0.01, `Backlight 1 % = output ${pwm(1).toFixed(3)}, just above ${floor}`);
    assert.ok(typeof idleMin === 'number', 'settings.json has Idle brightness with a min');
    assert.ok(pwm(idleMin) >= floor, `Idle brightness ${idleMin} % = output ${pwm(idleMin).toFixed(3)}, dark below ${floor}`);
  });
}
