#!/usr/bin/env node
/**
 * Generates the README images in .github/assets/ from the design system itself:
 *
 * - screens-{dark,light}.png        all ten screens in a 5 x 2 grid
 * - screen-<slug>-{dark,light}.png  one small preview per screen page
 * - icon-<name>-{dark,light}.svg    screen icons with the ink and accent baked in
 * - pill-<name>-{dark,light}.png    the link pills under the hero
 * - palette-{dark,light}.png        colour and type strip from tokens.json
 *
 * The suffix is the colour scheme the image is for (GitHub picks it with <picture>).
 * `currentColor` and CSS variables don't work in an SVG shown with <img>, hence the baked copies.
 *
 * Usage: pnpm readme:assets [--no-build]
 *
 * Needs Playwright and a Chromium. Neither is a dependency of this repo:
 * FACET_PLAYWRIGHT  path to a playwright (or playwright-core) package, else whatever Node can resolve
 * FACET_CHROMIUM    Chromium binary, else /usr/bin/chromium if it exists, else Playwright's own
 */
import { execSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync, mkdirSync, statSync } from 'node:fs';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { extname, join, normalize, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(fileURLToPath(import.meta.url), '../..');
const out = join(root, '.github/assets');
const dist = join(root, 'site/dist');
const BASE = '/facet/';

const tokens = JSON.parse(readFileSync(join(root, 'packages/tokens/src/tokens.json'), 'utf8'));
const color = (name, scheme) => tokens.color[name][scheme];
const ACCENT = tokens.color.accent.dark;
const SCHEMES = ['dark', 'light'];

/** The rounded glass of the panel, as a share of the screen width. */
const GLASS = 40 / 720;

/** Ink for the baked icons and pills: Facet's text colours, readable on GitHub's own backgrounds. */
const INK = { dark: color('text', 'dark'), light: color('text', 'light') };

// ---- screens, from the docs pages' source

function screenDocs() {
  const src = readFileSync(join(root, 'site/src/lib/screens.ts'), 'utf8');
  const docs = [];
  const re = /slug: '([^']+)',\s*preview: '([^']+)',\s*icon: '([^']+)',\s*title: '([^']+)'/g;
  for (const m of src.matchAll(re)) docs.push({ slug: m[1], icon: m[3], title: m[4] });
  if (!docs.length) throw new Error('No screens found in site/src/lib/screens.ts');
  return docs;
}

// ---- playwright

async function loadPlaywright() {
  const tries = [];
  if (process.env.FACET_PLAYWRIGHT) tries.push(process.env.FACET_PLAYWRIGHT);
  tries.push('playwright', 'playwright-core');
  const require = createRequire(join(root, 'package.json'));
  for (const id of tries) {
    try {
      return require(id);
    } catch {
      // next
    }
  }
  throw new Error('Playwright not found: set FACET_PLAYWRIGHT to a playwright package directory.');
}

function chromiumPath() {
  if (process.env.FACET_CHROMIUM) return process.env.FACET_CHROMIUM;
  return existsSync('/usr/bin/chromium') ? '/usr/bin/chromium' : undefined;
}

// ---- a tiny static server for site/dist under /facet/

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.json': 'application/json',
  '.woff2': 'font/woff2',
  '.woff': 'font/woff',
  '.webmanifest': 'application/manifest+json',
};

function serve() {
  const server = createServer((req, res) => {
    const url = decodeURIComponent((req.url ?? '/').split('?')[0]);
    if (!url.startsWith(BASE)) {
      res.writeHead(404).end();
      return;
    }
    let file = normalize(join(dist, url.slice(BASE.length)));
    if (!file.startsWith(dist)) {
      res.writeHead(403).end();
      return;
    }
    if (existsSync(file) && statSync(file).isDirectory()) file = join(file, 'index.html');
    if (!existsSync(file)) {
      res.writeHead(404).end();
      return;
    }
    res.writeHead(200, { 'content-type': TYPES[extname(file)] ?? 'application/octet-stream' });
    res.end(readFileSync(file));
  });
  return new Promise((ok) => server.listen(0, '127.0.0.1', () => ok(server)));
}

// ---- helpers

const write = (name, data) => {
  writeFileSync(join(out, name), data);
  const kb = (statSync(join(out, name)).size / 1024).toFixed(0);
  console.log(`  ${name.padEnd(30)} ${kb.padStart(4)} KB`);
};

