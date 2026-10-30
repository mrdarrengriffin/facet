// Robot vacuum screen (specs/vacuum.json: status_block, level_bar, room_tile per area, vacuum_mode, buttons; the Map
// view waits for the Iso floor plan on the contract) on the spec builder, bound to a Roborock-style vacuum in the fake Home Assistant: pick rooms
// -> a segment clean, Clean all, Pause / Resume / Dock, the mode sheet -> select options. Feature "vacuum" = WIP, on the contract
// firmware (features.json: every part "contract": 2).
const E = 'vacuum.robot', MODE = 'select.robot_mop_mode', MOP = 'select.robot_mop_intensity';
const screen = (extra = {}) => ['spec', { screen: 'vacuum', source: 'ha', entity: E, live: true, areas: 'Corridor:1,Living room:2,Kitchen:3', modesel: MODE, mopsel: MOP, ...extra }];
const last = (t, service) => t.ha.calls.filter((c) => c.service === service).at(-1);
const docked = (t) => { t.ha.set(E, 'docked', { fan_speed: 'balanced' }); t.ha.set(MODE, 'vacuum'); t.ha.set(MOP, 'moderate'); t.wait(600); };
export default {
  id: 'vacuum', family: 'home', about: 'Robot vacuum screen: pick rooms and clean, Pause / Resume / Dock, mode sheet.',
  tests: [
    { name: 'docked: state, a tile per room', mount: screen(), run(t) {
      docked(t);
      t.has(t.texts(), /^docked$/i, 'state'); for (const r of ['Corridor', 'Living room', 'Kitchen']) t.has(t.texts(), r, `${r} tile`);
    } },
    { name: 'nothing picked: Clean all calls vacuum.start', mount: screen(), run(t) {
      docked(t);
      t.tap(/^clean all$/i); t.call({ service: 'vacuum.start', entity: E }, 'start');
    } },
    { name: 'pick Kitchen: Clean Kitchen sends app_segment_clean [3]', mount: screen(), run(t) {
      docked(t);
      t.tap('Kitchen'); t.has(t.texts(), /^clean kitchen$/i, 'button label');
      t.tap(/^clean kitchen$/i);
      const c = last(t, 'vacuum.send_command');
      t.eq(JSON.stringify(c?.data), JSON.stringify({ command: 'app_segment_clean', params: [{ segments: [3], repeat: 1 }] }), 'segment clean');
    } },
    { name: 'pick two rooms: Clean 2 rooms sends both segments', mount: screen(), run(t) {
      docked(t);
      t.tap('Corridor'); t.tap('Kitchen'); t.tap(/^clean 2 rooms$/i);
      const segs = last(t, 'vacuum.send_command')?.data?.params?.[0]?.segments || [];
      t.eq([...segs].sort().join(','), '1,3', 'segments');
    } },
    { name: 'pick then unpick: back to Clean all', mount: screen(), run(t) {
      docked(t);
      t.tap('Kitchen'); t.tap('Kitchen'); t.has(t.texts(), /^clean all$/i, 'label');
    } },
    { name: 'cleaning: Pause calls vacuum.pause, then Resume calls vacuum.start', mount: screen(), run(t) {
      t.ha.set(E, 'cleaning'); t.wait(600);
      t.tap(/^pause$/i); t.call({ service: 'vacuum.pause', entity: E }, 'pause'); t.wait(600);
      t.tap(/^resume$/i); t.call({ service: 'vacuum.start', entity: E }, 'resume');
    } },
    { name: 'Dock calls vacuum.return_to_base', mount: screen(), run(t) {
      t.ha.set(E, 'cleaning'); t.wait(600);
      t.tap(/^dock$/i); t.call({ service: 'vacuum.return_to_base', entity: E }, 'dock');
    } },
    { name: 'mode sheet: picking a mode sets the select', mount: screen(), run(t) {
      docked(t);
      t.tap(/^mode$/i); t.wait(600);
      const opt = t.find(/^mop$/i) ? /^mop$/i : /mop/i;
      t.tap(opt); t.wait(600);
      t.call({ service: 'select.select_option', entity: MODE }, 'mode select');
    } },
    { name: 'contract: no Map view, every part on the contract (no iso_floorplan, no old Vacuum mode button)', mount: screen(), run(t) {
      docked(t);
      const c = JSON.stringify(t.comps());
      t.ok(!c.includes('"iso_floorplan"'), 'no floor plan'); t.ok(c.includes('"vacuum_mode"'), 'the mode row');
      t.ok(!t.texts().some((x) => /^map$/i.test(x)), 'no Map tab');
    } },
  ],
};
