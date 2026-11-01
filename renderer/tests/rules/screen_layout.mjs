// Screen layout check (after the F1 screen drew its circuit over the header and the standings): the
// component audit checks parts alone; this checks how a SCREEN places them. For every screen x variant x theme x
// density, at the build and again after 1 s and 3 s of updates (live screens move), and after an update from the
// default props to each variant (fw_update: the update path must place things as the build does):
//   overlap   two regions (the screen root's direct children: header, region boxes, ...) whose boxes intersect
//   escape    an object inside one region whose box (+ its ext draw size), cut by the ancestors that clip (scroll
//             lists, anything not overflow-visible), reaches into ANOTHER region's box, or off the screen
// A region that is meant to sit over others marks itself fw::qa::free(obj, "overlay: <why>") (also inside a region:
// a view that hosts its sheets) (a bottom sheet,
// a toast layer): it is skipped for both checks. Zero-size boxes (holders of state) are not regions.
// Moved from tools/audit_screens.mjs onto the runner's loop (tools/lib/rules.mjs): per screen x case x look, a screen-size
// stage of its own; tests/rules/audit.mjs imports checkScreen from here for the screens it audits.

const inter = (a, b) => Math.max(0, Math.min(a.x2, b.x2) - Math.max(a.x1, b.x1) + 1) * Math.max(0, Math.min(a.y2, b.y2) - Math.max(a.y1, b.y1) + 1);
const box = (o, ext = 0) => ({ x1: o.x - ext, y1: o.y - ext, x2: o.x + o.w - 1 + ext, y2: o.y + o.h - 1 + ext });
const name = (os, i) => {
  const o = os[i];
  const kids = os.filter((k) => k.p === i);
  const txt = o.text || (kids.find((k) => k.text) || {}).text;
  return `${o.t || 'obj'} ${o.w}x${o.h} at ${o.x},${o.y}${txt ? ` "${String(txt).slice(0, 24)}"` : ''}`;
};

// objs = fw_audit(stage) of a stage showing one screen; W x H = the screen
export function checkScreen(objs, W, H) {
  const errs = [];
  const root = objs.findIndex((o) => o.p < 0);
  if (root < 0) return errs;
  const overlay = (o) => typeof o.free === 'string' && /^overlay/i.test(o.free);
  const regions = objs.map((o, i) => ({ o, i })).filter(({ o }) => o.p === root && o.w > 0 && o.h > 0 && !overlay(o));
  // region of every object (walk up to the root's child)
  const regionOf = objs.map((o, i) => {
    let k = i;
    while (k >= 0 && objs[k].p !== root) k = objs[k].p;
    return k;
  });
  for (let a = 0; a < regions.length; a++)
    for (let b = a + 1; b < regions.length; b++) {
      const n = inter(box(regions[a].o), box(regions[b].o));
      if (n > 0) errs.push(`overlap: ${name(objs, regions[a].i)} and ${name(objs, regions[b].i)} share ${n} px`);
    }
  const screen = { x1: objs[root].x, y1: objs[root].y, x2: objs[root].x + W - 1, y2: objs[root].y + H - 1 };
  const seen = new Set();
  objs.forEach((o, i) => {
    const r = regionOf[i];
    if (r < 0 || r === i || objs[r].p !== root || overlay(objs[r]) || o.w <= 0 || o.h <= 0 || (o.opa !== undefined && o.opa === 0)) return;
    // an overlay inside a region (a view hosting its sheets: fw::qa::free "overlay: ...") and what it holds sit over
    // the screen on purpose, as a top-level overlay does
    for (let k = i; k >= 0 && k !== r; k = objs[k].p) if (overlay(objs[k])) return;
    // what can actually show: the box cut by every ancestor that clips (not overflow-visible; scroll lists clip)
    let b = box(o, o.ext || 0);
    for (let k = o.p; k >= 0 && k !== root && b; k = objs[k].p) {
      if (objs[k].ov && !objs[k].sc) continue;
      const c = box(objs[k], objs[k].ov ? objs[k].ext || 0 : 0);
      b = { x1: Math.max(b.x1, c.x1), y1: Math.max(b.y1, c.y1), x2: Math.min(b.x2, c.x2), y2: Math.min(b.y2, c.y2) };
      if (b.x1 > b.x2 || b.y1 > b.y2) b = null;
    }
    if (!b) return;
    for (const g of regions) {
      if (g.i === r) continue;
      const n = inter(b, box(g.o));
      const key = `${r}>${g.i}`;
      if (n > 0 && !seen.has(key)) { seen.add(key); errs.push(`escape: ${name(objs, i)} (in ${name(objs, r)}) reaches ${n} px into ${name(objs, g.i)}`); }
    }
    if ((b.x1 < screen.x1 || b.y1 < screen.y1 || b.x2 > screen.x2 || b.y2 > screen.y2) && !seen.has(`${r}>off`)) {
      seen.add(`${r}>off`);
      errs.push(`escape: ${name(objs, i)} (in ${name(objs, r)}) leaves the screen`);
    }
  });
  return errs;
}