/** Render an HTML fragment on a page that has the site's fonts loaded, and screenshot `#shot`. */
async function render(page, origin, html, css, scale) {
  await page.goto(`${origin}${BASE}design-system/`);
  await page.setViewportSize({ width: 2000, height: 1400 });
  await page.evaluate(
    ([h, c]) => {
      document.head.querySelectorAll('script').forEach((s) => s.remove());
      const style = document.createElement('style');
      style.textContent = `html, body { margin: 0; padding: 0; background: transparent !important; }
        #shot { display: inline-block; } ${c}`;
      document.head.append(style);
      document.body.className = '';
      document.body.innerHTML = `<div id="shot">${h}</div>`;
    },
    [html, css],
  );
  await page.evaluate(async () => {
    await document.fonts.ready;
    await Promise.all([...document.images].map((i) => i.decode().catch(() => {})));
  });
  return page.locator('#shot').screenshot({ omitBackground: true, scale: 'device' });
}

// ---- icons

function bakeIcon(svg, scheme) {
  return svg
    .replace(/var\(--facet-color-accent,\s*#[0-9a-fA-F]+\)/g, ACCENT)
    .replace(/currentColor/g, INK[scheme])
    .trim();
}

// ---- main

async function main() {
  if (!process.argv.includes('--no-build')) {
    console.log('Building the site...');
    execSync('pnpm build', { cwd: root, stdio: 'inherit' });
  }
  mkdirSync(out, { recursive: true });
  const docs = screenDocs();

  console.log('Icons');
  const iconDir = join(root, 'packages/icons/svg/screens');
  const icons = [...new Set([...docs.map((d) => d.icon), 'settings'])];
  for (const name of icons) {
    const svg = readFileSync(join(iconDir, `${name}.svg`), 'utf8');
    for (const s of SCHEMES) write(`icon-${name}-${s}.svg`, bakeIcon(svg, s) + '\n');
  }

  const { chromium } = await loadPlaywright();
  const server = await serve();
  const origin = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  try {
    // Raw 720 px screenshots of every screen from its docs page, per scheme.
    console.log('Screens');
    // Toasts are a layer over Home, so they get an icon line in the README but no preview.
    const screens = docs.filter((d) => d.slug !== 'toasts');
    const raw = { dark: {}, light: {} };
    const ctx = await browser.newContext({
      viewport: { width: 1600, height: 1200 },
      deviceScaleFactor: 1,
      reducedMotion: 'no-preference',
    });
    const page = await ctx.newPage();
    for (const doc of screens) {
      await page.goto(`${origin}${BASE}docs/screens/${doc.slug}/`);
      await page.waitForSelector('[data-frame] .f-screen');
      for (const s of SCHEMES) {
        await page.click(`[data-theme-set="${s}"]`);
        await page.evaluate(() => {
          const f = document.querySelector('[data-frame]');
          f.style.transform = 'none';
          f.style.marginLeft = '0';
        });
        // Let entry animations, charts and toasts settle.
        await page.waitForTimeout(1200);
        const shot = await page.locator('[data-frame] .f-screen').first().screenshot();
        raw[s][doc.slug] = `data:image/png;base64,${shot.toString('base64')}`;
      }
    }
    await ctx.close();

    const comp = await browser.newContext({ deviceScaleFactor: 2 });
    const cpage = await comp.newPage();

    // All screens: 5 x 2 grid at 2x, shown at about 950 px on GitHub.
    const TILE = 232;
    const GAP = 16;
    for (const s of SCHEMES) {
      const html = `<div class="grid">${screens
        .map((d) => `<img src="${raw[s][d.slug]}" alt="">`)
        .join('')}</div>`;
      const css = `.grid { display: grid; grid-template-columns: repeat(5, ${TILE}px); gap: ${GAP}px; }
        .grid img { width: ${TILE}px; height: ${TILE}px; display: block; border-radius: ${Math.round(TILE * GLASS)}px;
          outline: 1px solid ${color('line', s)}; outline-offset: -1px; }`;
      await cpage.setViewportSize({ width: 1400, height: 800 });
      const buf = await render(cpage, origin, html, css);
      write(`screens-${s}.png`, buf);
    }

    // One preview per screen for the Screens table (shown at 200 px).
    const PREVIEW = 200;
    for (const d of screens) {
      for (const s of SCHEMES) {
        const html = `<img class="one" src="${raw[s][d.slug]}" alt="">`;
        const css = `.one { width: ${PREVIEW}px; height: ${PREVIEW}px; display: block;
          border-radius: ${Math.round(PREVIEW * GLASS)}px; outline: 1px solid ${color('line', s)}; outline-offset: -1px; }`;
        write(`screen-${d.slug}-${s}.png`, await render(cpage, origin, html, css));
      }
    }

    // Link pills: mono caps labels like the panel's, the first one in orange.
    console.log('Pills');
    const pills = [
      { id: 'demo', label: 'Demo', primary: true },
      { id: 'components', label: 'Components' },
      { id: 'docs', label: 'Docs' },
      { id: 'roadmap', label: 'Roadmap' },
    ];
    for (const p of pills) {
      for (const s of SCHEMES) {
        const ink = p.primary ? color('on-accent', s) : INK[s];
        const html = `<span class="pill${p.primary ? ' primary' : ''}"><i></i>${p.label}</span>`;
        const css = `.pill { display: inline-flex; align-items: center; gap: 10px; height: 36px; padding: 0 18px;
            box-sizing: border-box; border-radius: 999px; border: 1px solid ${color('line-hi', s)};
            font: 500 13px/1 'JetBrains Mono', monospace; letter-spacing: 0.12em; text-transform: uppercase; color: ${ink}; }
          .pill i { width: 7px; height: 7px; border-radius: 50%; background: ${ACCENT}; }
          .pill.primary { background: ${ACCENT}; border-color: ${ACCENT}; }
          .pill.primary i { background: ${ink}; }`;
        write(`pill-${p.id}-${s}.png`, await render(cpage, origin, html, css));
      }
    }

    // Palette and type strip, on the panel's own background.
    console.log('Palette');
    const swatches = [
      'bg',
      'surface',
      'line',
      'text',
      'text-dim',
      'accent',
      'blue',
      'steel',
      'ok',
      'warn',
    ];
    const type = tokens.screenType;
    for (const s of SCHEMES) {
      const sw = swatches
        .map(
          (n) =>
            `<div class="sw"><span style="background:${color(n, s)}"></span><b>${n}</b><em>${color(n, s)}</em></div>`,
        )
        .join('');
      const html = `<div class="strip">
        <div class="type">
          <div class="label">Type</div>
          <div class="value">21:13</div>
          <div class="headline">Partly cloudy</div>
          <div class="body">Dry for the next hour</div>
          <div class="meta">Need you · 2 working</div>
        </div>
        <div class="colours"><div class="label">Colour</div><div class="sws">${sw}</div></div>
      </div>`;
      const t = (k) =>
        `font-weight: ${type[k].weight}; font-size: ${type[k].size}px; letter-spacing: ${type[k].letterSpacing}px;`;
      const css = `.strip { width: 920px; box-sizing: border-box; display: flex; gap: 56px; padding: 40px 48px;
          background: ${color('bg', s)}; color: ${color('text', s)}; border-radius: 36px;
          outline: 1px solid ${color('line', s)}; outline-offset: -1px; font-family: Inter, sans-serif; }
        .label, .meta, .sw em { font: 500 12px/1 'JetBrains Mono', monospace; letter-spacing: 0.12em;
          text-transform: uppercase; color: ${color('text-dim', s)}; }
        .label { margin-bottom: 24px; }
        .type { width: 280px; flex: none; }
        .value { ${t('value')} line-height: 1; margin-bottom: 16px; }
        .headline { font-weight: ${type.headline.weight}; font-size: 32px; line-height: 1.2; margin-bottom: 8px; }
        .body { font-size: 18px; color: ${color('text-dim', s)}; margin-bottom: 20px; }
        .meta { color: ${color('accent-text', s)}; }
        .colours { flex: 1; }
        .sws { display: grid; grid-template-columns: repeat(5, 1fr); gap: 20px 16px; }
        .sw span { display: block; height: 56px; border-radius: 12px; outline: 1px solid ${color('line', s)};
          outline-offset: -1px; margin-bottom: 10px; }
        .sw b { display: block; font: 500 14px/1.3 Inter, sans-serif; margin-bottom: 4px; }
        .sw em { font-style: normal; font-size: 11px; color: ${color('text-mute', s)}; letter-spacing: 0.06em; }`;
      write(`palette-${s}.png`, await render(cpage, origin, html, css));
    }
    await comp.close();
  } finally {
    await browser.close();
    server.close();
  }
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
