import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { z } from 'astro/zod';
import { groups } from './lib/groups';

const docs = defineCollection({
  loader: glob({ pattern: '**/*.{md,mdx}', base: './src/content/docs' }),
  schema: z.object({
    title: z.string(),
    description: z.string(),
    /** Sidebar group. Groups are ordered in src/lib/nav.ts. */
    group: z.enum(groups),
    /** Position within the group, lowest first. */
    order: z.number().default(100),
  }),
});

export const collections = { docs };
