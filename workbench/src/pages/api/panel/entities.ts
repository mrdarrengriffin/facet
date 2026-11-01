import type { APIRoute } from 'astro';
import { configured, ha } from '../../../lib/ha';
import { DEMO, demoValue } from '../../../../../renderer/tools/demo_ha.mjs';

export const prerender = false;
// GET /api/panel/entities?ids=a.b,c.d[&attrs=x,y] -> [{id, name, state, attrs?}]: those entities' states for the WASM
// renderer's binds (engine.ts polls it). ?domains=light,switch -> every entity of those domains (the pickers).
// No Home Assistant configured (lib/ha.ts): the fake one (renderer/tools/demo_ha.mjs).
type Row = { id: string; name: string; state: string; attrs?: Record<string, unknown> };
export const GET: APIRoute = async ({ url }) => {
  const want = (url.searchParams.get('domains') ?? '').split(',').filter((d) => /^[a-z_]+$/.test(d));
  const ids = (url.searchParams.get('ids') ?? '').split(',').filter((d) => /^[a-z_]+\.[a-z0-9_]+$/.test(d)).slice(0, 32);
  const attrs = (url.searchParams.get('attrs') ?? '').split(',').filter((a) => /^[a-z_]+$/.test(a)).slice(0, 24);
  if (!want.length && !ids.length) return Response.json({ error: 'domains or ids' }, { status: 400 });
  const out: Row[] = [];
  try {
    if (configured()) {
      const all = (await ha('states')) as { entity_id: string; state: string; attributes: Record<string, any> }[];
      for (const s of all.filter((s) => ids.includes(s.entity_id) || want.includes(s.entity_id.split('.')[0]))) {
        const o: Row = { id: s.entity_id, name: String(s.attributes.friendly_name ?? s.entity_id), state: s.state };
        const a = Object.fromEntries(attrs.filter((k) => k in s.attributes).map((k) => [k, s.attributes[k]]));
        if (Object.keys(a).length) o.attrs = a;
        out.push(o);
      }
    } else {
      const all = [...new Set([...ids, ...Object.keys(DEMO).filter((e) => want.includes(e.split('.')[0]))])];
      for (const e of all) {
        const st = demoValue(e, '');
        if (st === undefined) continue;
        const o: Row = { id: e, name: e, state: String(st) };
        const a = Object.fromEntries(attrs.map((k) => [k, demoValue(e, k)]).filter(([, v]) => v !== undefined));
        if (Object.keys(a).length) o.attrs = a;
        out.push(o);
      }
    }
    return Response.json(out.sort((a, b) => a.name.localeCompare(b.name)), { headers: { 'Cache-Control': 'no-store' } });
  } catch (e: any) {
    return Response.json({ error: String(e?.message ?? e) }, { status: 502 });
  }
};
