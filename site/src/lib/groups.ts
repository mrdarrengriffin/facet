/** Sidebar groups for the docs collection, in display order. */
export const groups = ['Start here', 'Concepts', 'Project'] as const;
export type Group = (typeof groups)[number];
