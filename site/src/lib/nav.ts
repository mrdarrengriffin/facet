import { getCollection, type CollectionEntry } from 'astro:content';

import { groups } from './groups';
import { screenDocs } from './screens';

export { groups };

export interface NavLink {
  label: string;
  href: string;
  badge?: string;
  /** Screen icon name, shown before the label. */
  icon?: string;
}

export interface NavGroup {
  label: string;
  links: NavLink[];
}

/** Prefix a site path with the base path. `url('/docs/')` → `/facet/docs/`. */
export function url(path: string): string {
  const base = import.meta.env.BASE_URL.replace(/\/$/, '');
  return `${base}${path.startsWith('/') ? path : `/${path}`}`;
}

export function docHref(entry: CollectionEntry<'docs'>): string {
  return url(`/docs/${entry.id}/`);
}

export const screenHref = (slug: string) => url(`/docs/screens/${slug}/`);

/** Docs pages written as Astro pages (they run code live), added to a docs group after its Markdown pages. */
const extraDocs: Record<string, NavLink[]> = {
  Concepts: [
    { label: 'Sounds', href: url('/docs/sounds/') },
    { label: 'Settings', href: url('/docs/settings/') },
  ],
};

/** Pages outside the docs collection that also appear in the sidebar. */
const explore: NavGroup = {
  label: 'Explore',
  links: [
    { label: 'Components', href: url('/components/') },
    { label: 'Demo', href: url('/demo/') },
    { label: 'Design system', href: url('/design-system/') },
    { label: 'Icons', href: url('/design-system/icons/') },
    { label: 'Install', href: url('/install/'), badge: 'Soon' },
  ],
};

const screens: NavGroup = {
  label: 'Screens',
  links: screenDocs.map((s) => ({ label: s.title, href: screenHref(s.slug), icon: s.icon })),
};

export async function getSortedDocs(): Promise<CollectionEntry<'docs'>[]> {
  const docs = await getCollection('docs');
  return docs.sort((a, b) => {
    const group = groups.indexOf(a.data.group) - groups.indexOf(b.data.group);
    if (group !== 0) return group;
    return a.data.order - b.data.order || a.data.title.localeCompare(b.data.title);
  });
}

export async function getSidebar(): Promise<NavGroup[]> {
  const docs = await getSortedDocs();
  const byGroup = (label: string): NavGroup => ({
    label,
    links: docs
      .filter((doc) => doc.data.group === label)
      .map((doc): NavLink => ({ label: doc.data.title, href: docHref(doc) }))
      .concat(extraDocs[label] ?? []),
  });
  return [byGroup('Start here'), byGroup('Concepts'), screens, explore, byGroup('Project')].filter(
    (g) => g.links.length > 0,
  );
}

/** Every page in reading order, for the previous / next links at the bottom of a page. */
export async function getReadingOrder(): Promise<{ label: string; href: string }[]> {
  const sidebar = await getSidebar();
  return sidebar.filter((g) => g.label !== 'Explore').flatMap((g) => g.links);
}
