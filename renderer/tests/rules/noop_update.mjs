// An update that changes nothing (the same props + one unknown key, so the update path runs) leaves every object
// where it was, in every variant: an update path that drops a root style (2026-10-28: the Hero block's refill took
// its box style off and it shrank to its text) fails here, whatever the audit's frame count sees.
// Moved from tools/lab_test.mjs onto the runner's loop: per contract component x variant, on a 720 x 720 stage of its own.
export default {
  id: 'noop-update', tiers: ['gate'], order: 20, looks: [0],
  about: 'A no-op update leaves every object of a contract component in place, in every variant',
  wants: (c) => c.section !== 'screens' && c.meta?.contract === 2,
  run(x, rep) {
    const { m, den, v } = x, id = x.c.id;
    let t = m.__t || 0;
    const tick = (n = 4) => { for (let k = 0; k < n; k++) { t += 16; m._fw_tick(t); } m.__t = t; };
    const S = (p, f) => { const q = m.stringToNewUTF8(p); try { return f(q); } finally { m._free(q); } };
    const ok = (cond, what) => rep.check(cond, what);
    const s2 = m._fw_stage(720, 720), p = { ...v.props, opening: false, timer: false };
    S(id, (pc) => S(JSON.stringify(p), (pp) => m._fw_render(s2, pc, pp, 0, 0))); tick(60);
    const r0 = m.UTF8ToString(m._fw_rects(s2));
    S(id, (pc) => S(JSON.stringify({ ...p, _noop: 1 }), (pp) => m._fw_update(s2, pc, pp, 0, 0))); tick(60);
    ok(m.UTF8ToString(m._fw_rects(s2)) === r0, `${den} ${id} [${v.label}] a no-op update leaves every object in place`);
    m._fw_stage_free(s2);
  },
};
