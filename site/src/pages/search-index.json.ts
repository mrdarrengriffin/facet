import type { APIRoute } from 'astro';
import { getSortedDocs, screenHref, url } from '../lib/nav';
import { screenDocs } from '../lib/screens';
import { buildRecords } from '../lib/search-index';

// Built once at build time. The client search in src/scripts/search.ts loads it on first use.
export const GET: APIRoute = async () => {
  const docs = await getSortedDocs();
  const records = (await Promise.all(docs.map(buildRecords))).flat();
  const strip = (t: string) => t.replace(/`/g, '');
  for (const s of screenDocs) {
    const href = screenHref(s.slug);
    records.push({
      title: s.title,
      heading: '',
      text: strip(`${s.description} ${s.intro.join(' ')}`),
      url: href,
    });
    records.push({
      title: s.title,
      heading: 'What it shows',
      text: strip(s.shows.join(' ')),
      url: `${href}#what-it-shows`,
    });
    records.push({
      title: s.title,
      heading: 'Data',
      text: s.data.replace(/\s+/g, ' '),
      url: `${href}#data`,
    });
    records.push({
      title: s.title,
      heading: 'Configuration',
      text: strip(s.config.join(' ')),
      url: `${href}#configuration`,
    });
  }
  records.push(
    {
      title: 'Components',
      heading: '',
      text: 'UI explorer: every component live with controls for its props, the code to create it and its variants. Buttons, icon buttons, dial, slider, disc, cards, tiles, rows, chips, toasts, bottom sheet, charts, clocks, radar, icons, mascot.',
      url: url('/components/'),
    },
    {
      title: 'Demo',
      heading: '',
      text: 'A full Facet panel running in the browser on a mock house: navigate the screens, simulate toasts, idle clock.',
      url: url('/demo/'),
    },
    {
      title: 'Design system',
      heading: '',
      text: 'Design tokens: colour, type scales, layout, spacing, radius and motion.',
      url: url('/design-system/'),
    },
  );
  return new Response(JSON.stringify(records), {
    headers: { 'Content-Type': 'application/json' },
  });
};
