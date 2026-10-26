// The device's navigation (core/device.cpp "Navigation", facet-188): Home is the root, Back = up one level, a page or a
// sent (agent / AI) screen sits one level above Home and a sent screen replaces the last one, depth Home + 2, hold Back
// = Home, the renderer owns Back, sending a spec never takes over the display. moon -> moon_details / moon_days are
// factory sub-views (not deck pages); weather is a deck page.
const SENT = ['nav_a', 'nav_b', 'nav_c'];
const spec = (id, extra = {}) => ({ v: 1, id, name: id.toUpperCase(), rev: 1, role: 'page', header: { label: id.toUpperCase() }, layout: 'list',
  nodes: [{ c: 'label', p: { text: `Sent ${id}` } }], ...extra });
const back = (t) => t.part('back_button');
const clean = (t) => SENT.forEach((id) => t.unsend(id));
export default {
  id: 'device', family: 'layout', about: 'The device app: Home, the screens it opens and the way back (navigation rules).',
  tests: [
    { name: 'open, Back = up one level, where am I', async run(t) {
      await t.device();
      t.eq(t.stack(), ['home'], 'start');
      t.ok(!back(t), 'Home has no Back');
      t.show('moon'); t.show('moon_details');
      t.eq(t.stack(), ['home', 'moon', 'moon_details'], 'Home > Moon > details');
      t.sounds(t.tap(back(t)), ['back']); t.eq(t.stack(), ['home', 'moon'], 'Back = Moon');
      t.tap(back(t)); t.eq(t.stack(), ['home'], 'Back = Home');
      t.ok(!back(t), 'Back gone on Home');
    } },
    { name: 'a page opens one level above Home', async run(t) {
      await t.device();
      t.show('moon'); t.show('moon_details'); t.show('weather');
      t.eq(t.stack(), ['home', 'weather'], 'weather page');
      t.tap(back(t)); t.eq(t.stack(), ['home'], 'Back = Home');
    } },
    { name: 'a second Home page is the parent of its header shortcut: Kitchen > Timer > Back = Kitchen', async run(t) {
      await t.device();
      t.show('kitchen'); t.eq(t.stack().length, 2, 'Kitchen one level above Home');
      const k = t.where();
      t.show('timer'); t.eq(t.stack(), ['home', k, 'timer'], 'Kitchen > Timer');
      t.tap(back(t)); t.eq(t.where(), k, 'Back = Kitchen');
      t.tap(back(t)); t.eq(t.stack(), ['home'], 'then Home');
    } },
    { name: 'from Home a page still opens one level above Home', async run(t) {
      await t.device();
      t.show('timer'); t.eq(t.stack(), ['home', 'timer'], 'Home > Timer');
      t.show('weather'); t.eq(t.stack(), ['home', 'weather'], 'page from a page: one level above Home');
    } },
    { name: 'depth capped at Home + 2: the deepest is replaced', async run(t) {
      await t.device();
      t.show('moon'); t.show('moon_details'); t.show('moon_days');
      t.eq(t.stack(), ['home', 'moon', 'moon_days'], 'three deep at most');
      t.tap(back(t)); t.eq(t.stack(), ['home', 'moon'], 'Back = up one level');
    } },
    { name: 'send 3 screens: each replaces the last, Back = Home', async run(t) {
      await t.device();
      t.show('moon'); t.show('moon_details');
      for (const id of SENT) { t.eq(t.send(spec(id)).status, 'applied', `send ${id}`); t.show(id); t.eq(t.stack(), ['home', id], `${id} one above Home`); }
      t.has(t.texts(), 'Sent nav_c', 'the last sent screen shows');
      t.tap(back(t)); t.eq(t.stack(), ['home'], 'Back = Home');
      clean(t);
    } },
    { name: 'sending or re-sending never takes over the display', async run(t) {
      await t.device();
      t.send(spec('nav_a')); t.eq(t.stack(), ['home'], 'send on Home: stays Home');
      t.show('moon');
      t.send(spec('nav_a', { rev: 2, nodes: [{ c: 'label', p: { text: 'Sent again' } }] }));
      t.eq(t.stack(), ['home', 'moon'], 're-send elsewhere: stays on Moon');
      t.show('nav_a'); t.has(t.texts(), 'Sent again', 'shown when asked');
      t.send(spec('nav_a', { rev: 3, nodes: [{ c: 'label', p: { text: 'Third' } }] })); t.wait(800);
      t.eq(t.stack(), ['home', 'nav_a'], 're-send while showing: same place');
      t.has(t.texts(), 'Third', 'rebuilt in place');
      clean(t);
    } },
    { name: 'hold Back = straight Home', async run(t) {
      await t.device();
      t.show('moon'); t.show('moon_details');
      const ev = t.hold(back(t), 900);
      t.eq(t.stack(), ['home'], 'Home'); t.emitted(ev, 'home'); t.sounds(ev, ['back']);
    } },
    { name: 'a spec cannot remove Back', async run(t) {
      await t.device();
      t.send(spec('nav_a', { header: { label: 'NO BACK', back: false } })); t.show('nav_a');
      t.ok(!!back(t), 'Back is there');
      t.tap(back(t)); t.eq(t.stack(), ['home'], 'and goes Home');
      clean(t);
    } },
    { name: 'cards layout: a tile tap -> nav.open opens the screen', async run(t) {
      await t.device();
      t.send(spec('nav_b', { layout: 'cards', nodes: [{ c: 'tile', p: { title: 'Moon tile', kind: 'idle', status: '' }, on: { tap: { do: 'nav.open', screen: 'moon' } } }] }));
      t.show('nav_b');
      t.tap('Moon tile'); t.eq(t.stack(), ['home', 'nav_b', 'moon'], 'tile opened Moon');
      t.tap(back(t)); t.eq(t.stack(), ['home', 'nav_b'], 'Back = the sent screen');
      clean(t);
    } },
  ],
};
