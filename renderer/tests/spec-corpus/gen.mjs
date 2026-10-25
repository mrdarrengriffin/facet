// Schema-driven specs for the spec corpus (tools/corpus.mjs): every component in renderer/catalogue/catalogue.json at
// its defaults, then one spec per select option, bool flip and range end, so every component / prop / option is built
// at least once. Every text prop is set to a short realistic value, so any other text on the screen is a demo default
// that leaked (the corpus "leak" check).
const TEXT = {
  title: 'Hall light', label: 'Hallway', name: 'Kitchen', text: 'Back door', sub: 'On', status: 'Idle', value: '21',
  unit: '°C', caption: 'Lounge', line: 'Heating', meta: 'just now', action: 'See all', body: 'Next on Tuesday',
  artist: 'Miles Davis', player: 'Lounge', task: 'Bracket', repo: 'facet', age: '2h', time: '09:41', day: 'Saturday',
  date: 'Sat 25 Oct', condition: 'Sunny', temp: '14°', feels: 'Feels 12°', place: 'Wisbech', over: '3,000 ft', detail: '2 min',
  next: '18° at 22:00', prefix: '£', month: 'October', event: 'Bins out', note: 'Soon', options: 'Off,Low,High',
  selected: 'Low', values: '3,5,4,6,7,5,8', items: 'Solar,Grid', ticks: '0,12,24', bands: '1,1,2,3,2,1', n: '3',
  value2: '6.2 kWh', line2: 'Today', line3: 'Cost', line4: '£1.84', code: 'HAM', colour: 'steel', cells: 'a,b,c,d',
};
const safe = (s) => String(s).replace(/[^a-z0-9]+/gi, '_').replace(/^_|_$/g, '').toLowerCase();
const FULL = /_view$|^(f1_circuit|tennis_court|football_pitch|iso_floorplan|bottom_sheet|toast_layer|lightning_radar|calendar_view)$/;

export function generate(cat) {
  const out = [];
  let n = 0;
  for (const [cid, comp] of Object.entries(cat.components).sort()) {
    const props = comp.props || [];
    const base = {};
    for (const p of props) if (p.type === 'text') base[p.name] = TEXT[p.name] ?? (p.name[0].toUpperCase() + p.name.slice(1));
    const variants = [{ tag: 'default', p: {} }];
    for (const p of props) {
      if (p.type === 'select') for (const o of p.options || []) { if (o !== p.default) variants.push({ tag: `${p.name}=${o}`, p: { [p.name]: o }, cover: [p.name, o] }); }
      else if (p.type === 'bool') variants.push({ tag: `${p.name}=${!p.default}`, p: { [p.name]: !p.default } });
      else if (p.type === 'range' && p.max !== undefined) variants.push({ tag: `${p.name}=${p.max}`, p: { [p.name]: p.max } });
    }
    for (const v of variants) {
      const id = `g${n++}`;
      out.push({
        file: `gen/${cid}/${v.tag}`, comp: cid, tag: v.tag, gen: true,
        spec: { v: 1, id, name: comp.name || cid, rev: 1, role: 'both', header: { label: 'Demo', back: true },
          layout: FULL.test(cid) ? 'center' : 'list', nodes: [{ c: cid, p: { ...base, ...v.p } }] },
      });
    }
  }
  return out;
}
export const optionCount = (cat) => Object.values(cat.components).reduce((a, c) => a + (c.props || []).reduce((b, p) => b + (p.type === 'select' ? (p.options || []).length : 1), 0), 0);
export { safe };
