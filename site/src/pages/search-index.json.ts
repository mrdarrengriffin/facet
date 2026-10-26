// Search index for the header search (scripts/search.ts, from the original site): one record per page and one per
// section (foundation sections, component variants / props / events / notes), all from the generated data.
import type { APIRoute } from 'astro';
import { getCollection } from 'astro:content';
import { COMPONENTS, FOUNDATIONS, SCREENS, href, screenHref, updateShown } from '../lib/data';
import { UPDATES_PAGE, url } from '../lib/nav';
import examples from '../lib/ai-examples.json';

type Rec = { title: string; heading: string; text: string; url: string };
const anchor = (label: string, i: number) => `s${i + 1}-${label.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')}`;
const plain = (s: string) => s.replace(/`|\*\*/g, '').replace(/\s+/g, ' ').trim();

export const GET: APIRoute = async () => {
  const recs: Rec[] = [];
  recs.push({ title: 'Design system', heading: '', text: 'The tokens and rules every component is built from: colour, type, space, layout, motion, states, icons, sound.', url: url('/foundations/') });
  recs.push({ title: 'Make screens with AI', heading: '', text: 'Let an AI assistant make panel screens from Facet components: specs, budgets, the panel API, Home Assistant actions and a starter prompt.', url: url('/ai/') });
  for (const e of examples) recs.push({ title: e.title, heading: 'Example screen', text: plain(e.about), url: url(`/ai/examples/${e.id}/`) });
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
