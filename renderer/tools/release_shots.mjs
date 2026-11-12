// Pictures for a version's release notes (RELEASING.md "Release notes"): rendered by the renderer itself from the
// docs builds (FW_FEATURES=docs ./build.sh 1.5x and 1x), with the docs site's made-up house (tools/demo_ha.mjs, the
// site deck, tools/demo_art.mjs album art), never from a real panel. Each picture is one or more panels in a bezel,
// side by side, every screen at the same size (360), so a Waveshare (720) and a Guition (480) read as the same 4" glass.
//   node tools/release_shots.mjs <version> <name>=<panel>[+<panel>...] ...
// <panel> = <board>:<screen>[?<param>=<value>&...][;<setting>=<value>...] | file:<png>   board = 720 or 480; screen = a registered screen
// (home, idle, ...) or a spec screen (overhead, media, ...: its sample source when it has one); file: = an existing
// picture (an older version's README picture from git, for a before / after pair; cropped square, scaled to 360);
// <board>:@media / <board>:@idle = the release firmware build (FW_OUT/1.5x, 1x) linked to the functional tests' fake
// Home Assistant with one made-up speaker playing (its demo album art): Media with the art on its CD, the idle screen
// with Idle shows = Album art.
//   node tools/release_shots.mjs 0.5.0 overhead=720:overhead+480:overhead "art=720:media+480:idle;idleShow=art"
//   node tools/release_shots.mjs <version> <name>=wide:<png>   a browser picture (the editor: a fresh screenshot of a real
// panel's editor, never site/public/editor/editor.png): rounded corners, 1600 wide; <name>=phone:<png> = a phone
// screenshot in a phone bezel, 900 high
// Writes release-notes/img/<version>/<name>.png (palette PNG); the notes link them by their raw URL (RELEASING.md).
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { demoValue } from './demo_ha.mjs';
import { serveDemoArt, demoArtUrl } from './demo_art.mjs';
import { Bench, setBuild } from './lib/fwbench.mjs';
import { Ctx } from './lib/fakeha.mjs';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = resolve(HERE, '..');
const sharp = createRequire(join(ROOT, 'site', 'package.json'))('sharp');  // the docs site's (site/node_modules)
const [version, ...jobs] = process.argv.slice(2);
if (!/^\d+\.\d+\.\d+(-[a-z]+\.\d+)?$/.test(version || '') || !jobs.length) {
  console.error('usage: node tools/release_shots.mjs <version> <name>=<board>:<screen>[+...] ...'); process.exit(2);
}
const env = (() => { try { return Object.fromEntries(readFileSync(join(ROOT, 'local.env'), 'utf8').split('\n').map((l) => l.match(/^([A-Z_]+)=(.*)$/)).filter(Boolean).map((m) => [m[1], m[2]])); } catch { return {}; } })();
const OUTDIR = join(resolve(ROOT, process.env.FW_OUT || env.FW_OUT || join(HERE, 'out')), 'docs');
const IMG = join(ROOT, 'release-notes', 'img', version);
mkdirSync(IMG, { recursive: true });

const P = 360, B = 14, R = 32, GAP = 40;  // screen size, bezel, screen corner radius, gap between panels
const mods = {};
async function mod(board) {
  if (mods[board]) return mods[board];
  const dir = join(OUTDIR, board === '480' ? '1x' : '1.5x');
  if (!existsSync(join(dir, 'fw.mjs'))) throw new Error(`no docs build in ${dir} (FW_FEATURES=docs ./build.sh ${board === '480' ? '1x' : '1.5x'})`);
  const F = (await import(pathToFileURL(join(dir, 'fw.mjs')).href + `?${board}`)).default;
  const wasm = await WebAssembly.compile(readFileSync(join(dir, 'fw.wasm')));
  const m = await F({ instantiateWasm: (imports, ok) => { WebAssembly.instantiate(wasm, imports).then((i) => ok(i, wasm)); return {}; }, print: () => {}, printErr: () => {} });
  m._fw_init();
  const str = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
  // the docs site's deck (a Home with every card size), every page but Media with a sample source on it (Media reads
  // the made-up house's speakers, which have album art)
  const deck = JSON.parse(readFileSync(join(ROOT, 'site', 'src', 'data', 'site-deck.json'), 'utf8'));
  for (const p of deck.pages) {
    const sp = spec(p.kind);
    if (p.kind === 'media') p.params = { ...(p.params ?? {}), source: 'ha' };
    else if (sp?.params?.source?.options?.includes('sample')) p.params = { ...(p.params ?? {}), source: 'sample' };
  }
  deck.rev = (deck.rev ?? 1) + 1;
  const r = JSON.parse(str('deck', (a) => str(JSON.stringify(deck), (b) => m.UTF8ToString(m._fw_spec(a, b)))));
  if (!r.ok) throw new Error(`deck refused: ${JSON.stringify(r)}`);
  return (mods[board] = { m, str, S: board === '480' ? 480 : 720, t: 0, put: new Set() });
}
function spec(id) { try { return JSON.parse(readFileSync(join(HERE, 'specs', `${id}.json`), 'utf8')); } catch { return null; } }

