// Checkbox: a 20 px rounded square. Open = a 2 px outline, doing = an orange dot in the centre, done = filled orange
// with a 2 px check. Accessible as a checkbox (`aria-checked` true / false / mixed); read-only unless `onChange`.
import { h, s } from './dom.js';

export type CheckState = 'open' | 'doing' | 'done';
export const CHECK_STATES: CheckState[] = ['open', 'doing', 'done'];

export interface CheckboxProps {
  state?: CheckState;
  /** Text next to the box. Done text turns dim. */
  label?: string;
  /** Makes it interactive: tapping cycles open → done (and doing → done). */
  onChange?: (state: CheckState) => void;
}

/** Just the box, e.g. for server-rendered markup: the same SVG check the component uses. */
export const CHECK_PATH = 'M5.5 10.5 L8.5 13.5 L14.5 7';

export function checkbox(o: CheckboxProps = {}) {
  let state: CheckState = o.state ?? 'open';
  const box = h(
    'span',
    { class: 'f-checkbox-box', 'aria-hidden': 'true' },
    s(
      'svg',
      { viewBox: '0 0 20 20', width: 20, height: 20 },
      s('path', {
        d: CHECK_PATH,
        fill: 'none',
        'stroke-width': 2,
        'stroke-linecap': 'round',
        'stroke-linejoin': 'round',
      }),
    ),
  );
  const text =
    o.label !== undefined ? h('span', { class: 'f-checkbox-label fst-body' }, o.label) : null;
  const el = h(
    'span',
    {
      class: 'f-checkbox',
      role: 'checkbox',
      tabindex: o.onChange ? 0 : undefined,
      'aria-readonly': o.onChange ? undefined : 'true',
    },
    box,
    text,
  );
  const render = () => {
    el.dataset.state = state;
    el.setAttribute(
      'aria-checked',
      state === 'done' ? 'true' : state === 'doing' ? 'mixed' : 'false',
    );
  };
  if (o.onChange) {
    el.classList.add('live');
    const toggle = () => {
      state = state === 'done' ? 'open' : 'done';
      render();
      o.onChange!(state);
    };
    el.addEventListener('click', toggle);
    el.addEventListener('keydown', (e) => {
      if (e.key === ' ' || e.key === 'Enter') {
        e.preventDefault();
        toggle();
      }
    });
  }
  render();
  return Object.assign(el, {
    set(next: CheckState) {
      state = next;
      render();
    },
    get state() {
      return state;
    },
  });
}
