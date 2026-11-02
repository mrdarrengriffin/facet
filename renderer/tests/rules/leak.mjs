// Sample leak (CONTRACT.md C51a; was the spec corpus's "leak" check): a factory screen never shows its sample data
// once real data is bound. The sample words are DECLARED: what each feed's sample() puts (core/feed.h, lab "feeds").
// Per factory spec with a sample and an ha source, at each density, on a module of its own:
//   ui      the HA source, nothing bound: the words the screen draws whatever the data (labels, units, states)
//   sample  the feeds' sample words that are not ui words (a word every screen shows is not sample data)
//   real    the HA source with an entity per entity param (the first demo entity of its domain) and the fake HA's demo
//           values (tools/demo_ha.mjs, never a real Home Assistant): a sample word on screen here = a leak
// A screen without a sample source has no sample data to leak (noted). Words: a label's whole text, at least 4
// characters and not only digits (a "42" can be both a sample and a real value by chance).
import { readFileSync } from 'node:fs';
import { DEMO, demoValue } from '../../tools/demo_ha.mjs';

const SPECS = new URL('../../specs/', import.meta.url);
const spec = (id) => { try { return JSON.parse(readFileSync(new URL(`${id}.json`, SPECS), 'utf8')); } catch { return null; } };
const word = (t) => { const s = String(t || '').trim(); return s.length >= 4 && !/^[\d\s.,:%°+-]*$/.test(s) ? s : null; };

export default {
  id: 'leak', tiers: ['gate'], ownStage: true, order: 90, looks: [0], cases: 'variants',
  about: 'A factory screen never shows its declared sample data (each feed\'s sample()) once real data is bound (C51a)',
  wants: (c) => c.id === 'spec',
  run({ m, den, v }, rep) {
    const id = v.props?.screen, sp = id && spec(id);
    if (!sp) return;
    const src = sp.params?.source;
    if (!src || !(src.options || []).includes('sample') || !(src.options || []).includes('ha')) { rep.note(`${den} ${id}: no sample source`); return; }
    const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
    const W = den === '1x' ? 480 : 720;
    let t = m.__t || 0;
    const tick = (n) => { for (let i = 0; i < n; i++) { t += 16; m._fw_tick(t); } m._fw_events(); m.__t = t; };
    const texts = (props, real) => {
      const st = m._fw_stage(W, W);
      const e = S('spec', (pc) => S(JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(st, pc, pp, 0, -1))));
      if (e) { m._fw_stage_free(st); throw new Error(`${id}: render ${e}`); }
      if (real) for (const [en, a] of JSON.parse(m.UTF8ToString(m._fw_ha_wants()) || '[]')) {
        const val = demoValue(en, a);
        if (val !== undefined) S(en, (pe) => S(a, (pa) => S(String(val), (pv) => m._fw_ha_put(pe, pa, pv))));
      }
      tick(real ? 190 : 63);
      for (let calm = 0, i = 0; i < 150 && calm < 3; i++) { tick(1); calm = m._fw_anims() ? 0 : calm + 1; }
      const os = JSON.parse(m.UTF8ToString(m._fw_audit(st)));
      m._fw_stage_free(st);
      return new Set(os.filter((o) => o.t === 'label' && o.w > 0 && o.h > 0).map((o) => word(o.text)).filter(Boolean));
    };
    // real: each entity param gets the first demo entity of its domain
    const params = {};
    for (const [k, p] of Object.entries(sp.params || {}))
      if ((p.type === 'entity' || p.type === 'entities') && p.domain && !p.default) {
        const e = Object.keys(DEMO).find((x) => x.startsWith(`${p.domain}.`));
        if (e) params[k] = e;
      }
    const st0 = m._fw_stage(W, W);
    const declared = JSON.parse(S('feeds', (pc) => S('', (pa) => m.UTF8ToString(m._fw_lab(st0, pc, pa))))).flatMap((f) => f.samples);
    m._fw_stage_free(st0);
    const ui = texts({ screen: id, source: 'ha' }, false);
    const sample = [...new Set(declared.map(word).filter(Boolean))].filter((w) => !ui.has(w));
    const real = texts({ screen: id, source: 'ha', ...params }, true);
    const leaks = sample.filter((w) => real.has(w));
    for (const w of leaks.slice(0, 3)) rep.check(false, `${den} ${id}: the sample "${w}" shows with real data bound (${JSON.stringify(params)})`);
    if (!leaks.length) rep.check(true, `${den} ${id}: no sample data with real data bound (${sample.length} sample words)`);
  },
};
