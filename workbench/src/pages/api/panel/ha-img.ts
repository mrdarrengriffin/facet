import type { APIRoute } from 'astro';
import { configured, env } from '../../../lib/ha';
import { demoArt } from '../../../../../renderer/tools/demo_art.mjs';
import { png } from '../../../../../renderer/tools/lib/fwbench.mjs';

export const prerender = false;

/**
 * GET /api/panel/ha-img?path=<path>: one picture from Home Assistant for the WASM workbench (core/images.h: a picture
 * path is fetched from the panel's own HA link; here with the workbench's). Only HA's picture proxies (a media player's
 * entity_picture: /api/media_player_proxy/..., /api/image_proxy/...); HA's answer is passed on as it came. No Home
 * Assistant configured (lib/ha.ts): the fake one's demo album art (renderer/tools/demo_art.mjs), else 404.
 */
export const GET: APIRoute = async ({ url: u }) => {
  const path = u.searchParams.get('path') ?? '';
  if (!/^\/api\/(media_player_proxy|image_proxy)\/[a-z_]+\.[a-z0-9_]+(\?[^\s#]*)?$/.test(path)) return new Response('not a Home Assistant picture', { status: 403 });
  if (!configured()) {
    const a = demoArt(path);
    if (!a) return new Response('not found (404)', { status: 404, headers: { 'x-fw-reason': 'not found (404)' } });
    return new Response(png(a.rgba, a.w, a.h), { headers: { 'Content-Type': 'image/png', 'Cache-Control': 'no-store' } });
  }
  const { url, token } = env();
  try {
    const r = await fetch(`${url}${path}`, { headers: { Authorization: `Bearer ${token}` }, signal: AbortSignal.timeout(8000) });
    if (!r.ok) return new Response(`Home Assistant answered ${r.status}`, { status: r.status === 404 ? 404 : 502, headers: { 'x-fw-reason': `Home Assistant answered ${r.status}` } });
    return new Response(r.body, { headers: { 'Content-Type': r.headers.get('content-type') ?? 'application/octet-stream', 'Cache-Control': 'no-store' } });
  } catch (e: any) { return new Response(String(e?.message || e), { status: 504, headers: { 'x-fw-reason': 'timeout' } }); }
};
