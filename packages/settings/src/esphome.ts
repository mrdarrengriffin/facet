/**
 * Settings → ESPHome YAML: the panel's Home Assistant entities and its web_server page, from the schema.
 *
 * Include it as a package (`packages: { settings: !include settings.yaml }`). It declares every setting as a template
 * select / number / switch (config category, optimistic, restored after a reboot) and groups them on the ESPHome
 * web_server v3 page in the schema's groups and order. What a setting does stays in the firmware: add it with
 * `select: [{ id: !extend theme_select, on_value: [...] }]`. Board defaults (`boardDefault`) come from a substitution
 * (`default_<id>`, e.g. `default_transition: Instant`) that the board's own config can override.
 */
import { groups, settings, type Setting } from './schema.js';

const q = (v: string | number) => JSON.stringify(String(v));
const snake = (s: string) => s.replace(/[A-Z]/g, (c) => `_${c.toLowerCase()}`);

function entity(s: Setting, weight: number): string[] {
  const out = [
    `  - platform: template`,
    `    id: ${s.esphome.id}`,
    `    name: ${q(s.esphome.name)}`,
    `    icon: ${q(s.esphome.icon)}`,
    `    entity_category: config`,
    `    optimistic: true`,
  ];
  if (s.kind === 'select') {
    const label = (id: string) => s.options.find((o) => o.id === id)!.label;
    out.push(
      `    options: [${s.options.map((o) => q(o.label)).join(', ')}]`,
      `    initial_option: ${s.boardDefault ? q(`\${default_${snake(s.id)}}`) : q(label(s.default))}`,
      `    restore_value: true`,
    );
  } else if (s.kind === 'number') {
    out.push(
      `    min_value: ${s.min}`,
      `    max_value: ${s.max}`,
      `    step: ${s.step}`,
      `    initial_value: ${s.default}`,
      `    unit_of_measurement: ${q(s.unit)}`,
      `    mode: ${s.control === 'slider' ? 'slider' : 'box'}`,
      `    restore_value: true`,
    );
  } else {
    out.push(`    restore_mode: ${s.default ? 'RESTORE_DEFAULT_ON' : 'RESTORE_DEFAULT_OFF'}`);
  }
  out.push(`    web_server: { sorting_group_id: settings_${s.group}, sorting_weight: ${weight} }`);
  return out;
}

export function toEsphome(): string {
  const out = ['# Generated from @facet/settings (settings.json). Do not edit by hand.', ''];
  const boardDefaults = settings.filter((s) => s.boardDefault && s.kind === 'select');
  if (boardDefaults.length) {
    out.push('substitutions:');
    for (const s of boardDefaults)
      if (s.kind === 'select')
        out.push(
          `  default_${snake(s.id)}: ${q(s.options.find((o) => o.id === s.default)!.label)}`,
        );
    out.push('');
  }
  out.push('web_server:', '  version: 3', '  sorting_groups:');
  groups.forEach((g, i) =>
    out.push(
      `    - id: settings_${g.id}`,
      `      name: ${q(g.label)}`,
      `      sorting_weight: ${(i + 1) * 10}`,
    ),
  );
  for (const kind of ['select', 'number', 'switch'] as const) {
    const list = settings.filter((s) => s.kind === kind);
    if (!list.length) continue;
    out.push('', `${kind}:`);
    for (const s of list)
      out.push(...entity(s, settings.filter((x) => x.group === s.group).indexOf(s) + 1));
  }
  return `${out.join('\n')}\n`;
}
