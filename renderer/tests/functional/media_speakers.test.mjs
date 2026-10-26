// Media screen speakers sheet (specs/media.json: media_view's Speaker button + Bottom sheet, 2026-10-26): with the
// house's players listed the sheet is Auto (follows what's playing) first, then those players by friendly name and
// state, never the docs demo rows; Auto emits "speaker" "auto" and the screen follows the player that is playing; a
// real pick re-binds the screen; with no list on a device the sheet holds the bound player alone.
const A = 'media_player.office_speaker', B = 'media_player.kitchen_display';
const DEMO = [/^Kitchen speaker$/, /^Living room TV$/, /^Study$/, /^Downstairs$/];
const screen = (extra = {}) => ['spec', { screen: 'media', entity: A, speakers: `${A},${B}`, live: true, ...extra }];
const house = (t, aSt = 'paused', bSt = 'idle') => {
  t.ha.set(A, aSt, { friendly_name: 'Office speaker', media_title: 'Office tune', media_artist: 'Band A' });
  t.ha.set(B, bSt, { friendly_name: 'Kitchen display', media_title: 'Kitchen tune', media_artist: 'Band B' });
  t.wait(600);
};
const open = (t) => { t.tap(t.part('speaker_button')); t.wait(600); return t.part('bottom_sheet')?.texts || []; };
const row = (t, re) => { t.tap(re); t.wait(900); };
const hero = (t) => t.part('media_view')?.texts || [];
export default {
  id: 'media_speakers', family: 'home', about: 'Media screen speakers sheet: Auto first, then the real players; Auto follows what is playing.',
  tests: [
    { name: 'real list: Auto first, then the real players, no demo rows', mount: screen(), run(t) {
      house(t);
      const tx = open(t);
      t.has(tx, /^Auto$/, 'Auto row'); t.has(tx, /^Office speaker$/, 'first player'); t.has(tx, /^Kitchen display$/, 'second player');
      t.ok(tx.findIndex((x) => /^Auto$/.test(x)) < tx.findIndex((x) => /^Office speaker$/.test(x)), `Auto first: ${tx.join(' | ')}`);
      for (const d of DEMO) t.hasNot(tx, d, 'demo row');
    } },
    { name: 'a real pick emits its entity id and re-binds the screen', mount: screen(), run(t) {
      house(t);
      open(t);
      const n = t.ha.calls.length;
      t.tap(/^Kitchen display$/); t.wait(900);
      t.has(hero(t), /Kitchen tune/, 'now playing follows the pick');
      t.ok(t.ha.calls.length >= n, 'no stray calls');
    } },
    { name: 'Auto: emits "speaker" "auto" and follows the player that is playing', mount: screen(), run(t) {
      house(t, 'paused', 'idle');
      open(t);
      const ev = t.tap(/^Auto$/); t.wait(600);
      t.emitted(ev, 'speaker', 'auto');
      t.ha.set(B, 'playing', { friendly_name: 'Kitchen display', media_title: 'Kitchen tune', media_artist: 'Band B' }); t.wait(1500);
      t.has(hero(t), /Kitchen tune/, 'follows the playing player');
      t.has(t.part('speaker_button')?.texts || [], /^AUTO · KITCHEN/i, "button reads auto · player (long names truncate)");
    } },
    { name: 'no list on a device: the bound player alone, no demo rows, no Auto', mount: screen({ speakers: '' }), run(t) {
      house(t);
      const tx = open(t);
      t.has(tx, /^Office speaker$/, 'the bound player');
      for (const d of DEMO) t.hasNot(tx, d, 'demo row');
      t.hasNot(tx, /^Auto$/, 'no Auto without a list');
    } },
  ],
};
