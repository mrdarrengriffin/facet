/**
 * The UI explorer: a sidebar of every component, a live stage with controls for its props, the code to create it,
 * and its variants. The stage follows the site theme unless overridden; sounds can be switched on.
 */
import { tickClocks } from '@facet/ui';
import { fitFrame, initSounds, initThemeOverride, soundToggleFor } from '../facet-runtime';
import { controlEl } from '../controls';
import { entries, type Entry, type Props } from './registry';

type Destroyable = (HTMLElement | SVGElement) & { destroy?: () => void };

const $ = <T extends Element>(root: ParentNode, sel: string) => root.querySelector<T>(sel)!;
const el = <K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Record<string, string> = {},
  ...kids: (Node | string)[]
) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  e.append(...kids);
  return e;
};

export function initExplorer(root: HTMLElement): void {
  initSounds();
  const list = $<HTMLElement>(root, '[data-ex-list]');
  const filter = $<HTMLInputElement>(root, '[data-ex-filter]');
  const title = $<HTMLElement>(root, '[data-ex-title]');
  const group = $<HTMLElement>(root, '[data-ex-group]');
  const desc = $<HTMLElement>(root, '[data-ex-desc]');
  const controls = $<HTMLElement>(root, '[data-ex-controls]');
  const stage = $<HTMLElement>(root, '[data-ex-stage]');
  const code = $<HTMLElement>(root, '[data-ex-code]');
  const copy = $<HTMLButtonElement>(root, '[data-ex-copy]');
  const variantsBox = $<HTMLElement>(root, '[data-ex-variants]');
  const variantsSection = $<HTMLElement>(root, '[data-ex-variants-section]');
  initThemeOverride(root, $<HTMLElement>(root, '[data-ex-themed]'));
  // the sound switch shows only for components that make a sound (only the stage is inside the sound scope)
  const soundCheck = soundToggleFor(root, stage);

  // ---- sidebar
  const groups = [...new Set(entries.map((e) => e.group))];
  const links = new Map<string, HTMLAnchorElement>();
  for (const g of groups) {
    const ul = el('ul');
    for (const e of entries.filter((x) => x.group === g)) {
      const a = el('a', { href: `#${e.id}`, 'data-id': e.id }, e.name);
      links.set(e.id, a);
      ul.append(el('li', {}, a));
    }
    list.append(
      el(
        'section',
        { class: 'sidebar__group', 'data-group': g },
        el('h2', { class: 'facet-label' }, g),
        ul,
      ),
    );
  }
  filter.addEventListener('input', () => {
    const q = filter.value.trim().toLowerCase();
    for (const e of entries)
      links.get(e.id)!.parentElement!.hidden =
        !!q && !`${e.name} ${e.group} ${e.description}`.toLowerCase().includes(q);
    for (const sec of list.querySelectorAll<HTMLElement>('[data-group]'))
      sec.hidden = ![...sec.querySelectorAll('li')].some((li) => !li.hidden);
  });

  // ---- stage
  let current: Entry = entries[0]!;
  let props: Props = {};
  let shown: Destroyable | null = null;
  const destroyIn = (box: Element) =>
    box.querySelectorAll<HTMLElement & { destroy?: () => void }>('*').forEach((n) => n.destroy?.());

  function renderStage() {
    shown?.destroy?.();
    destroyIn(stage);
    stage.replaceChildren();
    const node = current.render(props) as Destroyable;
    shown = node;
    const size = typeof current.size === 'function' ? current.size(props) : current.size;
    if (size) {
      const frame = el('div', { class: 'ex-frame' });
      frame.style.width = `${size[0]}px`;
      frame.style.height = `${size[1]}px`;
      frame.append(node);
      const vp = el('div', { class: 'ex-fit' });
      vp.append(frame);
      stage.append(vp);
      fitFrame(vp, frame, size[0], size[1]);
    } else stage.append(el('div', { class: 'ex-center' }, node as Node));
    code.textContent = current.code(props);
    stage.classList.toggle('is-big', !!size);
    soundCheck(current.sound);
  }

  function renderControls() {
    controls.replaceChildren();
    if (!current.controls.length)
      controls.append(el('p', { class: 'ex-none' }, 'No props to change.'));
    for (const c of current.controls)
      controls.append(
        controlEl(
          c,
          props[c.key]!,
          (v) => {
            props[c.key] = v;
            renderStage();
          },
          `ex-${current.id}`,
        ),
      );
  }

  function renderVariants() {
    destroyIn(variantsBox);
    variantsBox.replaceChildren();
    const vs = current.variants?.() ?? [];
    variantsSection.hidden = vs.length === 0;
    for (const v of vs)
      variantsBox.append(
        el(
          'figure',
          { class: 'ex-variant' },
          el('div', { class: 'ex-variant__cell' }, v.el as Node),
          el('figcaption', {}, v.label),
        ),
      );
  }

  function select(id: string) {
    current = entries.find((e) => e.id === id) ?? entries[0]!;
    props = Object.fromEntries(current.controls.map((c) => [c.key, c.value]));
    title.textContent = current.name;
    group.textContent = current.group;
    desc.textContent = current.description;
    for (const [k, a] of links) a.setAttribute('aria-current', k === current.id ? 'page' : 'false');
    renderControls();
    renderStage();
    renderVariants();
  }

  copy.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(code.textContent ?? '');
      copy.textContent = 'Copied';
      setTimeout(() => (copy.textContent = 'Copy'), 1200);
    } catch {
      copy.textContent = 'Select and copy';
    }
  });
  window.addEventListener('hashchange', () => select(location.hash.slice(1)));
  select(location.hash.slice(1) || 'button');
  // live clocks on the stage and in the variants
  setInterval(() => {
    if (current.live) tickClocks(stage);
  }, 1000);
}