// the release build on the fake HA (tools/lib/fakeha.mjs), one made-up speaker playing with its demo art
async function house(board, what) {
  setBuild(resolve(OUTDIR, '..'));
  const b = new Bench(null, board === '480' ? '1x' : '1.5x', {}), t = new Ctx(b);
  const SP = 'media_player.sitting_room_speaker';
  t.ha.s[SP] = { '': 'playing', friendly_name: 'Sitting room', media_title: 'Harvest Moon', media_artist: 'Neil Young', volume_level: '0.45',
    media_position: '118', media_duration: '303', entity_picture: demoArtUrl('sitting_room_speaker') };
  t.ha.answers = true;
  await t.device({ v: 2, rev: 9, start: 'h', pages: [
    { id: 'h', kind: 'home', name: 'Home', cards: [{ t: 'page', page: 'media', size: 'l' }] },
    { id: 'media', kind: 'media', params: { source: 'ha' } }] });
  t.b.spec('act', { do: 'ha.connect', url: 'http://ha.test:8123', token: 'abcdefgh1234' }); t.b.tick(8); t.b.settle(600);
  if (what === 'idle') { t.setting('idleShow', 'art'); t.setting('idleAfter', 60); t.b.events(); t.wait(62000); }
  else t.show(what);
  for (let i = 0; i < 4; i++) { t.wait(1500); t.b.tick(8); t.ha.push(); t.b.settle(400); }
  b.m._fw_refresh(b.stage);
  const p = b.m._fw_frame(b.stage), px = Buffer.from(b.m.HEAPU8.slice(p, p + b.S * b.S * 4));
  for (let i = 3; i < px.length; i += 4) px[i] = 255;
  return sharp(px, { raw: { width: b.S, height: b.S, channels: 4 } }).resize(P, P, { kernel: 'lanczos3' }).png().toBuffer();
}

async function shoot(board, what) {
  if (what.startsWith('@')) return house(board, what.slice(1));
  const [first, ...sets] = what.split(';');
  const [screen, query = ''] = first.split('?');
  const params = Object.fromEntries(new URLSearchParams(query));
  const k = await mod(board), { m, str, S } = k;
  for (const s of sets) { const [id, v] = s.split('='); const e = str(id, (a) => str(v, (b) => m.UTF8ToString(m._fw_setting(a, b)))); if (e) throw new Error(`${id}=${v}: ${e}`); }
  const meta = JSON.parse(m.UTF8ToString(m._fw_meta()));
  const registered = meta.some((c) => c.id === screen && c.section === 'screens');
  const sp = registered ? null : spec(screen);
  if (!registered && !sp) throw new Error(`${screen}: no such screen`);
  const [comp, props] = registered ? (screen === 'home' ? ['device', {}] : [screen, {}])
    : ['spec', { screen, ...(screen === 'media' ? { source: 'ha' } : sp.params?.source?.options?.includes('sample') ? { source: 'sample' } : {}), ...params }];
  const feed = () => {
    for (const [e, a] of JSON.parse(m.UTF8ToString(m._fw_ha_wants()) || '[]')) {
      const key = `${e}|${a}`, v = demoValue(e, a);
      if (k.put.has(key) || v === undefined) continue;
      k.put.add(key);
      str(e, (ep) => str(a, (ap) => str(v, (vp) => m._fw_ha_put(ep, ap, vp))));
    }
  };
  const tick = (n) => { for (let i = 0; i < n; i++) { k.t += 16; m._fw_tick(k.t); serveDemoArt(m); feed(); } m._fw_events(); };
  const st = m._fw_stage(S, S);
  tick(2);
  const e = str(comp, (cp) => str(JSON.stringify(props), (pp) => m.UTF8ToString(m._fw_render(st, cp, pp, 0, 0))));
  if (e) { m._fw_stage_free(st); throw new Error(`${screen}: ${e}`); }
  tick(180);  // ~3 s: data in, pictures decoded, entrance motion settled
  m._fw_refresh(st);
  const p = m._fw_frame(st), b = Buffer.from(m.HEAPU8.slice(p, p + S * S * 4));
  m._fw_stage_free(st);
  for (let i = 3; i < b.length; i += 4) b[i] = 255;
  return sharp(b, { raw: { width: S, height: S, channels: 4 } }).resize(P, P, { kernel: 'lanczos3' }).png().toBuffer();
}

