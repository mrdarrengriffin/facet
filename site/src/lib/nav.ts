// Site navigation, in the original Facet site's shape (main a5df2ac: site/src/lib/nav.ts + Header.astro): header links,
// the docs sidebar groups and the reading order for previous / next. Foundations (the "Design system") and
// components come from the WASM registry; pages not written yet show as "Soon" (getting started, concepts).
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

export const headerLinks = [
  { label: 'Docs', href: url('/roadmap/'), match: [url('/roadmap/'), url('/screens/')] },
  { label: 'Components', href: url('/components/'), match: [url('/components/')] },
  { label: 'Design system', href: url('/foundations/'), match: [url('/foundations/')] },
  ...(UPDATES_PAGE ? [{ label: 'Updates', href: url('/updates/'), match: [url('/updates/')] }] : []),
];

const soon = (label: string): NavLink => ({ label, badge: 'Soon' });
// the docs sidebar lists are alphabetical (Overview stays first)
const byLabel = (a: NavLink, b: NavLink) => a.label.localeCompare(b.label);

export function getSidebar(): NavGroup[] {
  return [
    { label: 'Start here', links: [soon('Getting started'), { label: 'Install', href: url('/install/') }] },
    { label: 'Concepts', links: [soon('Design tokens'), soon('Components'), soon('Firmware')] },
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
