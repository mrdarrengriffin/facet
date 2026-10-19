/** Content for the Sounds page (src/pages/docs/sounds.astro) and its search records. */
import type { SoundId } from '@facet/sounds';

export interface SoundEventDoc {
  id: SoundId;
  /** What the sound means. */
  meaning: string;
  /** What plays it: the hooks, or your own code. */
  trigger: string;
}

export const soundEvents: SoundEventDoc[] = [
  {
    id: 'tap',
    meaning:
      'A plain button press. The quietest, shortest sound, because it is the one you hear most.',
    trigger: 'Hooks: any live button without a more specific sound.',
  },
  {
    id: 'open',
    meaning: 'Going deeper: a card or row opens its screen. Rising, so it reads as "forward".',
    trigger: 'Hooks: Home cards, "more" rows and anything with `data-snd="open"`.',
  },
  {
    id: 'back',
    meaning: 'Going back: the falling mirror of Open.',
    trigger: 'Hooks: back buttons, sheet scrims and `data-snd="back"`.',
  },
  {
    id: 'toggle-on',
    meaning: 'Something switches on or starts. Brighter and a little higher than Toggle off.',
    trigger: 'Hooks: the light ON/OFF button, Play, and `data-snd="toggle"` when it turns on.',
  },
  {
    id: 'toggle-off',
    meaning: 'Something switches off or pauses. Duller and lower, so you can tell the two apart.',
    trigger: 'Hooks: the same controls when they turn off (Pause, OFF).',
  },
  {
    id: 'slider-tick',
    meaning: 'One detent while dragging. Its pitch follows the value, so a sweep rises or falls.',
    trigger: 'Hooks: every 5 % on a slider and every 2 % on the light dial.',
  },
  {
    id: 'confirm',
    meaning: 'Done or saved: a short, positive two-note chime.',
    trigger: "Your code: `play('confirm')` when an action has finished.",
  },
  {
    id: 'timer-done',
    meaning: 'A timer has finished. A friendly phrase, longer than the rest, but never an alarm.',
    trigger: "Your code: `play('timer-done')` when a timer reaches zero.",
  },
  {
    id: 'error',
    meaning: 'Not possible: a low, muted knock. Never harsh.',
    trigger: 'Hooks: pressing a disabled button. Your code: an action that failed.',
  },
  {
    id: 'notify',
    meaning: 'Something new needs a glance: a calm ping.',
    trigger: "Your code: `play('notify')` when a toast appears (the `toast.sound` token names it).",
  },
];

export const soundsIntro =
  'Facet has ten UI sounds, one for each kind of moment, and six packs that voice them. Every sound is made live with Web Audio from a handful of numbers: there are no audio files to load.';