// ---------------------------------------------------------------- CLI
export default {
  id: 'screen-layout', tiers: ['fast', 'gate'], ownStage: true,
  about: 'Screens: regions do not overlap, nothing escapes its region or the screen, at the build, 1 s, 3 s and after an update',
  wants: (c) => c.section === 'screens',
  run(x, rep) {
    const { m, den, c, v, light } = x, S = den === '1x' ? 480 : 720;
    const str = (s, f) => { const p = m.stringToNewUTF8(s); try { return f(p); } finally { m._free(p); } };
    const defaults = Object.fromEntries((c.meta.props || []).map((p) => [p.name, p.default]));
    let t = m.__t || 0;
    const until = (end) => { while (t < end) { t += 16; m._fw_tick(t); } m.__t = t; };
    const look = light ? 'light' : 'dark';
    const errs = new Map(), put = (e, at) => errs.set(e, [...(errs.get(e) || []), at]);
    {  // the build, then 1 s and 3 s of updates
      const A = m._fw_stage(S, S), t0 = t;
      const e = str(c.id, (cp) => str(JSON.stringify({ ...defaults, ...v.props }), (pp) => m.UTF8ToString(m._fw_render(A, cp, pp, light, 0))));
      if (e) put(`render: ${e}`, `${v.label}`);
      else for (const at of [0, 1000, 3000]) {
        until(t0 + at);
        m._fw_events();
        for (const err of checkScreen(JSON.parse(m.UTF8ToString(m._fw_audit(A))), S, S)) put(err, `${v.label} · ${look} · ${at / 1000}s`);
      }
      m._fw_stage_free(A);
    }
    if (x.vi) {  // the update path from the default props to this variant must place things as the build does
      const A = m._fw_stage(S, S);
      const e0 = str(c.id, (cp) => str(JSON.stringify(defaults), (pp) => m.UTF8ToString(m._fw_render(A, cp, pp, light, 0))));
      const e = e0 || str(c.id, (cp) => str(JSON.stringify({ ...defaults, ...v.props }), (pp) => m.UTF8ToString(m._fw_update(A, cp, pp, light, 0))));
      if (e && e !== 'rebuild') put(`update: ${e}`, `${v.label}`);
      else if (e !== 'rebuild') {  // a rebuild = the build checked above
        until(t + 1000);
        m._fw_events();
        for (const err of checkScreen(JSON.parse(m.UTF8ToString(m._fw_audit(A))), S, S)) put(err, `default → ${v.label} (update) · ${look}`);
      }
      m._fw_stage_free(A);
    }
    for (const [e, ats] of errs) for (const at of ats) rep.check(false, `${c.id} [${den}] ${e}  — ${at}`);
    if (!errs.size) rep.check(true, `${c.id} [${den}] ${v.label} · ${look}`);
  },
};
