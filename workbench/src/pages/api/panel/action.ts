import type { APIRoute } from 'astro';
import { configured, ha } from '../../../lib/ha';

export const prerender = false;

/**
 * POST /api/panel/action {action: 'spec_call', value: {service, entity, data?}}: a spec node's Home Assistant call from
 * the WASM renderer, the same calls a panel makes (core/ha.h act):
 *   no data: homeassistant.toggle|turn_on|turn_off on light / switch / fan / input_boolean, media_player transport,
 *   cover open / close / stop, script / scene turn_on, button / input_button press;
 *   with data: one of FACET_CALL, on an entity of the service's own domain.
 * No Home Assistant configured: accepted and not sent ({ok: true, demo: true}).
 */
const FACET_CALL = ['light.turn_on', 'cover.set_cover_position', 'media_player.volume_set', 'media_player.select_source', 'climate.set_temperature',
  'input_select.select_option', 'select.select_option', 'input_number.set_value', 'number.set_value', 'script.turn_on',
  'climate.set_hvac_mode', 'climate.set_preset_mode', 'cover.set_cover_tilt_position'];
const bad = (msg: string) => Response.json({ error: msg }, { status: 400 });

export const POST: APIRoute = async ({ request }) => {
  let body: any;
  try { body = await request.json(); } catch { return bad('bad json'); }
  if (body?.action !== 'spec_call') return bad('action: spec_call');
  const svc = String(body?.value?.service ?? ''), id = String(body?.value?.entity ?? ''), dom = id.split('.')[0];
  if (!/^[a-z_]+\.[a-z0-9_]+$/.test(id)) return bad('not an entity');
  const raw = body?.value?.data;
  let data: Record<string, unknown> = {};
  if (raw !== undefined && raw !== null && raw !== '') {
    try { data = typeof raw === 'string' ? JSON.parse(raw) : raw; } catch { return bad('data is not JSON'); }
    if (!data || typeof data !== 'object' || Array.isArray(data)) return bad('data must be an object');
    if (!FACET_CALL.includes(svc) || svc.split('.')[0] !== dom) return bad(`not a spec call: ${svc} ${id}`);
  } else {
    const ok = (/^homeassistant\.(toggle|turn_on|turn_off)$/.test(svc) && ['light', 'switch', 'fan', 'input_boolean'].includes(dom))
      || (/^media_player\.(media_play_pause|media_next_track|media_previous_track|media_stop)$/.test(svc) && dom === 'media_player')
      || (/^cover\.(open_cover|close_cover|stop_cover)$/.test(svc) && dom === 'cover')
      || (svc === `${dom}.turn_on` && ['script', 'scene'].includes(dom))
      || (svc === `${dom}.press` && ['button', 'input_button'].includes(dom));
    if (!ok) return bad(`not a spec call: ${svc} ${id}`);
  }
  if (!configured()) return Response.json({ ok: true, demo: true });
  try {
    const [d, s] = svc.split('.');
    await ha(`services/${d}/${s}`, { method: 'POST', body: { ...data, entity_id: id }, timeoutMs: 15000 });
    return Response.json({ ok: true });
  } catch (e: any) { return Response.json({ error: String(e?.message ?? e) }, { status: 502 }); }
};