const mask = (w, h, r) => Buffer.from(`<svg width="${w}" height="${h}"><rect width="${w}" height="${h}" rx="${r}" fill="#fff"/></svg>`);
// a screen picture (P x P) in its bezel: the glass rounded, a dark frame with a faint edge
async function panel(screen) {
  const W = P + 2 * B;
  const glass = await sharp(screen).composite([{ input: mask(P, P, R), blend: 'dest-in' }]).png().toBuffer();
  const bezel = Buffer.from(`<svg width="${W}" height="${W}"><rect x="0.5" y="0.5" width="${W - 1}" height="${W - 1}" rx="${R + B}" fill="#141619" stroke="#33373d"/></svg>`);
  return sharp(bezel).composite([{ input: glass, left: B, top: B }]).png().toBuffer();
}
async function picture(spec) {
  if (spec.startsWith('phone:')) {  // a phone screenshot in a dark phone bezel, 900 high
    const { data, info } = await sharp(readFileSync(resolve(spec.slice(6)))).resize({ height: 900 }).png().toBuffer({ resolveWithObject: true });
    const glass = await sharp(data).composite([{ input: mask(info.width, info.height, 40), blend: 'dest-in' }]).png().toBuffer();
    const PB = 18, W = info.width + 2 * PB, H = info.height + 2 * PB;
    const body = Buffer.from(`<svg width="${W}" height="${H}"><rect x="0.5" y="0.5" width="${W - 1}" height="${H - 1}" rx="${40 + PB}" fill="#141619" stroke="#33373d"/></svg>`);
    return sharp(body).composite([{ input: glass, left: PB, top: PB }]).png().toBuffer();
  }
  if (spec.startsWith('wide:')) {
    const img = sharp(readFileSync(resolve(spec.slice(5)))).resize({ width: 1600 });
    const { data, info } = await img.png().toBuffer({ resolveWithObject: true });
    return sharp(data).composite([{ input: mask(info.width, info.height, 20), blend: 'dest-in' }]).png().toBuffer();
  }
  const panels = [];
  for (const one of spec.split('+')) {
    if (one.startsWith('file:')) {
      const src = sharp(readFileSync(resolve(one.slice(5))));
      const { width, height } = await src.metadata(), s = Math.min(width, height);
      panels.push(await panel(await src.extract({ left: 0, top: 0, width: s, height: s }).resize(P, P, { kernel: 'lanczos3' }).flatten({ background: '#000' }).png().toBuffer()));
    } else {
      const [board, what] = [one.slice(0, one.indexOf(':')), one.slice(one.indexOf(':') + 1)];
      if (!['720', '480'].includes(board)) throw new Error(`${one}: board is 720 or 480`);
      panels.push(await panel(await shoot(board, what)));
    }
  }
  const W = P + 2 * B, total = panels.length * W + (panels.length - 1) * GAP;
  return sharp({ create: { width: total, height: W, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } })
    .composite(panels.map((input, i) => ({ input, left: i * (W + GAP), top: 0 }))).png().toBuffer();
}
for (const job of jobs) {
  const at = job.indexOf('='), name = job.slice(0, at);
  const out = join(IMG, `${name}.png`);
  writeFileSync(out, await sharp(await picture(job.slice(at + 1))).png({ palette: true, quality: 95, effort: 10, compressionLevel: 9 }).toBuffer());
  const { width, height } = await sharp(out).metadata();
  console.log(`release_shots: release-notes/img/${version}/${name}.png ${width} x ${height}`);
}
