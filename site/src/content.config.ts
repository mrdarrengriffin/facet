// Content collections. updates = the Updates page (Facet's timeline, newest first): one Markdown file per entry in
// src/content/updates/. Format: README in that folder.
import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { z } from 'astro/zod';

const updates = defineCollection({
  loader: glob({ pattern: '[0-9]*.md', base: './src/content/updates' }),
  schema: z.object({
    title: z.string(),
    /** YYYY-MM-DD */
    date: z.string().regex(/^\d{4}-\d{2}-\d{2}$/),
    /** HH:MM (local time), shown next to the date and orders entries of the same day; leave out when unknown */
    time: z.string().regex(/^\d{2}:\d{2}$/).optional(),
    /** what it touched: Components, Design system, Sound, Site, ... */
    area: z.string(),
    /** a turning point in Facet's story: drawn as a tinted box with a Milestone mark */
    milestone: z.boolean().default(false),
    /** pictures in public/updates/: a before / after pair, or one image */
    before: z.string().optional(),
    after: z.string().optional(),
    image: z.string().optional(),
    alt: z.string().optional(),
    /** links: [label, path under the site base, or a full https:// URL] */
    links: z.array(z.tuple([z.string(), z.string()])).default([]),
  }),
});

export const collections = { updates };
