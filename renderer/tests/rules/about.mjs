// about (Darren 2026-10-25): every setting has a plain description, the docs tables, the agent guide and the editor
// show it: each registered prop (meta "about") and each spec parameter (renderer/specs/*.json params.<name>.about).
// Moved from tools/audit.mjs: a startup check of the runner (tools/lib/rules.mjs), once at 1.5x (density-free).
import { readFileSync, readdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = resolve(dirname(fileURLToPath(import.meta.url)), '../..');

export default {
  id: 'about', tiers: ['fast', 'gate'], dens: ['1.5x'],
  about: 'Every component prop and every spec parameter has a description',
  once({ m }, rep) {
    const meta = JSON.parse(m.UTF8ToString(m._fw_meta())).map((c) => ({ ...c, meta: typeof c.meta === 'string' ? JSON.parse(c.meta) : c.meta }));
    const missing = [];
    for (const c of meta) for (const p of c.meta.props || []) if (!p.about) missing.push(`${c.id}.${p.name}`);
    for (const f of readdirSync(join(HERE, 'specs')).filter((f) => f.endsWith('.json')).sort()) {
      const sp = JSON.parse(readFileSync(join(HERE, 'specs', f), 'utf8'));
      for (const [k, v] of Object.entries(sp.params || {})) if (!(v && typeof v === 'object' && v.about)) missing.push(`specs/${f} ${k}`);
    }
    rep.check(!missing.length, missing.length ? `about: ${missing.length} settings have no description: ${missing.slice(0, 10).join(', ')}${missing.length > 10 ? ` +${missing.length - 10}` : ''}` : 'about: every setting has a description');
  },
};
