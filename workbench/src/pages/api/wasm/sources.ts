import type { APIRoute } from 'astro';
import { discover } from '../../../lib/facet-sources';

export const prerender = false;

/** GET /api/wasm/sources: the screen sources found in Home Assistant (lib/facet-sources.ts): per screen the source in use, what
 *  each source found, the params it fills; entity options per domain; the deck (factory deck + sources + params)
 *  that the workbench applies and a panel can be sent (Send specs). Cached 30 s. */
export const GET: APIRoute = async () => {
  try { return Response.json(await discover(), { headers: { 'Cache-Control': 'no-store' } }); }
  catch (e: any) { return Response.json({ error: String(e?.message ?? e) }, { status: 502 }); }
};
