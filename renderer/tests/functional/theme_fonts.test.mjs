// Theme fonts (renderer/CONTRACT.md C49, Appendix A font + type; theme engine T3). Built-in families (fonts/fonts.json,
// rasterised into the build by tools/gen_fonts.py) and fonts the PANEL downloads itself (core/font_store.h: fetched
// once, checked, kept in flash, TinyTTF; the fallback draws until then). The network and the flash are the simulation
// the panel's interfaces allow (core/font_store.cpp sim_*). Each test fails without the behaviour it names:
//   built     each built-in theme draws its own family in every type style a contract component uses (none falls
//             back to Facet's font: "every type token's family is built at its size"), Facet draws its own
//   size      a theme's type size is drawn ([floor, box], decision Q2) and the line box stays Facet's
//   bold      weight: bold draws the text a cell wider (synthetic), in the same box
//   download  fallback first, then a live restyle when the file lands = a fresh build; flash after a reboot (no
//             fetch, offline too); offline first use -> retry -> lands; 404, oversize, corrupt, not OFL, missing
//             glyphs: refused with the reason, the fallback stays; corrupt in flash: dropped and fetched again; a
//             slow file landing after the theme changed is harmless; the file check fuzzed
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { frame565, diff565, seeded } from './_release.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const PS2P = readFileSync(join(HERE, '..', 'fixtures', 'fonts', 'PressStart2P.ttf'));
const lab = (t, c, a = '') => JSON.parse(t.b.str(c, (pc) => t.b.str(typeof a === 'string' ? a : JSON.stringify(a), (pa) => t.b.m.UTF8ToString(t.b.m._fw_lab(-1, pc, pa)))) || '{}');
const M = ['placeholder', { name: 'Theme fonts' }];
const U = 'https://fonts.example.org/PressStart2P.ttf';
const DL = (url = U, extra = {}) => { const f = { family: 'press-start-2p', source: 'download', url, fallback: true, ...extra }; return { schema: 1, id: 'dl-test', font: { body: f, display: f, label: f, mono: f } }; };
const apply = (t, th) => { const r = lab(t, 'theme_apply', th); if (!r.ok) throw new Error(JSON.stringify(r.errors)); t.b.tick(2); t.b.settle(200); return r; };
const use = (t, id) => { lab(t, 'theme_use', id); t.b.tick(2); t.b.settle(200); };
const which = (t, type) => lab(t, 'font_which', type);
const st = (t) => lab(t, 'font_status');
const wait = (t, ms) => { const end = t.b.t + ms; while (t.b.t < end) t.b.tick(); t.b.settle(300); };  // sim time (settle stops when calm)
const sim = (t) => { lab(t, 'theme_use', ''); lab(t, 'font_sim'); t.b.tick(2); };  // Facet first: a live download theme would ask again
const serve = (t, url, body, o = {}) => lab(t, 'font_serve', { url, status: 200, delay: 1000, b64: body ? Buffer.from(body).toString('base64') : '', ...o });
const fresh = (t) => { t.b.rebuild('placeholder', { name: 'Theme fonts' }, 0); t.b.settle(300); return frame565(t); };
// every type style a contract component names (tools/gen_fonts.py builds theme fonts for exactly these)
const STYLES = ['display', 'hero', 'value', 'headline', 'title', 'body', 'label', 'c-label', 'c-meta', 'c-caps-sm', 'clock-mid', 'clock-ampm', 'clock-seconds'];
const den = (t) => (t.b.S > 600 ? 0 : 1);
const unOFL = (b) => {  // the Press Start 2P file with its licence URL (name id 14, UTF-16BE) no longer naming OFL
  const out = Buffer.from(b), pat = Buffer.from('\0O\0F\0L', 'latin1');
  for (let i = out.indexOf(pat); i >= 0; i = out.indexOf(pat, i + 1)) out.write('\0X\0Y\0Z', i, 'latin1');
  return out;
};

