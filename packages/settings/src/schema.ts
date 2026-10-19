/**
 * Facet personalisation settings.
 *
 * `settings.json` is the single source of truth: one entry per setting, in groups. The on-panel Settings screen,
 * the panel's Home Assistant entities (`toEsphome()`), the ESPHome web_server page and the docs playground
 * controls are all generated from it, so they can't drift.
 */
import data from './settings.json' with { type: 'json' };

export type ThemeName = 'dark' | 'light';
export type SettingKind = 'select' | 'number' | 'switch';
/** How the Settings screen shows the setting. */
export type Control = 'segmented' | 'swatches' | 'sheet' | 'slider' | 'presets' | 'toggle';
export type AccentRole = 'accent' | 'accent-press' | 'accent-dim' | 'accent-text' | 'on-accent';

export interface Group {
  id: string;
  label: string;
}

export interface Option {
  id: string;
  /** Shown on the panel and used as the Home Assistant option string. */
  label: string;
  hint?: string;
  /** Accent sets: the five accent colour roles per theme. */
  colors?: Record<ThemeName, Record<AccentRole, string>>;
  /** Text sizes: font size per type role (line boxes stay the same). */
  type?: Record<string, number>;
}

export interface EsphomeEntity {
  /** ESPHome component id (kept stable: Home Assistant entity ids follow the name, automations the id). */
  id: string;
  /** Entity name = what Home Assistant shows and builds the entity id from. */
  name: string;
  icon: string;
}

interface Base {
  id: string;
  group: string;
  label: string;
  control: Control;
  about?: string;
  esphome: EsphomeEntity;
  /** The default is set per board (e.g. Instant on slower boards). */
  boardDefault?: boolean;
}
export interface SelectSetting extends Base {
  kind: 'select';
  default: string;
  options: Option[];
}
export interface NumberSetting extends Base {
  kind: 'number';
  default: number;
  min: number;
  max: number;
  step: number;
  unit: string;
  /** Values offered as one-tap choices (control `presets`). */
  presets?: number[];
  /** What 0 means, e.g. "Never". */
  zero?: string;
}
export interface SwitchSetting extends Base {
  kind: 'switch';
  default: boolean;
}
export type Setting = SelectSetting | NumberSetting | SwitchSetting;
export type Value = string | number | boolean;
export type Values = Record<string, Value>;

export const version: number = data.version;
export const groups: readonly Group[] = data.groups;
export const settings: readonly Setting[] = data.settings as Setting[];

export const setting = (id: string): Setting | undefined => settings.find((s) => s.id === id);
export const inGroup = (group: string): Setting[] => settings.filter((s) => s.group === group);

/** Every setting at its default. */
export function defaults(): Values {
  return Object.fromEntries(settings.map((s) => [s.id, s.default]));
}

/** A value made valid for its setting: unknown options fall back to the default, numbers clamp and snap to step. */
export function coerce(s: Setting, v: unknown): Value {
  if (s.kind === 'switch')
    return typeof v === 'boolean' ? v : v === 'on' || v === 'true' || v === 1 ? true : s.default;
  if (s.kind === 'select') {
    const o = s.options.find((x) => x.id === v || x.label === v);
    return o ? o.id : s.default;
  }
  const n = Number(v);
  if (!Number.isFinite(n)) return s.default;
  const snapped =
    Math.round((Math.min(s.max, Math.max(s.min, n)) - s.min) / s.step) * s.step + s.min;
  return Math.min(s.max, snapped);
}

/** Merge saved values over the defaults, dropping unknown ids and invalid values. */
export function resolve(saved: Partial<Values> = {}): Values {
  return Object.fromEntries(
    settings.map((s) => [s.id, s.id in saved ? coerce(s, saved[s.id]) : s.default]),
  );
}

export function option(s: Setting, id: Value): Option | undefined {
  return s.kind === 'select' ? s.options.find((o) => o.id === id) : undefined;
}

/** Short text for a number: 0 → "Never", 90 s → "90 s", 120 s → "2 min", 60 min → "1 h". */
export function formatNumber(s: NumberSetting, v: number): string {
  if (v === 0 && s.zero) return s.zero;
  if (s.unit === 's' && v >= 60 && v % 60 === 0) return `${v / 60} min`;
  if (s.unit === 'min' && v >= 60 && v % 60 === 0) return `${v / 60} h`;
  return s.unit === '%' ? `${v}%` : `${v} ${s.unit}`;
}

/** The value as the panel shows it. */
export function format(s: Setting, v: Value): string {
  if (s.kind === 'switch') return v ? 'On' : 'Off';
  if (s.kind === 'select') return option(s, v)?.label ?? String(v);
  return formatNumber(s, Number(v));
}

/** Home Assistant entity id for a device (`<domain>.<device>_<name slug>`). */
export function entityId(s: Setting, device: string): string {
  const slug = s.esphome.name
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '_')
    .replace(/^_|_$/g, '');
  return `${s.kind}.${device}_${slug}`;
}

/** The accent set for a value (unknown = the default, Ember). */
export function accentColors(id: Value, theme: ThemeName): Record<AccentRole, string> {
  const s = setting('accent') as SelectSetting;
  const o = option(s, id) ?? option(s, s.default)!;
  return o.colors![theme];
}
