import type { APIRoute } from 'astro';
import { configured, env } from '../../../lib/ha';

export const prerender = false;

/** The REST reads the panel may make (GET only): one entity's state, the calendar list, one calendar's events. */
const READS = [
  /^states\/[a-z_]+\.[a-z0-9_]+$/,
  /^calendars$/,
  /^calendars\/calendar\.[a-z0-9_]+\?start=[0-9TZ:.+-]+&end=[0-9TZ:.+-]+$/,
];

/**
 * GET /api/panel/ha-get?path=<path>: one Home Assistant REST read for the WASM workbench (core/device.h
 * ha_link::get: the panel's own link, here with the workbench's token so none reaches the browser). Answers HA's body as
 * HA sent it, streamed (the module reads it piece by piece, as the panel would). Only the paths in READS (the Energy
 * feed's extras: the Octopus Energy integration's rate events and Octoplus calendars).
 */
export const GET: APIRoute = async ({ url: u }) => {
  const path = u.searchParams.get('path') ?? '';
  if (!READS.some((r) => r.test(path))) return new Response('not a read the panel may make', { status: 403 });
  if (!configured()) return new Response('no Home Assistant (HA_URL, HA_TOKEN)', { status: 404 });  // as HA answers a read it cannot make
  const { url, token } = env();
  try {
    const r = await fetch(`${url}/api/${path}`, { headers: { Authorization: `Bearer ${token}` }, signal: AbortSignal.timeout(20000) });
    if (!r.ok) return new Response(`Home Assistant answered ${r.status}`, { status: 502 });
    return new Response(r.body, { headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
  } catch (e: any) { return new Response(String(e?.message || e), { status: 502 }); }
};
