// Generated data (scripts/extract.mjs -> src/data/*.json) and small helpers shared by the pages.
import components from '../data/components.json';
import foundations from '../data/foundations.json';
import site from '../data/site.json';
import tokens from '../data/tokens.json';
import sound from '../data/sound-packs.json';

export type Prop = { name: string; type: 'text' | 'select' | 'range' | 'bool'; default?: unknown; options?: string[]; min?: number; max?: number; step?: number };
export type Variant = { label: string; about?: string; props: Record<string, unknown> };
export type Meta = { about?: string; demo?: boolean; layout?: 'doc'; events?: boolean; frame: [number, number]; props?: Prop[]; variants?: Variant[] };
export type Status = 'wip' | 'done' | 'edited';
export type Entry = {
  id: string; name: string; section: string; meta: Meta; lede: string; ledeKind: 'draft' | 'about' | 'fallback'; description: string; notes: string;
  events: string[]; uses: string[]; sounds: { id: string; name: string; for: string }[]; status: Status;
  source: { path: string; url: string }[];
};

export const COMPONENTS = (components as unknown as Entry[]).slice().sort((a, b) => a.name.localeCompare(b.name));
export const FOUNDATIONS = foundations as unknown as Entry[];
export const SITE = site;
export const TOKENS = tokens;
/** sound packs (variants of tokens/sound.json) for the header picker; the full sound list is fetched on first use */
export const SOUND = sound as { default: string; variants: { id: string; name: string; about: string }[] };
export const ALL = [...FOUNDATIONS, ...COMPONENTS];

export const BASE = import.meta.env.BASE_URL.replace(/\/?$/, '/');
export const href = (e: Pick<Entry, 'id' | 'section'>) => `${BASE}${e.section}/${e.id.replace(/_/g, '-')}/`;
export const slug = (id: string) => id.replace(/_/g, '-');

export const STATUS: Record<Status, { label: string; hint: string }> = {
  wip: { label: 'WIP', hint: 'Work in progress: not approved yet' },
  done: { label: 'Done', hint: 'Approved and committed' },
  edited: { label: 'Done, edited', hint: 'Approved, changed since (the change is not approved yet)' },
};

export const defaults = (m: Meta) =>
  Object.fromEntries((m.props ?? []).map((p) => [p.name, p.default ?? (p.type === 'bool' ? false : p.type === 'range' ? p.min ?? 0 : '')]));

/** the values a prop takes, for the props table */
export function values(p: Prop): string {
  if (p.type === 'select') return (p.options ?? []).join(' · ');
  if (p.type === 'range') return `${p.min ?? 0} to ${p.max ?? ''}${p.step && p.step !== 1 ? `, step ${p.step}` : ''}`;
  if (p.type === 'bool') return 'true · false';
  return 'text';
}

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
/** the small inline Markdown the README rows and source comments use: `code`, **bold** */
export function inline(s: string): string {
  return esc(s).replace(/`([^`]+)`/g, '<code>$1</code>').replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
}

/** colour tokens -> CSS custom properties (both themes, every accent set) */
export function themeCss(): string {
  const c = TOKENS.colour as { colours: Record<string, { dark: string; light: string }>; accent: { default: string; roles: string[]; sets: { id: string; dark: Record<string, string>; light: Record<string, string> }[] } };
  const vars = (t: 'dark' | 'light') => Object.entries(c.colours).map(([k, v]) => `--c-${k}:${v[t]};`).join('');
  const set = (t: 'dark' | 'light', s: (typeof c.accent.sets)[number]) => Object.entries(s[t]).map(([k, v]) => `--c-${k}:${v};`).join('');
  let css = `:root{${vars('dark')}color-scheme:dark}:root[data-theme=light]{${vars('light')}color-scheme:light}`;
  for (const s of c.accent.sets) {
    css += `:root[data-accent=${s.id}]{${set('dark', s)}}:root[data-theme=light][data-accent=${s.id}]{${set('light', s)}}`;
  }
  return css;
}
export const ACCENTS = (TOKENS.colour as { accent: { default: string; sets: { id: string; label: string }[] } }).accent;
