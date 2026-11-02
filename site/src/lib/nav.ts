// Site navigation: the header's top line (what a buyer needs), the docs sidebar groups (Use Facet, Screens, Help,
// Inside Facet), the footer's Inside Facet links and the reading order for previous / next. Screens come from the
// generated data; a page still to come shows as "Soon".
import { BASE, screenGroups, screenHref } from './data';

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
 *  On: What's next, then the release notes (src/content/updates/, drafts hidden). */
export const UPDATES_PAGE = true;
// The AI example screens (/ai/examples/<id>/, the grid on /ai/, search records): false hides them.
export const AI_EXAMPLES = true;

// the top line: what a buyer needs. Inside Facet (how it is built) is in the sidebar and the footer, not here.
export const headerLinks = [
  { label: 'Screens', href: url('/screens/'), match: [url('/screens/')] },
  { label: 'Panels', href: url('/panels/'), match: [url('/panels/')] },
  { label: 'Editor', href: url('/editor/'), match: [url('/editor/')] },
  { label: 'Install', href: url('/install/'), match: [url('/install/')] },
  { label: 'Help', href: url('/help/'), match: [url('/help/'), url('/contributing/')] },
  ...(UPDATES_PAGE ? [{ label: 'Updates', href: url('/updates/'), match: [url('/updates/')] }] : []),
];

/** Inside Facet: how it is built (the sidebar group and the footer) */
export const insideLinks: NavLink[] = [
  { label: 'How Facet is built', href: url('/inside/') },
  { label: 'Tokens', href: url('/inside/tokens/') },
  { label: 'Foundations', href: url('/foundations/') },
  { label: 'Components', href: url('/inside/components/') },
  { label: 'Component library', href: url('/components/') },
  { label: 'Spec format', href: url('/inside/spec/') },
  { label: 'Schema', href: url('/inside/schema/') },
  { label: 'Make screens with AI', href: url('/ai/') },
];

// a page still to come: soon('Label') = a 'Soon' row, no link
export const soon = (label: string): NavLink => ({ label, badge: 'Soon' });
// the docs sidebar lists are alphabetical (Overview stays first)
const byLabel = (a: NavLink, b: NavLink) => a.label.localeCompare(b.label);

export function getSidebar(): NavGroup[] {
  return [
    {
      label: 'Use Facet',
      links: [
        { label: 'Install', href: url('/install/') },
        { label: 'Panels', href: url('/panels/') },
        { label: 'Editor', href: url('/editor/') },
        ...(UPDATES_PAGE ? [{ label: 'Updates', href: url('/updates/') }] : []),
      ],
    },
    {
      label: 'Screens',
      links: [{ label: 'Overview', href: url('/screens/') }, ...screenGroups().flatMap((g) => g.items).map((s) => ({ label: s.name, href: screenHref(s) })).sort(byLabel)],
    },
    {
      label: 'Help',
      links: [{ label: 'FAQ', href: url('/help/') }, { label: 'Troubleshooting', href: url('/help/troubleshooting/') }, { label: 'Contributing', href: url('/contributing/') }],
    },
    { label: 'Inside Facet', links: insideLinks },
  ];
}

/** Every docs page in reading order, for the previous / next links. */
export function getReadingOrder(): { label: string; href: string }[] {
  return getSidebar().flatMap((g) => g.links.filter((l): l is NavLink & { href: string } => !!l.href).map((l) => ({ label: l.label === 'Overview' ? 'Screens' : l.label, href: l.href })));
}
