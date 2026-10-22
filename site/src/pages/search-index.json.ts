// Search index for the header search (scripts/search.ts, from the original site): one record per page and one per
// section (foundation sections, component variants / props / events / notes), all from the generated data.
import type { APIRoute } from 'astro';
import { getCollection } from 'astro:content';
import { COMPONENTS, FOUNDATIONS, href } from '../lib/data';
import { url } from '../lib/nav';

type Rec = { title: string; heading: string; text: string; url: string };
const anchor = (label: string, i: number) => `s${i + 1}-${label.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')}`;
const plain = (s: string) => s.replace(/`|\*\*/g, '').replace(/\s+/g, ' ').trim();

export const GET: APIRoute = async () => {
  const recs: Rec[] = [];
  recs.push({ title: 'Design system', heading: '', text: 'The tokens and rules every component is built from: colour, type, space, layout, motion, states, icons, sound.', url: url('/foundations/') });
  recs.push({ title: 'Roadmap', heading: '', text: 'Where Facet is headed, area by area: available, in progress, planned.', url: url('/roadmap/') });
  recs.push({ title: 'Updates', heading: '', text: 'How Facet came to be and what changed lately, newest first.', url: url('/updates/') });
  for (const e of await getCollection('updates')) recs.push({ title: 'Updates', heading: e.data.title, text: (e.body ?? '').replace(/[>*]/g, '').replace(/\s+/g, ' ').trim(), url: `${url('/updates/')}#${e.id}` });
  recs.push({ title: 'Components', heading: '', text: 'Every component, drawn live by the panel renderer.', url: url('/components/') });
  recs.push({ title: 'Screens', heading: '', text: 'Screens come after the components.', url: url('/screens/') });
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
