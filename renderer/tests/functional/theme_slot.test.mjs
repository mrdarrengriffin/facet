// The panel's look + its one sent-theme slot (core/theme_slot.{h,cpp}, theme engine T4; CONTRACT.md §10, C47): the
// facet_theme action's path (firmware/fw_device.h theme_from_ha -> fx::theme_send), the `look` setting, the stored
// last-good copy and its boot restore. The store is fw_lab's memory stand-in for NVS (theme_slot_mem / _reboot), so
// every path a panel takes runs here. Each test fails without the behaviour it names:
//   accept   a good file: stored, the look moves to Sent, the theme is live, the frame changes
//   refuse   a bad file (each class: structure, out of range (refused, not clamped), oversize): errors with path +
//            line, the live look, its generation, the frame, the store and the setting exactly as before
//   reboot   the stored copy restored at boot (look Sent); a damaged record (checksum, magic, short, too long) =
//            erased + Facet + said; a record the parser refuses = kept + Facet + said; the device still runs
//   drop     facet_theme("") = no sent theme, look back to Facet, store empty
//   budget   a sent file over 16 KB refused before parsing; the load's peak memory within the panel budget
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { frame565, diff565 } from './_release.mjs';

const THEMES = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'themes');
const file = (id) => readFileSync(join(THEMES, `${id}.json`), 'utf8');
const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(a, (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))));
const setting = (t, id, v) => t.b.str(id, (a) => t.b.str(v, (p) => t.b.m.UTF8ToString(t.b.m._fw_setting(a, p))));
const live = (t) => lab(t, 'theme_live');
// a fresh device on Home with an empty store (first boot)
async function boot(t) { await t.device(); lab(t, 'theme_slot_mem'); setting(t, 'look', 'facet'); t.wait(800); }
// a theme file with one change (path a.b.c = value)
const edit = (id, fn) => { const j = JSON.parse(file(id)); fn(j); return JSON.stringify(j, null, 2); };
// the refusal contract: nothing moved
function unchanged(t, before, what) {
  const now = live(t), st = lab(t, 'theme_slot_stored');
  t.eq([now.id, now.gen], [before.live.id, before.live.gen], `${what}: live theme + generation unchanged`);
  t.eq([st.has, st.bytes, st.writes], [before.st.has, before.st.bytes, before.st.writes], `${what}: store unchanged`);
  t.eq(t.setting('look'), before.look, `${what}: look setting unchanged`);
  t.eq(diff565(before.frame, frame565(t)), 0, `${what}: frame unchanged`);
}
const snap = (t) => ({ live: live(t), st: lab(t, 'theme_slot_stored'), look: t.setting('look'), frame: frame565(t) });

