// Search index for the header search (scripts/search.ts, from the original site): one record per page and one per
// section (foundation sections, component variants / props / events / notes), all from the generated data.
import type { APIRoute } from 'astro';
import { getCollection } from 'astro:content';
import { COMPONENTS, FOUNDATIONS, SCREENS, href, screenHref, updateShown } from '../lib/data';
import { AI_EXAMPLES, UPDATES_PAGE, url } from '../lib/nav';
import examples from '../lib/ai-examples.json';

type Rec = { title: string; heading: string; text: string; url: string };
const anchor = (label: string, i: number) => `s${i + 1}-${label.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')}`;
const plain = (s: string) => s.replace(/`|\*\*/g, '').replace(/\s+/g, ' ').trim();

export const GET: APIRoute = async () => {
  const recs: Rec[] = [];
  recs.push({ title: 'Getting started', heading: '', text: 'What Facet is, what you need (a Waveshare ESP32-P4-86 or Guition ESP32-S3-4848S040 panel and Home Assistant) and the steps from install to your own screens.', url: url('/docs/') });
  recs.push({ title: 'Install', heading: '', text: 'Put Facet on your panel from the browser, connect it to Wi-Fi and add it to Home Assistant.', url: url('/install/') });
  const guides: [string, string, string][] = [
    ['Home cards', 'What can go on Home: tiles, cards, wide cards and readouts, what each card does when tapped, and choosing cards in Settings > Screens.', '/docs/home-cards/'],
    ['Updating and troubleshooting', 'Firmware updates in Home Assistant, the settling countdown, re-flashing with or without erase, Wi-Fi not joining, the panel not showing in Home Assistant, controls doing nothing.', '/docs/troubleshooting/'],
    ['FAQ and hardware', 'Supported panels (Waveshare ESP32-P4-86, Guition ESP32-S3-4848S040), other panels, Home Assistant, internet and cloud, ESPHome, own screens, cost and licence.', '/docs/faq/'],
    ['Components', 'Concepts: what a component is, screens made of components, props, variants, states, events, sounds and updates.', '/docs/concepts/components/'],
    ['Design tokens', 'Concepts: named values for colour, type, spacing, layout, motion, states and icons; the 4 px grid, contrast tests, 480 and 720 sizes, 16-bit colour.', '/docs/concepts/design-tokens/'],
    ['How the firmware works', 'Concepts: one renderer on the panel and in the browser, ESPHome, how the panel reads and controls Home Assistant, its own data, the deck, the panel API, updates.', '/docs/concepts/firmware/'],
  ];
  for (const [title, text, path] of guides) recs.push({ title, heading: '', text, url: url(path) });
  recs.push({ title: 'Design system', heading: '', text: 'The tokens and rules every component is built from: colour, type, space, layout, motion, states, icons, sound.', url: url('/foundations/') });
  recs.push({ title: 'Make screens with AI', heading: '', text: 'Let an AI assistant make panel screens from Facet components: specs, budgets, the panel API, Home Assistant actions and a starter prompt.', url: url('/ai/') });
  if (AI_EXAMPLES) for (const e of examples) recs.push({ title: e.title, heading: 'Example screen', text: plain(e.about), url: url(`/ai/examples/${e.id}/`) });
  recs.push({ title: 'Roadmap', heading: '', text: 'Where Facet is headed, area by area: available, in progress, planned.', url: url('/roadmap/') });
  if (UPDATES_PAGE) recs.push({ title: 'Updates', heading: '', text: 'How Facet came to be and what changed lately, newest first.', url: url('/updates/') });
  if (UPDATES_PAGE) for (const e of (await getCollection('updates')).filter(updateShown)) recs.push({ title: 'Updates', heading: e.data.title, text: (e.body ?? '').replace(/[>*]/g, '').replace(/\s+/g, ' ').trim(), url: `${url('/updates/')}#${e.id}` });
  recs.push({ title: 'Components', heading: '', text: 'Every component, drawn live by the panel renderer.', url: url('/components/') });
  recs.push({ title: 'Screens', heading: '', text: 'Every screen, live: what it shows, its settings, where its data comes from.', url: url('/screens/') });
  for (const s of SCREENS) {
    recs.push({ title: s.name, heading: '', text: plain(`Screen. ${s.about}`), url: screenHref(s) });
    if (s.params.length) recs.push({ title: s.name, heading: 'Settings', text: s.params.map((p) => p.label).join(', '), url: `${screenHref(s)}#settings` });
    if (s.sources) recs.push({ title: s.name, heading: 'Sources', text: s.sources.list.map((x) => x.name).join(', '), url: `${screenHref(s)}#sources` });
  }
  for (const f of FOUNDATIONS) {
    recs.push({ title: f.name, heading: '', text: plain(`Design system. ${f.lede}`), url: href(f) });
    (f.meta.variants ?? []).forEach((v, i) => recs.push({ title: f.name, heading: v.label, text: plain(v.about ?? ''), url: `${href(f)}#${anchor(v.label, i)}` }));
  }
  for (const c of COMPONENTS) {
    recs.push({ title: c.name, heading: '', text: plain(`Component. ${c.lede}.`), url: href(c) });
    recs.push({ title: c.name, heading: 'Variants', text: (c.meta.variants ?? []).map((v) => v.label).join(', '), url: `${href(c)}#variants` });
    if (c.meta.props?.length) recs.push({ title: c.name, heading: 'Props', text: c.meta.props.map((p) => p.name).join(', '), url: `${href(c)}#props` });
    if (c.events.length || c.sounds.length) recs.push({ title: c.name, heading: 'Events and sounds', text: [...c.events, ...c.sounds.map((s) => `${s.name} ${s.for}`)].join(', '), url: `${href(c)}#events` });
  }
  return new Response(JSON.stringify(recs), { headers: { 'content-type': 'application/json' } });
};