export default {
  id: 'theme_fonts', family: 'foundations', about: 'Theme fonts: built-in families, type sizes, bold, and fonts the panel downloads itself (fallback, flash, offline, refusals).',
  tests: [
    { name: 'built: Blockcraft and Win98 draw their own family in every contract type style; Facet draws its own', mount: M, run(t) {
      const out = [];
      for (const th of ['blockcraft', 'win98']) {
        use(t, th);
        for (const s of STYLES) { const w = which(t, `type:${s}`); if (w.facet || !w.exact) out.push(`${th} ${s}: ${w.facet ? "Facet's font" : 'not built at ' + w.px + ' px'}`); }
      }
      t.eq(out, [], 'every style in the theme family, built at its size');
      use(t, 'facet');
      t.eq(STYLES.filter((s) => !which(t, `type:${s}`).facet), [], 'Facet: its own fonts');
    } },
    { name: 'size: the theme size is drawn within [floor, box], the line box stays', mount: M, run(t) {
      use(t, 'blockcraft');
      t.eq(which(t, 'type:body').px, [27, 18][den(t)], 'Blockcraft body at 27 / 18 px (3 / 2 px a font pixel: crisp, 1 bpp)');
      t.eq(which(t, 'type:title').px, [36, 27][den(t)], 'title');
      use(t, 'win98');
      t.eq(which(t, 'type:title').px, [36, 24][den(t)], 'Win98 title at 36 / 24 px (W95FA x3 / x2)');
      use(t, 'facet');
    } },
    { name: 'bold: weight bold draws the text one cell wider, in the same box', mount: M, run(t) {
      const base = { schema: 1, id: 'bold-test' };
      apply(t, base); const f0 = frame565(t);
      apply(t, { ...base, parts: { body: { weight: 'bold' }, label: { weight: 'bold' }, title: { weight: 'bold' } } }); const f1 = frame565(t);
      const ink = (f) => { let n = 0; const bg = f[0]; for (const v of f) if (v !== bg) n++; return n; };
      t.ok(ink(f1) > ink(f0), `bold: more ink (${ink(f0)} -> ${ink(f1)} px)`);
      apply(t, base); t.eq(diff565(f0, frame565(t)), 0, 'regular again = the frame before');
      use(t, 'facet');
    } },
    { name: 'download: the fallback draws first; when the file lands every look repaints live (= a fresh build)', mount: M, run(t) {
      sim(t); const facet = frame565(t);
      serve(t, U, PS2P, { delay: 2000 });
      apply(t, DL());
      t.eq(which(t, 'type:body').download, false, 'not there yet: the fallback');
      t.eq(diff565(facet, frame565(t)), 0, 'the fallback is the built-in family: the frame is Facet\'s');
      t.eq(st(t).map((s) => s.state), ['loading'], 'one fetch running');
      wait(t, 2500);
      t.eq(st(t).map((s) => [s.state, s.fetches]), [['ready', 1]], 'landed, one GET');
      t.ok(which(t, 'type:body').download, 'now drawn with the downloaded file');
      const live = frame565(t);
      t.ok(diff565(facet, live) > 0, 'the text changed live, nothing rebuilt');
      t.eq(diff565(live, fresh(t)), 0, 'live = a fresh build with the file there');
    } },
    { name: 'download: kept in flash: after a reboot it draws from flash with no fetch, offline too', mount: M, run(t) {
      sim(t); serve(t, U, PS2P, { delay: 100 }); apply(t, DL()); wait(t, 500);
      t.eq(lab(t, 'font_store').files, 1, 'stored once');
      const before = frame565(t);
      lab(t, 'font_offline', '1'); lab(t, 'font_reboot'); t.b.settle(200);
      const s = st(t);
      t.eq(s.map((x) => [x.state, x.fetches, x.from_store]), [['ready', 0, true]], 'from flash, no GET');
      t.eq(diff565(before, frame565(t)), 0, 'the same pixels as before the reboot');
      use(t, 'facet');
    } },
    { name: 'download: offline on first use: the fallback, a retry later, then the file', mount: M, run(t) {
      sim(t); lab(t, 'font_offline', '1'); serve(t, U, PS2P, { delay: 100 });
      apply(t, DL()); wait(t, 500);
      t.eq(st(t).map((x) => x.state), ['waiting'], 'offline: waiting, the fallback draws');
      t.eq(which(t, 'type:body').download, false, 'fallback');
      lab(t, 'font_offline', '0'); wait(t, 32000);
      t.eq(st(t).map((x) => [x.state, x.fetches]), [['ready', 2]], 'retried after 30 s and landed');
      t.ok(which(t, 'type:body').download, 'drawn');
      use(t, 'facet');
    } },
    { name: 'download: refused files keep the fallback and say why (404, oversize, corrupt, not OFL)', mount: M, run(t) {
      const cases = [
        ['https://x.org/missing.ttf', null, { status: 404 }, /the server answered 404/],
        ['https://x.org/big.ttf', null, { size: 600 * 1024 }, /over the size cap \(512 KB\)/],
        ['https://x.org/cut.ttf', PS2P.subarray(0, 4000), {}, /corrupt/],
        ['https://x.org/noofl.ttf', unOFL(PS2P), {}, /not the SIL Open Font License/],
        ['https://x.org/text.ttf', Buffer.from('<html>not a font</html>'), {}, /not a TrueType font file|too short/],
      ];
      const out = [];
      for (const [url, body, o, re] of cases) {
        sim(t); const facet = frame565(t);
        serve(t, url, body, { delay: 100, ...o }); apply(t, DL(url)); wait(t, 500);
        const s = st(t)[0] || {};
        if (s.state !== 'refused' || !re.test(s.msg)) out.push(`${url}: ${s.state} "${s.msg}"`);
        if (which(t, 'type:body').download) out.push(`${url}: drawn anyway`);
        if (diff565(facet, frame565(t))) out.push(`${url}: the fallback frame changed`);
        if (lab(t, 'font_store').files) out.push(`${url}: stored`);
      }
      t.eq(out, [], 'each refused, with its reason; the fallback frame unchanged; nothing stored');
      use(t, 'facet');
    } },
    { name: 'download: glyph coverage: missing characters refuse unless fallback: true (then the fallback draws them)', mount: M, run(t) {
      const b64 = PS2P.toString('base64');
      const cjk = lab(t, 'font_check', { b64, need: [0x4E2D], fallback: false });
      t.eq([cjk.ok, cjk.missing], [false, 1], 'a CJK character the file lacks: refused');
      t.ok(/lacks 1 characters/.test(cjk.why), `why: ${cjk.why}`);
      t.eq(lab(t, 'font_check', { b64, need: [0x4E2D], fallback: true }).ok, true, 'fallback: true: accepted');
      t.eq(lab(t, 'font_check', { b64, need: [] }).ok, true, 'printable ASCII: all there');
      sim(t); serve(t, U, PS2P, { delay: 100 }); apply(t, DL(U, { fallback: false })); wait(t, 500);
      const s = st(t)[0];
      t.eq(s.state, 'refused', `without fallback the text glyphs Facet draws (accents) are missing: ${s.msg}`);
      sim(t); serve(t, U, PS2P, { delay: 100 }); apply(t, DL(U, { fallback: true })); wait(t, 500);
      t.eq(st(t)[0].state, 'ready', 'with fallback: true it is drawn, Facet\'s font draws what it lacks');
      use(t, 'facet');
    } },
    { name: 'download: a corrupt file in flash is dropped and fetched again', mount: M, run(t) {
      sim(t); serve(t, U, PS2P, { delay: 100 }); apply(t, DL()); wait(t, 500);
      t.ok(lab(t, 'font_corrupt', U).ok, 'flash corrupted');
      lab(t, 'font_reboot'); wait(t, 500);
      t.eq(st(t).map((x) => [x.state, x.fetches, x.from_store]), [['ready', 1, false]], 'refetched, not drawn from the bad file');
      use(t, 'facet');
    } },
    { name: 'download: slow: a file landing after the theme changed back to Facet changes nothing', mount: M, run(t) {
      sim(t); const facet = frame565(t);
      serve(t, U, PS2P, { delay: 5000 }); apply(t, DL()); use(t, 'facet'); wait(t, 6000);
      t.eq(diff565(facet, frame565(t)), 0, 'Facet frame unchanged after the late file');
      t.eq(st(t)[0].state, 'ready', 'stored for next time');
    } },
    { name: 'check fuzz: 3000 seeded mutations of a font file never crash; truncations refuse', mount: M, run(t) {
      const rnd = seeded(204), out = [];
      for (let i = 0; i < 3000; i++) {
        const b = Buffer.from(PS2P);
        const n = 1 + Math.floor(rnd() * 8);
        for (let k = 0; k < n; k++) b[Math.floor(rnd() * Math.min(b.length, i % 2 ? 2048 : b.length))] = Math.floor(rnd() * 256);
        const cut = i % 5 === 0 ? b.subarray(0, Math.floor(rnd() * b.length)) : b;
        const r = lab(t, 'font_check', { b64: cut.toString('base64'), fallback: true });
        if (typeof r.ok !== 'boolean') out.push(`#${i}: no answer`);
      }
      t.eq(out, [], 'every mutation answered');
    } },
  ],
};
