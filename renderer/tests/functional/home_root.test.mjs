// The start Home is a root wherever it is built: on its own (the editor's preview, a panel's POST /fw/show {"id": "home"})
// as on the device's stack. No Back there; a second Home page keeps its Back (it sits above Home).
export default {
  id: 'home', family: 'layout', about: 'Home on its own: the start page has no Back.',
  tests: [
    { name: 'the start Home on its own has no Back', async run(t) {
      await t.mount('home', {});
      t.ok(!t.part('back_button'), 'no Back on the start Home');
    } },
  ],
};
