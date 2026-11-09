#!/usr/bin/env node
// The Facet editor served BY a panel: builds editor v1 (src/wasm/editor-v1.ts with editor-v1-host-device.ts in
// place of editor-v1-host.ts; the page: src/wasm/editor-v1-page.mjs) + the panel's renderer files at its density, gzips them and writes them as a web UI the
// device glue serves from flash (firmware/fw_device.h "Web UI").
//   node workbench/scripts/editor-device.mjs --wasm <dir: manifest.json fw.mjs fw.wasm> [--renderer <renderer dir>]
//                                  [--device <board id> (default guition-4848s040)] [--den 1x|1.5x (default: the board's)]
//                                  [--out <the ESPHome component dir, e.g. esphome/gen/<board>/facet_wb>]
//                                  [--dist <dir: the plain files, to look at locally>]
// --out writes fw_web.bin (the gzipped files back to back), fw_web.cpp (links it into rodata with .incbin, absolute
// path) and fw_web_gen.h (namespace fw::web: File {path, type, cache, etag, off, len}, FILES, blob()). Run it AFTER
// firmware/bundle.mjs (that one deletes files it did not write).
// Paths on the panel: / (index, no-cache + ETag), /ed/editor.js|css (?v=hash, immutable), /ed/<den>/manifest.json
// (no-cache), /ed/<den>/fw.mjs|fw.wasm (the engine adds ?v=<built>, immutable), /ed/fonts/*.woff2 (?v=hash, immutable;
// the editor's fonts are embedded, nothing loads from the internet).
import { createHash } from 'node:crypto';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync, constants } from 'node:zlib';
import { build } from 'vite';
import { editorPage } from '../src/wasm/editor-page.mjs';
import { v1Boot, v1Body } from '../src/wasm/editor-v1-page.mjs';

const WB = dirname(dirname(fileURLToPath(import.meta.url)));
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const wasmDir = arg('--wasm');
const R = resolve(arg('--renderer', join(WB, '..', 'renderer')));
const out = arg('--out'), dist = arg('--dist');
const device = arg('--device', 'guition-4848s040');
let den = arg('--den');
try { den ??= JSON.parse(readFileSync(join(WB, '..', 'boards', `${device}.json`), 'utf8')).scale; } catch { /* below */ }
if (!den) { console.error(`unknown --device ${device} (a boards/<id>.json) and no --den`); process.exit(2); }
if (!wasmDir || (!out && !dist)) { console.error('usage: editor-device.mjs --wasm <dir> [--renderer <dir>] --out <dir> | --dist <dir>'); process.exit(2); }
const t0 = Date.now();

// 1. the editor bundle (vite / rolldown): one js + one css, the device host swapped in
const tmp = mkdtempSync(join(tmpdir(), 'fe-dev-'));
const entry = join(tmp, 'entry.ts');
writeFileSync(entry, `import '${WB}/src/wasm/editor-v1.css';\nimport { start } from '${WB}/src/wasm/editor-v1';\nstart();\n`);
await build({
  configFile: false, logLevel: 'warn', root: tmp,
  plugins: [{
    name: 'device-host', enforce: 'pre',
    resolveId(src, importer) {
      if (src === './editor-v1-host' && importer?.endsWith('/editor-v1.ts')) return join(WB, 'src/wasm/editor-v1-host-device.ts');
      return null;
    },
  }],
  build: {
    outDir: join(tmp, 'out'), emptyOutDir: true, minify: true, target: 'es2022', cssCodeSplit: false, modulePreload: false,
    rollupOptions: { input: entry, output: { entryFileNames: 'editor.js', assetFileNames: 'editor[extname]', codeSplitting: false } },
  },
});
const js = readFileSync(join(tmp, 'out/editor.js')), css = readFileSync(join(tmp, 'out/editor.css'));
rmSync(tmp, { recursive: true, force: true });

// 2. the page
const h8 = (b) => createHash('sha1').update(b).digest('hex').slice(0, 12);
const { css: vars } = editorPage(R);
// the editor's fonts, embedded (no internet needed): only the weights the editor uses (Inter 400 /
// 500 / 600, JetBrains Mono 400; OFL, licences beside them), cut once from @fontsource's latin woff2 with fonttools:
//   pyftsubset <in> --unicodes=U+0020-007E,U+00A0-00FF,U+0131,U+0152-0153,U+02C6,U+02DA,U+02DC,U+2013-2014,U+2018-201A,
//     U+201C-201E,U+2022,U+2026,U+2032-2033,U+2039-203A,U+2044,U+20AC,U+2122,U+2190-2193,U+2212,U+00D7,U+2713,U+2715
//     --layout-features=kern,liga,calt,tnum,case (mono: kern) --flavor=woff2   (58 KB for the four)
const FONTS = [['Inter', 400, 'inter-400'], ['Inter', 500, 'inter-500'], ['Inter', 600, 'inter-600'], ['JetBrains Mono', 400, 'jetbrains-mono-400']]
  .map(([family, weight, file]) => ({ family, weight, path: `/ed/fonts/${file}.woff2`, b: readFileSync(join(WB, `src/wasm/fonts/${file}.woff2`)) }));
