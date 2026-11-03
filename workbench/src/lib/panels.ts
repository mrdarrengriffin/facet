// The panels the workbench can show things on and send specs to: FACET_PANELS, a comma list of
// <name>=<host>@<board-id>, e.g. "office=192.168.1.40@waveshare-p4-86,bench=192.168.1.41@guition-4848s040".
// The board (boards/<id>.json) gives the size and density. A panel needs the workbench firmware (esphome/) and, for
// anything but its own pages, "Allow API access" on and a key (FACET_PANEL_KEYS: lib/panel-auth.ts).
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { REPO } from './repo';

export type Panel = { name: string; host: string; board: string; title: string; density: '1x' | '1.5x'; size: number };
export function panels(): Record<string, Panel> {
  const out: Record<string, Panel> = {};
  for (const item of (process.env.FACET_PANELS ?? '').split(',').map((s) => s.trim()).filter(Boolean)) {
    const m = item.match(/^([a-z0-9_-]+)=([^@\s]+)@([a-z0-9-]+)$/);
    if (!m) continue;
    try {
      const b = JSON.parse(readFileSync(join(REPO, 'boards', `${m[3]}.json`), 'utf8'));
      out[m[1]] = { name: m[1], host: m[2], board: b.id, title: `${m[1]} (${b.name}, ${b.resolution.w})`, density: b.scale, size: b.resolution.w };
    } catch { /* unknown board: left out */ }
  }
  return out;
}
