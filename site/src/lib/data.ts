// Generated data (scripts/extract.mjs -> src/data/*.json) and small helpers shared by the pages.
import components from '../data/components.json';
import foundations from '../data/foundations.json';
import site from '../data/site.json';
import tokens from '../data/tokens.json';
import sound from '../data/sound-packs.json';
import icons from '../data/icons.json';
import { COMPONENT_GROUPS } from './groups';

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

export const ACCENTS = (TOKENS.colour as { accent: { default: string; sets: { id: string; label: string }[] } }).accent;

/** icons.json stroke.ui: px stroke at 16 / 24 / 32 / 48 px, linear in between, clamped; returned in 24-grid units */
export function opticalStroke(size: number): number {
  const t = (icons as { stroke: [number, number][] }).stroke;
  let px = t[t.length - 1][1];
  for (let i = 0; i < t.length; i++) {
    if (size <= t[i][0]) { px = i === 0 ? t[0][1] : t[i - 1][1] + ((t[i][1] - t[i - 1][1]) * (size - t[i - 1][0])) / (t[i][0] - t[i - 1][0]); break; }
  }
  return Math.round(((px * 24) / size) * 1000) / 1000;
}
type Icon = { kind: string; viewBox: string; inner: string };
/** an icon of the WASM set as inline SVG (ui / screen: currentColor strokes; weather: its own layers) */
export function iconSvg(name: string, size = 24, cls = ''): string {
  const i = (icons as { icons: Record<string, Icon> }).icons[name];
  if (!i) return '';
  const stroke = i.kind === 'weather' ? '' : ` fill="none" stroke="currentColor" stroke-width="${opticalStroke(size)}" stroke-linecap="round" stroke-linejoin="round"`;
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${i.viewBox}" width="${size}" height="${size}" aria-hidden="true" class="icon ${cls}"${stroke}>${i.inner}</svg>`;
}

/** components in sidebar groups (lib/groups.ts order; unknown ids last, under Other) */
export function componentGroups(): { label: string; items: Entry[] }[] {
  const known = new Set(COMPONENT_GROUPS.flatMap(([, ids]) => ids));
  const out = COMPONENT_GROUPS.map(([label, ids]) => ({ label, items: ids.map((id) => COMPONENTS.find((c) => c.id === id)).filter((c): c is Entry => !!c) }));
  const other = COMPONENTS.filter((c) => !known.has(c.id));
  if (other.length) out.push({ label: 'Other', items: other });
  return out.filter((g) => g.items.length);
}
export const groupOf = (id: string) => componentGroups().find((g) => g.items.some((c) => c.id === id))?.label ?? 'Components';
