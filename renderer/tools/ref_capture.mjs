#!/usr/bin/env node
// Capture the Astro originals of ported components (tests/references.json) into tests/ref/, both themes at 720, from
// the docs explorer (FW_DOCS_URL, default http://127.0.0.1:4399/facet), headless Chromium. A manual step, not a check:
// the visual rule (tests/rules/visual.mjs) compares against what is kept in the repo.
//   node tools/ref_capture.mjs
import { readFileSync, existsSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const refsFile = join(HERE, 'tests/references.json');
const REFS = existsSync(refsFile) ? JSON.parse(readFileSync(refsFile, 'utf8')).refs : [];
const refDir = join(HERE, 'tests/ref');
async function capture() {
  const { chromium } = await import('/home/claude/claude-assistant/apps/hub/node_modules/playwright/index.mjs');
  const DOCS = process.env.FW_DOCS_URL || 'http://127.0.0.1:4399/facet';
  mkdirSync(refDir, { recursive: true });
  const b = await chromium.launch({ executablePath: '/usr/bin/chromium' });
  const p = await b.newPage({ viewport: { width: 1600, height: 1200 } });
  const items = [...new Set(REFS.filter((r) => r.astro.item).map((r) => r.astro.item))];
  for (const item of items) {
    await p.goto(`${DOCS}/components/#${item}`, { waitUntil: 'load' });
    await p.waitForTimeout(2500);
    for (const th of ['dark', 'light']) {
      await p.evaluate((th) => document.documentElement.setAttribute('data-facet-theme', th), th);
      await p.waitForTimeout(600);
      for (const r of REFS.filter((x) => x.astro.item === item)) {
        const els = await p.$$(r.astro.selector || '.explorer__preview .ex-prev');
        if (r.astro.selector) await p.waitForTimeout(400);
        const el = els[r.astro.index ?? 0];
        if (el) await el.screenshot({ path: join(refDir, `${r.comp}--${r.variant}--${th}.png`), animations: 'disabled' });
        else console.log(`capture: ${item} has no element ${r.astro.index ?? 0} for ${r.comp} ${r.variant}`);
      }
    }
  }
  await b.close();
}

await capture();
