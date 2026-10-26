// Site navigation, in the original Facet site's shape (main a5df2ac: site/src/lib/nav.ts + Header.astro): header links,
// the docs sidebar groups and the reading order for previous / next. Foundations (the "Design system") and
// components come from the WASM registry; pages not written yet show as "Soon" (concepts).
import { BASE, FOUNDATIONS, href, screenGroups, screenHref } from './data';

export interface NavLink {
  label: string;
  /** no href = a page still to come ("Soon" badge, not a link) */
  href?: string;
  badge?: string;
  /** WASM icon name, shown before the label */
  icon?: string;
}

export interface NavGroup {
  label: string;
  links: NavLink[];
}

/** Prefix a site path with the base path. `url('/components/')` -> `/facet-docs/components/`. */
export function url(path: string): string {
  return `${BASE.replace(/\/$/, '')}${path.startsWith('/') ? path : `/${path}`}`;
}

/** the Updates page (src/pages/[updates].astro) and every link to it (header, sidebar, search, homepage, roadmap).
 *  Off for now (Darren 2026-10-25): the entries stay in src/content/updates/ so the page can come back by setting this. */
export const UPDATES_PAGE = false;
// The AI example screens (/ai/examples/<id>/, the grid on /ai/, search records): hidden until the smaller set is redone.
export const AI_EXAMPLES = true;

export const headerLinks = [
  { label: 'Docs', href: url('/docs/'), match: [url('/docs/'), url('/install/'), url('/contributing/'), url('/roadmap/'), url('/screens/'), url('/ai/')] },
  { label: 'Components', href: url('/components/'), match: [url('/components/')] },
  { label: 'Design system', href: url('/foundations/'), match: [url('/foundations/')] },
  ...(UPDATES_PAGE ? [{ label: 'Updates', href: url('/updates/'), match: [url('/updates/')] }] : []),
];

// a page still to come: soon('Label') = a 'Soon' row, no link
export const soon = (label: string): NavLink => ({ label, badge: 'Soon' });
// the docs sidebar lists are alphabetical (Overview stays first)
const byLabel = (a: NavLink, b: NavLink) => a.label.localeCompare(b.label);

export function getSidebar(): NavGroup[] {
  return [
    { label: 'Start here', links: [{ label: 'Getting started', href: url('/docs/') }, { label: 'Install', href: url('/install/') }, { label: 'Make screens with AI', href: url('/ai/') }, { label: 'Contributing', href: url('/contributing/') }] },
    {
      label: 'Using Facet',
      links: [{ label: 'Home cards', href: url('/docs/home-cards/') }, { label: 'Updating and troubleshooting', href: url('/docs/troubleshooting/') }, { label: 'FAQ and hardware', href: url('/docs/faq/') }],
    },
    {
      label: 'Concepts',
      links: [{ label: 'Components', href: url('/docs/concepts/components/') }, { label: 'Design tokens', href: url('/docs/concepts/design-tokens/') }, { label: 'How the firmware works', href: url('/docs/concepts/firmware/') }],
    },
    {
      label: 'Design system',
      links: [{ label: 'Overview', href: url('/foundations/') }, ...FOUNDATIONS.map((f) => ({ label: f.name, href: href(f) })).sort(byLabel)],
    },
    {
      label: 'Screens',
      links: [{ label: 'Overview', href: url('/screens/') }, ...screenGroups().flatMap((g) => g.items).map((s) => ({ label: s.name, href: screenHref(s) })).sort(byLabel)],
    },
    {
      label: 'Project',
      links: [
        { label: 'Roadmap', href: url('/roadmap/') },
        ...(UPDATES_PAGE ? [{ label: 'Updates', href: url('/updates/') }] : []),
      ],
    },
  ];
}

/** Every docs page in reading order, for the previous / next links. */
export function getReadingOrder(): { label: string; href: string }[] {
  return getSidebar().flatMap((g) => g.links.filter((l): l is NavLink & { href: string } => !!l.href).map((l) => ({ label: l.label === 'Overview' ? (l.href === url('/screens/') ? 'Screens' : 'Design system') : l.label, href: l.href })));
}
