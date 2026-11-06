import type { APIRoute } from 'astro';

export const prerender = false;

/** The public APIs a feed's direct source may read (core/net.h): GET only, these hosts and paths only. */
const READS = [
  /^https:\/\/api\.adsb\.lol\/v2\/point\/-?\d{1,2}(\.\d+)?\/-?\d{1,3}(\.\d+)?\/\d{1,3}$/,  // the flights feed (core/flights_source.cpp)
];

/**
 * GET /api/wasm/web?url=<url>: one web read for the WASM module's feeds (core/net.h, core/main.cpp fw_net_gets), made
 * here because those APIs send no CORS headers. Streams the body back as the API sent it (the module caps it).
 */
export const GET: APIRoute = async ({ url: u }) => {
  const url = u.searchParams.get('url') ?? '';
  if (!READS.some((r) => r.test(url))) return new Response('not a read a feed may make', { status: 403 });
  try {
    const r = await fetch(url, { headers: { Accept: 'application/json', 'User-Agent': 'Facet workbench' }, signal: AbortSignal.timeout(15000) });
    if (!r.ok) return new Response(`the site answered ${r.status}`, { status: 502 });
    return new Response(r.body, { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
  } catch (e: any) { return new Response(String(e?.message || e), { status: 502 }); }
};
