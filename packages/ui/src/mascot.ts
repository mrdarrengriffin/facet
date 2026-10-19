// The mascot: a 32 × 24 body with two eyes. Emotion comes only from small geometry and motion, nothing cartoony:
//   eyes     round · squint · closed line · wide · happy arches
//   brows    small bars above the eyes, tilted a few degrees (only some moods use them)
//   motion   blink, glance, bob, breathing, a hop when something finishes, a tilt when it needs you
//   extras   an orange "!" (needs you), three "z" drifting up from the head (sleeping), working-variant props
// On a panel each part is a tiny LVGL object and every motion is an animation of its x, y, height or opacity.
import { h, s as svg } from './dom.js';

export const MOODS = ['idle', 'working', 'done', 'needs', 'error', 'sleeping'] as const;
export type Mood = (typeof MOODS)[number];

export const MOOD_INFO: Record<Mood, { colour: string; what: string }> = {
  idle: { colour: 'steel', what: 'slow blink, glances left/right' },
  working: {
    colour: 'accent',
    what: 'calm: eyes scanning left -> right like reading, occasional blink, 3 progress dots',
  },
  done: { colour: 'steel', what: 'happy arches, hop on arrival' },
  needs: { colour: 'warn', what: 'wide eyes, raised brows, tilt, orange "!"' },
  error: { colour: 'error', what: 'droopy half-closed eyes, sad brows, slumps 1 px' },
  sleeping: { colour: 'text-mute', what: 'closed eyes, slow breathing, three "z" rising up-right' },
};

/** Ways to show "working". C (reading) is the default; `setWorkVariant()` switches every mascot that doesn't fix
 *  its own. All of them only move small parts (x / y / opacity / rotation), so they work on a panel too. */
export const WORK_VARIANTS = [
  { id: 'a', name: 'A · Focused', what: 'soft squint + 1 px bob ' },
  {
    id: 'b',
    name: 'B · Typing',
    what: 'eyes down at a keyboard line under the mascot, keystroke bob, blinking cursor block',
  },
  {
    id: 'c',
    name: 'C · Reading',
    what: 'eyes dart left -> right in a steady rhythm, occasional blink, 3 progress dots',
  },
  {
    id: 'd',
    name: 'D · Thinking',
    what: 'eyes up and to the side, one brow raised, three "..." dots pulsing above',
  },
  {
    id: 'e',
    name: 'E · Builder',
    what: 'determined flat brows + squint, a small gear turning beside the head',
  },
] as const;
export type WorkVariant = (typeof WORK_VARIANTS)[number]['id'];
let workVariant: WorkVariant = 'c';
export const getWorkVariant = () => workVariant;
/** Switch every mascot that follows the default (mascots created with `work` keep theirs). */
export function setWorkVariant(v: WorkVariant) {
  workVariant = v;
  if (typeof document !== 'undefined')
    document
      .querySelectorAll<HTMLElement>('.fm-mascot:not([data-work-fixed])')
      .forEach((m) => (m.dataset.work = v));
}

/** Agents tile kind + status text -> mood. done in the last 2 h = done (happy), older = idle (awake, blinking);
 *  idle tiles sleep; "error"/"failed" in the status = error. */
export function moodFor(kind: string, status = ''): Mood {
  if (/error|failed/i.test(status)) return 'error';
  if (kind === 'working') return 'working';
  if (kind === 'needs') return 'needs';
  if (kind === 'idle') return 'sleeping';
  if (kind === 'done') {
    const m = status.match(/(\d+)\s*(m|h|d|w)\b/);
    const mins = m
      ? Number(m[1]) * ({ m: 1, h: 60, d: 1440, w: 10080 } as Record<string, number>)[m[2]]
      : 9999;
    return mins <= 120 ? 'done' : 'idle';
  }
  return 'idle';
}

/** Mascot component: mood prop (+ optional colour token override); set(mood) re-plays the arrival motion (hop). */
export function mascotMood(o: { mood: Mood; colour?: string; delay?: number; work?: WorkVariant }) {
  const parts = h(
    'b',
    { class: 'fm-eyes' },
    h('b', { class: 'fm-brow fm-l' }),
    h('b', { class: 'fm-brow fm-r' }),
    h('b', { class: 'fm-eye fm-l' }),
    h('b', { class: 'fm-eye fm-r' }),
  );
  const fx = h(
    'b',
    { class: 'fm-fx' },
    h('b', { class: 'fm-orbit' }, h('b')),
    h('b', { class: 'fm-bang' }, h('b'), h('b')),
    // sleeping: three "z" = 3-segment 2 px polylines (LVGL lv_line, 4 points), 6 / 8 / 10 px
    h(
      'b',
      { class: 'fm-z' },
      ...[7, 7, 7].map((z, i) =>
        svg(
          'svg',
          {
            class: `fm-zz fm-zz${i + 1}`,
            width: z + 2,
            height: z + 2,
            viewBox: `-1 -1 ${z + 2} ${z + 2}`,
          },
          svg('polyline', {
            points: `0,0 ${z},0 0,${z} ${z},${z}`,
            fill: 'none',
            stroke: 'currentColor',
            'stroke-width': 1.75,
            'stroke-linecap': 'round',
            'stroke-linejoin': 'round',
          }),
        ),
      ),
    ),
    // working variants: B keyboard line + cursor, C progress dots, D thinking dots, E gear
    h('b', { class: 'fm-kb' }, h('b', { class: 'fm-kb-line' }), h('b', { class: 'fm-kb-cur' })),
    h('b', { class: 'fm-prog' }, h('b'), h('b'), h('b')),
    h('b', { class: 'fm-think' }, h('b'), h('b'), h('b')),
    h('b', { class: 'fm-gear' }, h('b'), h('b'), h('b')),
  );
  const bodyEl = h('b', { class: 'fm-body' }, parts);
  const el = h(
    'span',
    {
      class: 'f-mascot fm-mascot',
      'data-work': o.work ?? workVariant,
      'data-work-fixed': o.work ? '' : undefined,
    },
    h('b', { class: 'fm-hop' }, bodyEl),
    fx,
  );
  // de-synchronise the idle blink/glance between tiles (stable per instance)
  el.style.setProperty('--fm-off', `${-(o.delay || Math.floor(Math.random() * 5000))}ms`);
  const set = (mood: Mood, colour?: string) => {
    const prev = el.dataset.mood;
    el.dataset.mood = mood;
    el.style.setProperty(
      '--fm-c',
      `var(--facet-color-${colour ?? o.colour ?? MOOD_INFO[mood].colour})`,
    );
    if (mood === 'done' && prev !== 'done') {
      el.classList.remove('is-hop');
      void el.offsetWidth;
      el.classList.add('is-hop');
    }
  };
  set(o.mood, o.colour);
  return Object.assign(el, {
    set,
    get mood() {
      return el.dataset.mood as Mood;
    },
  });
}
