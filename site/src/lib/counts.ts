// Every count the site prints, read at build time from what a panel serves (/fw/schema = renderer/catalogue/schema.json)
// and the spec catalogue (renderer/catalogue/catalogue.json): never typed into a page. A page prints one with
// <Count k="..." /> (components/Count.astro), which marks it; scripts/check-dist.mjs fails the build on a number before
// one of these nouns that is not marked.
import schema from '../../../renderer/catalogue/schema.json';
import catalogue from '../../../renderer/catalogue/catalogue.json';
import { COMPONENTS } from './data';

type Screen = { kind: string };
const specs = import.meta.glob('../../../renderer/specs/*.json', { query: '?raw', import: 'default', eager: true }) as Record<string, string>;
const screenIds = Object.entries(schema.screens as Record<string, Screen>).filter(([, s]) => s.kind === 'spec').map(([id]) => id);
const specKB = screenIds.map((id) => new TextEncoder().encode(specs[`../../../renderer/specs/${id}.json`] ?? '').length / 1024);
const budget = catalogue.budget as { nodes: number; binds: number; bytes: number; screens: number };
const components = Object.keys(catalogue.components).length;

export const COUNTS = {
  /** the factory screens a panel ships (schema screens of kind spec): what you choose from */
  screens: screenIds.length,
  /** the contract components a spec can name (catalogue) */
  components,
  /** the design-system foundations (schema components that are not spec components: colour, type, ...) */
  foundations: Object.keys(schema.components).length - components,
  tokens: (schema.tokens as unknown[]).length,
  routes: (schema.routes as unknown[]).length,
  settings: (schema.settings as { settings: unknown[] }).settings.length,
  /** a spec's budget (core/spec.h, the panel's check) */
  nodes: budget.nodes,
  binds: budget.binds,
  specKB: budget.bytes / 1024,
  deckScreens: budget.screens,
  /** the factory specs' sizes, to the nearest KB (smallest, largest) */
  specMinKB: Math.max(1, Math.round(Math.min(...specKB))),
  specMaxKB: Math.round(Math.max(...specKB)),
};
export type CountKey = keyof typeof COUNTS;

// the docs' component list is the catalogue's (one source): a mismatch is a build error, not a wrong number
if (COMPONENTS.length !== components) throw new Error(`counts: the docs list ${COMPONENTS.length} components, the catalogue ${components}`);