const faces = FONTS.map((f) => `@font-face{font-family:'${f.family}';font-style:normal;font-weight:${f.weight};font-display:swap;src:url(${f.path}?v=${h8(f.b)}) format('woff2')}`).join('');
// the page = editor v1's markup and boot data (src/wasm/editor-v1-page.mjs, the same as the workbench's), fonts embedded,
// plus the agent hint (the panel serves /ai)
const body = v1Body(v1Boot(dirname(R), { panel: true }), '<span>Served by this panel.</span><span class="agent-hint dim">Agents: paste <a href="/ai">/ai</a> to yours</span>');
// the renderer's download starts with the page (a preload; the editor reads the same URL, counting its progress
// against the manifest's wasm_bytes)
const wasmB = readFileSync(join(wasmDir, 'fw.wasm')), built = JSON.parse(readFileSync(join(wasmDir, 'manifest.json'), 'utf8')).built;
const html = `<!doctype html>
<html lang="en" data-device="${device}" data-den="${den}">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="icon" href="data:,">
<title>Facet</title>
<style>${faces}${vars}</style>
<link rel="preload" href="/ed/${den}/fw.wasm?v=${encodeURIComponent(built)}" as="fetch" type="application/wasm" crossorigin="anonymous">
<link rel="stylesheet" href="/ed/editor.css?v=${h8(css)}">
</head>
<body>
${body}
<script type="module" src="/ed/editor.js?v=${h8(js)}"></script>
</body>
</html>
`;

// 3. the files, gzipped
const LONG = 'public, max-age=31536000, immutable', REVAL = 'no-cache';
const files = [
  ['/', 'text/html; charset=utf-8', REVAL, Buffer.from(html)],
  ['/ed/editor.js', 'text/javascript', LONG, js],
  ['/ed/editor.css', 'text/css', LONG, css],
  [`/ed/${den}/manifest.json`, 'application/json', REVAL, readFileSync(join(wasmDir, 'manifest.json'))],
  [`/ed/${den}/fw.mjs`, 'text/javascript', LONG, readFileSync(join(wasmDir, 'fw.mjs'))],
  [`/ed/${den}/fw.wasm`, 'application/wasm', LONG, wasmB],
  ...FONTS.map((f) => [f.path, 'font/woff2', LONG, f.b]),
];
if (dist) for (const [p, , , b] of files) { const f = join(dist, p === '/' ? 'index.html' : p); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, b); }
const parts = [];
let off = 0, raw = 0;
const rows = files.map(([path, type, cache, b]) => {
  const gz = gzipSync(b, { level: constants.Z_BEST_COMPRESSION });
  const pad = (4 - (gz.length % 4)) % 4;
  parts.push(gz, Buffer.alloc(pad));
  const r = { path, type, cache, etag: `"${h8(gz)}"`, off, len: gz.length };
  off += gz.length + pad; raw += b.length;
  return r;
});
const blob = Buffer.concat(parts);
if (out) {
  mkdirSync(out, { recursive: true });
  const bin = resolve(out, 'fw_web.bin');
  writeFileSync(bin, blob);
  writeFileSync(join(out, 'fw_web_gen.h'), `// GENERATED by workbench/scripts/editor-device.mjs (the Facet editor served by the panel). Do not edit.
#pragma once
#include <cstdint>
namespace fw {
namespace web {
struct File { const char *path, *type, *cache, *etag; uint32_t off, len; };
inline constexpr File FILES[] = {
${rows.map((r) => `  {${JSON.stringify(r.path)}, ${JSON.stringify(r.type)}, ${JSON.stringify(r.cache)}, ${JSON.stringify(r.etag)}, ${r.off}, ${r.len}},`).join('\n')}
};
extern "C" const uint8_t fw_web_blob[];
inline const uint8_t *blob() { return fw_web_blob; }
}  // namespace web
}  // namespace fw
`);
  // the blob hash in the source: a new blob = a changed .cpp = recompiled (cmake does not track .incbin inputs)
  writeFileSync(join(out, 'fw_web.cpp'), `// GENERATED by editor-device.mjs: links fw_web.bin (${blob.length} bytes, sha1 ${h8(blob)}) into rodata.
__asm__(".section .rodata.fw_web,\\"a\\"\\n.balign 4\\n.global fw_web_blob\\nfw_web_blob:\\n.incbin \\"${bin}\\"\\n.previous\\n");
`);
}
console.log(`editor web UI: ${rows.length} files, ${(raw / 1024).toFixed(0)} KB -> ${(blob.length / 1024).toFixed(0)} KB gzipped (${rows.map((r) => `${r.path.split('/').pop() || 'index'} ${(r.len / 1024).toFixed(0)}K`).join(', ')}) in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
