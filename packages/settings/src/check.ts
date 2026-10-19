/** Schema checks, run by the build: a broken schema fails `pnpm build`. */
import { groups, settings, type AccentRole, type ThemeName } from './schema.js';

const lum = (hex: string) => {
  const c = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
  const f = (v: number) => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
  return 0.2126 * f(c[0]!) + 0.7152 * f(c[1]!) + 0.0722 * f(c[2]!);
};
/** WCAG contrast ratio of two #RRGGBB colours. */
export function contrast(a: string, b: string): number {
  const [hi, lo] = [lum(a), lum(b)].sort((x, y) => y - x) as [number, number];
  return (hi + 0.05) / (lo + 0.05);
}

/** Canvas and card colours per theme (tokens color.bg / color.surface). */
export const BACKGROUNDS: Record<ThemeName, string[]> = {
  dark: ['#000000', '#0A0B0E'],
  light: ['#F4F2EE', '#FFFFFF'],
};
/** Minimum contrast per accent role, against the canvas and cards (on-accent: against the accent). */
export const MIN_CONTRAST: Partial<Record<AccentRole, number>> = {
  accent: 3,
  'accent-text': 4.5,
  'on-accent': 4.5,
};

export function check(packs: string[] = []): string[] {
  const errors: string[] = [];
  const ids = new Set<string>(),
    eids = new Set<string>(),
    gids = new Set(groups.map((g) => g.id));
  for (const s of settings) {
    const at = `setting ${s.id}`;
    if (ids.has(s.id)) errors.push(`${at}: duplicate id`);
    if (eids.has(s.esphome.id)) errors.push(`${at}: duplicate esphome id ${s.esphome.id}`);
    ids.add(s.id);
    eids.add(s.esphome.id);
    if (!gids.has(s.group)) errors.push(`${at}: unknown group ${s.group}`);
    if (s.kind === 'select') {
      if (!s.options.some((o) => o.id === s.default))
        errors.push(`${at}: default ${s.default} is not an option`);
      if (new Set(s.options.map((o) => o.label)).size !== s.options.length)
        errors.push(`${at}: duplicate option labels`);
    }
    if (s.kind === 'number') {
      for (const v of [s.default, ...(s.presets ?? [])])
        if (v < s.min || v > s.max || (v - s.min) % s.step)
          errors.push(`${at}: ${v} is outside ${s.min}..${s.max} step ${s.step}`);
    }
    if (s.id === 'accent' && s.kind === 'select') {
      for (const o of s.options)
        for (const th of ['dark', 'light'] as ThemeName[]) {
          const c = o.colors?.[th];
          if (!c) {
            errors.push(`accent ${o.id}: no ${th} colours`);
            continue;
          }
          for (const [role, min] of Object.entries(MIN_CONTRAST) as [AccentRole, number][]) {
            const against = role === 'on-accent' ? [c.accent] : BACKGROUNDS[th];
            for (const b of against) {
              const r = contrast(c[role], b);
              if (r < min)
                errors.push(
                  `accent ${o.id} ${th}: ${role} ${c[role]} on ${b} is ${r.toFixed(2)}:1 (min ${min})`,
                );
            }
          }
        }
    }
    if (s.id === 'soundPack' && s.kind === 'select' && packs.length) {
      const opts = s.options.map((o) => o.id).join(',');
      if (opts !== packs.join(','))
        errors.push(`soundPack options ${opts} differ from @facet/sounds packs ${packs.join(',')}`);
    }
  }
  return errors;
}