export default {
  id: 'theme_slot', family: 'device', about: 'Themes sent to the panel (facet_theme), the Theme (look) setting, the stored copy and boot restore.',
  tests: [
    { name: 'accept: a good file is stored, the look moves to Sent, the theme is live and the screen changes', async run(t) {
      await boot(t); const f0 = frame565(t);
      const r = lab(t, 'theme_slot_send', file('blockcraft')); t.wait(1200);
      t.ok(r.ok, `accepted (${JSON.stringify(r.errors)})`); t.eq(r.stored, true, 'stored'); t.eq(r.id, 'blockcraft', 'id');
      t.eq(t.setting('look'), 'sent', 'look = sent'); t.eq(live(t).id, 'blockcraft', 'live');
      t.eq(lab(t, 'theme_slot_stored').writes, 1, 'one store write');
      t.ok(diff565(f0, frame565(t)) > 0, 'the device frame changed');
      const again = lab(t, 'theme_slot_send', file('blockcraft'));
      t.ok(again.ok && again.stored, 'the same file again: ok'); t.eq(lab(t, 'theme_slot_stored').writes, 1, 'the same file again: no second flash write');
    } },
    { name: 'refuse: structure, unknown key, out of range (refused, not clamped), not JSON: errors with path + line, nothing moves', async run(t) {
      await boot(t); lab(t, 'theme_slot_send', file('win98')); t.wait(1200);
      const cases = [
        ['bad side', edit('blockcraft', (j) => { j.parts = j.parts || {}; j.parts.card = { edges: [{ side: 'diagonal', w: 'stroke:hair', ink: 'text' }] }; }), /side|diagonal/],
        ['unknown key', edit('blockcraft', (j) => { j.sparkle = true; }), /sparkle/],
        ['out of range', edit('blockcraft', (j) => { j.size = { ...(j.size || {}), 'radius:card': 400 }; }), /radius:card/],
        ['not JSON', '{"schema": 1, "id": "x",', /./],
      ];
      for (const [what, json, path] of cases) {
        const before = snap(t);
        const r = lab(t, 'theme_slot_send', json); t.wait(400);
        t.eq(r.ok, false, `${what}: refused`); t.ok(r.n_errors >= 1 && r.errors.length >= 1 && r.errors.length <= 4, `${what}: 1-4 errors listed (${r.n_errors})`);
        t.ok(r.errors.some((e) => path.test(`${e.path} ${e.msg}`)), `${what}: the error names it (${JSON.stringify(r.errors[0])})`);
        if (what !== 'unknown key' && what !== 'out of range') t.ok(r.errors.some((e) => e.line > 0), `${what}: a line number`);
        unchanged(t, before, what);
      }
    } },
    { name: 'budget: a file over 16 KB is refused before parsing; a load stays within the panel memory budget', async run(t) {
      await boot(t); const before = snap(t);
      const big = edit('blockcraft', (j) => { j.about = 'x'.repeat(16 * 1024); });
      const r = lab(t, 'theme_slot_send', big);
      t.eq(r.ok, false, 'oversize refused'); t.has(r.errors.map((e) => e.msg), /at most 16 KB/, 'says the limit'); unchanged(t, before, 'oversize');
      const st = lab(t, 'theme_slot_status'); t.eq([st.max_bytes, st.peak_max], [16384, 98304], 'budgets reported');
      for (const id of ['blockcraft', 'win98']) {
        const l = lab(t, 'theme_load', file(id)); t.ok(l.ok && l.peak > 0 && l.peak <= st.peak_max, `${id}: peak ${l.peak} <= ${st.peak_max}`);
      }
    } },
    { name: 'save failure: the theme still applies, the answer says not stored, nothing restored at reboot', async run(t) {
      await boot(t); lab(t, 'theme_slot_failsave', 'on');
      const r = lab(t, 'theme_slot_send', file('win98')); t.wait(800);
      t.ok(r.ok, 'accepted'); t.eq(r.stored, false, 'stored: false'); t.eq(live(t).id, 'win98', 'live');
      lab(t, 'theme_slot_failsave', ''); const b = lab(t, 'theme_slot_reboot'); t.wait(800);
      t.eq([b.boot, b.live], ['none', 'facet'], 'reboot: nothing stored = Facet');
    } },
    { name: 'reboot: the stored copy is restored at boot (look Sent)', async run(t) {
      await boot(t); lab(t, 'theme_slot_send', file('blockcraft')); t.wait(800);
      const b = lab(t, 'theme_slot_reboot'); t.wait(1200);
      t.eq(b.boot, 'restored', 'restored'); t.eq(live(t).id, 'blockcraft', 'live after reboot'); t.eq(b.sent?.id, 'blockcraft', 'slot');
      t.eq(t.setting('look'), 'sent', 'look still sent');
    } },
    { name: 'corrupt store (checksum, magic, short, too long): erased, Facet, reported; the device still runs', async run(t) {
      for (const how of ['crc', 'magic', 'short', 'long']) {
        await boot(t); lab(t, 'theme_slot_send', file('blockcraft')); t.wait(600);
        lab(t, 'theme_slot_corrupt', how);
        const b = lab(t, 'theme_slot_reboot'); t.wait(800);
        t.has([b.boot], /^corrupt/, `${how}: reported (${b.boot})`); t.eq(b.live, 'facet', `${how}: Facet`); t.eq(b.sent, null, `${how}: no sent theme`);
        t.eq(lab(t, 'theme_slot_stored').has, false, `${how}: erased`);
        t.has([b.why], /no sent theme/, `${how}: the look says why`);
        t.show('settings'); t.wait(600); t.has(t.texts(), 'Theme', `${how}: the device still builds screens`);
      }
    } },
    { name: 'refused at boot (a stored file this firmware refuses): kept, Facet, reported', async run(t) {
      await boot(t); lab(t, 'theme_slot_send', file('blockcraft')); t.wait(600);
      lab(t, 'theme_slot_put', edit('blockcraft', (j) => { j.font = { body: 'Nope Sans' }; }));
      const b = lab(t, 'theme_slot_reboot'); t.wait(800);
      t.has([b.boot], /^refused/, `reported (${b.boot})`); t.eq(b.live, 'facet', 'Facet'); t.eq(lab(t, 'theme_slot_stored').has, true, 'record kept');
    } },
    { name: 'drop: facet_theme("") = no sent theme, look back to Facet, store empty', async run(t) {
      await boot(t); lab(t, 'theme_slot_send', file('win98')); t.wait(600);
      const r = lab(t, 'theme_slot_send', ''); t.wait(800);
      t.ok(r.ok && r.dropped, 'dropped'); t.eq(t.setting('look'), 'facet', 'look facet'); t.eq(live(t).id, 'facet', 'live facet');
      t.eq(lab(t, 'theme_slot_stored').has, false, 'store empty');
    } },
    { name: 'look setting: built-ins go live, Sent with nothing sent = Facet + why, Facet = Facet (default unchanged)', async run(t) {
      await t.device(); lab(t, 'theme_slot_mem');
      t.eq(t.setting('look'), 'facet', 'default look = facet'); t.eq(live(t).id, 'facet', 'default live = facet');
      t.eq(setting(t, 'look', 'win98'), '', 'win98 set'); t.wait(800); t.eq(live(t).id, 'win98', 'win98 live');
      t.eq(setting(t, 'look', 'Blockcraft'), '', 'by label'); t.wait(800); t.eq(live(t).id, 'blockcraft', 'blockcraft live');
      setting(t, 'look', 'sent'); t.wait(800); t.eq(live(t).id, 'facet', 'sent, none: Facet');
      t.has([lab(t, 'theme_slot_status').why], /no sent theme/, 'says why');
      t.ok(setting(t, 'look', 'aero') !== '', 'an unknown look is refused by the setting');
      setting(t, 'look', 'facet'); t.wait(800); t.eq(live(t).id, 'facet', 'facet');
    } },
    { name: 'Settings screen: Mode (light / dark) and Theme (look) are separate rows', async run(t) {
      await t.device(); t.show('settings'); t.wait(800);
      const tx = t.texts(); t.has(tx, 'Mode', 'Mode row'); t.has(tx, 'Theme', 'Theme row');
      lab(t, 'theme_slot_mem'); setting(t, 'look', 'facet'); t.wait(800);
      t.tap('Theme'); t.wait(800); t.tap('Win98'); t.wait(1500);
      t.eq(t.setting('look'), 'win98', 'picked on the panel: look = win98'); t.eq(live(t).id, 'win98', 'win98 live');
      setting(t, 'look', 'facet'); t.wait(800);
    } },
  ],
};
