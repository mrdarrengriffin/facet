// Agents: how many need you (the hero), then a tile per agent, three across, sorted needs > working > done > idle.
import { motion } from '@facet/tokens';
import { at, CW, content, header, heroBlock, layout, page, tile } from '../layout.js';
import { h } from '../dom.js';
import type { Agent } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

const ORDER = { needs: 0, working: 1, done: 2, idle: 3 } as const;

export function agentsScreen(ctx: ScreenContext, o: ScreenOptions = {}): Screen {
  const hero = heroBlock({ label: 'need you' });
  const tiles = h('div', { class: 'fs-tiles' });
  const el = page(
    'fs-agents',
    header({ label: 'Agents', onBack: o.onBack }),
    content(at(0, 0, CW, 96, hero), at(0, 96 + layout.group, CW, null, tiles)),
  );
  let sig = '';
  return {
    el,
    update(s) {
      const list: Agent[] = [...(s.agents ?? [])].sort((a, b) => ORDER[a.kind] - ORDER[b.kind]);
      const key = JSON.stringify(list);
      if (key === sig) return;
      sig = key;
      const needs = list.filter((a) => a.kind === 'needs').length,
        working = list.filter((a) => a.kind === 'working').length;
      hero.set({
        n: String(needs),
        label: needs === 1 ? 'needs you' : 'need you',
        line: `${working} working · ${list.length - needs - working} done or idle`,
        accent: needs > 0,
      });
      let k = 0;
      tiles.replaceChildren(
        ...list.map((a) =>
          tile(a, a.kind === 'working' ? motion.duration['pulse-stagger'] * k++ : 0),
        ),
      );
    },
  };
}
