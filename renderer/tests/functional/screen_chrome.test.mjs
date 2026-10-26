// Screen chrome (components/screen_chrome.cpp): the header's right end (shortcut, settings, timer status, time).
// facet-180: with no timer running the timer status takes no room (no empty gap before the time).
export default {
  id: 'screen_chrome', family: 'layout', about: 'Header right end: shortcut, settings button, timer time left, the time.',
  tests: [
    { name: 'status timer with no timer running: no gap (same width as no status)', async run(t) {
      await t.mount('screen_chrome', { shortcut: 'timer', status: 'none' }); const none = t.part('screen_chrome');
      await t.mount('screen_chrome', { shortcut: 'timer', status: 'timer' }); const idle = t.part('screen_chrome');
      t.eq(idle.w, none.w, 'chrome width with an idle timer status');
      const btns = t.objs().filter((o) => o.clk && t.visible(o)), time = t.objs().filter((o) => o.text && /^\d{1,2}:\d{2}$/.test(o.text) && t.visible(o)).at(-1);
      const right = Math.max(...btns.filter((o) => !time || o.x < time.x).map((o) => o.x + o.w));
      if (time) t.ok(time.x - right <= 48, `gap between the buttons and the time: ${time.x - right}px`);
    } },
  ],
};
