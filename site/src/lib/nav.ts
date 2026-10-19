import { getCollection, type CollectionEntry } from 'astro:content';

import { groups } from './groups';

export { groups };

export interface NavLink {
  label: string;
  href: string;
  badge?: string;
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

/** Pages outside the docs collection that also appear in the sidebar. */
const explore: NavGroup = {
  label: 'Explore',
  links: [
    { label: 'Design system', href: url('/design-system/') },
    { label: 'Demo', href: url('/demo/'), badge: 'Soon' },
    { label: 'Install', href: url('/install/'), badge: 'Soon' },
  ],
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
  const sidebar: NavGroup[] = groups.map((label) => ({
    label,
    links: docs
      .filter((doc) => doc.data.group === label)
      .map((doc) => ({ label: doc.data.title, href: docHref(doc) })),
  }));
  // Explore sits after the concepts, before the project pages.
  sidebar.splice(2, 0, explore);
  return sidebar.filter((group) => group.links.length > 0);
}
