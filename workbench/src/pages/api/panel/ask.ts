import type { APIRoute } from 'astro';
import { configured, ha } from '../../../lib/ha';

export const prerender = false;

/**
 * POST /api/panel/ask {service, data} -> {ok, response}: an HA action whose reply a Facet feed adapter reads, for the
 * WASM workbench (core/ha.h ask; the panel sends the same action over its ESPHome API with wants_response).
 * Same closed list as core/ha.h ask(): calendar.get_events {entity_id: calendar.x, start_date_time, end_date_time},
 * weather.get_forecasts {entity_id: weather.x, type: hourly | daily | twice_daily}.
 * response = HA's service_response ({"calendar.x": {"events": [...]}}), the shape the panel receives.
 */
export const POST: APIRoute = async ({ request }) => {
  let b: any;
  try { b = await request.json(); } catch { return Response.json({ ok: false, error: 'bad json' }, { status: 400 }); }
  const d = b?.data ?? {};
  if (!configured()) return Response.json({ ok: true, response: {}, demo: true });  // no Home Assistant: nothing to ask
  if (b?.service === 'weather.get_forecasts') {  // the Weather feed's forecast (core/weather_source.cpp)
    if (typeof d.entity_id !== 'string' || !/^weather\.[a-z0-9_]+$/.test(d.entity_id)) return Response.json({ ok: false, error: 'not a weather entity' }, { status: 400 });
    if (!['hourly', 'daily', 'twice_daily'].includes(d.type)) return Response.json({ ok: false, error: 'type hourly, daily or twice_daily' }, { status: 400 });
    try {
      const r = await ha('services/weather/get_forecasts?return_response', { method: 'POST', body: { entity_id: d.entity_id, type: d.type }, timeoutMs: 15000 });
      return Response.json({ ok: true, response: r?.service_response ?? {} }, { headers: { 'Cache-Control': 'no-store' } });
    } catch (e: any) { return Response.json({ ok: false, error: String(e?.message || e) }, { status: 502 }); }
  }
  if (b?.service !== 'calendar.get_events') return Response.json({ ok: false, error: 'ask not allowed' }, { status: 400 });
  if (typeof d.entity_id !== 'string' || !/^calendar\.[a-z0-9_]+$/.test(d.entity_id)) return Response.json({ ok: false, error: 'not a calendar' }, { status: 400 });
  const body: Record<string, string> = { entity_id: d.entity_id };
  for (const k of ['start_date_time', 'end_date_time']) if (typeof d[k] === 'string' && d[k].length <= 32) body[k] = d[k];
  try {
    const r = await ha('services/calendar/get_events?return_response', { method: 'POST', body, timeoutMs: 15000 });
    return Response.json({ ok: true, response: r?.service_response ?? {} }, { headers: { 'Cache-Control': 'no-store' } });
  } catch (e: any) { return Response.json({ ok: false, error: String(e?.message || e) }, { status: 502 }); }
};
