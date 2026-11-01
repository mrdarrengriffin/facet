// sound-usage: "what uses what sound", generated from the code at startup (tools/gen_sound_usage.mjs), never committed.
// Its warnings (a component that makes a sound without a when-line in sound.json usage, or the reverse) are notes.
import { usage } from '../../tools/gen_sound_usage.mjs';

export default {
  id: 'sound-usage', tiers: ['fast', 'gate'], dens: ['1.5x'],
  about: 'The sound usage is generated from the code; its when-lines are listed',
  once(x, rep) {
    rep.check(usage.components.length > 0, `sound-usage: ${usage.components.length} components make a sound, ${usage.warnings.length} without a when-line or with an unknown id`);
    for (const w of usage.warnings) rep.note(`sound-usage: ${w}`);
  },
};
