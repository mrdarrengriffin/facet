// Made-up album art for the demo media players (tools/demo_ha.mjs entity_picture): the fake Home Assistant's picture
// proxy. The tests' fake image host (tools/lib/fakeha.mjs), the docs site's sample HA (site/src/lib/sample-ha.ts) and
// the workbench without a Home Assistant (workbench/src/pages/api/panel/ha-img.ts) all answer a demo picture URL from
// here: drawn in code (no image files), the same pixels everywhere. Plain JS: node and the site's bundler load it.
//   demoArtUrl(name)  the entity_picture of a demo player: a path on HA, as HA gives it (its proxy, an access token)
//   demoArt(url)      {w, h, rgba} for a demo picture URL (any host in front of the path), else null
//   serveDemoArt(m)   a WASM module's wanted pictures (core/images.h fw_img_wants) answered: demo art, else 404

const SIZE = 300;  // a typical album-art size from a speaker integration
/** the picture proxy path HA gives as entity_picture, with the demo's access token */
export const demoArtUrl = (name) => `/api/media_player_proxy/media_player.${name}?token=demo&cache=${name}`;

// one palette per picture: background top / bottom, the sun, the stripes (made-up, warm and cool)
const LOOKS = [
  [[38, 52, 110], [214, 96, 74], [250, 204, 120], [24, 30, 64]],
  [[18, 92, 96], [232, 180, 90], [246, 236, 210], [12, 54, 60]],
  [[86, 40, 98], [236, 120, 140], [255, 214, 170], [52, 22, 64]],
];
const hash = (s) => [...s].reduce((h, c) => (h * 31 + c.charCodeAt(0)) >>> 0, 7);

export function demoArt(url) {
  const m = /\/api\/media_player_proxy\/media_player\.([a-z0-9_]+)\?(?:.*&)?token=demo\b/.exec(String(url));
  if (!m) return null;
  const [top, bot, sun, band] = LOOKS[hash(m[1]) % LOOKS.length];
  const w = SIZE, h = SIZE, rgba = new Uint8Array(w * h * 4);
  const mix = (a, b, t) => a.map((v, i) => Math.round(v + (b[i] - v) * t));
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let c = mix(top, bot, (y + x * 0.25) / (h * 1.25));
      const dx = x - w * 0.5, dy = y - h * 0.56, r = Math.hypot(dx, dy);
      const edge = Math.min(1, Math.max(0, w * 0.26 - r + 0.5));  // the sun, its edge anti-aliased
      if (edge > 0) c = mix(c, mix(sun, bot, Math.max(0, dy) / (w * 0.6)), edge);
      if (y > h * 0.62 && Math.floor((y - h * 0.62) / 9) % 2 === 0) c = mix(c, band, Math.min(1, (y - h * 0.62) / (h * 0.3)));  // the sea's stripes
      if (x < 6 || y < 6 || x >= w - 6 || y >= h - 6) c = mix(c, band, 0.35);  // a print edge
      rgba.set([...c, 255], (y * w + x) * 4);
    }
  }
  return { w, h, rgba };
}

export function serveDemoArt(m) {
  if (!m._fw_img_wants) return;
  const str = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
  for (const url of JSON.parse(m.UTF8ToString(m._fw_img_wants()) || '[]')) {
    const a = demoArt(url);
    if (!a) { str(url, (pu) => str('not found (404)', (pw) => m._fw_img_fail(pu, pw))); continue; }
    const p = m._malloc(a.rgba.length);
    m.HEAPU8.set(a.rgba, p);
    str(url, (pu) => m._fw_img_put(pu, a.w, a.h, p));
    m._free(p);
  }
}
