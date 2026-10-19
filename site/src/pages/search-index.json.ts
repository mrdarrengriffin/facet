import type { APIRoute } from 'astro';
import { getSortedDocs } from '../lib/nav';
import { buildRecords } from '../lib/search-index';

// Built once at build time. The client search in src/scripts/search.ts loads it on first use.
export const GET: APIRoute = async () => {
  const docs = await getSortedDocs();
  const records = (await Promise.all(docs.map(buildRecords))).flat();
  return new Response(JSON.stringify(records), {
    headers: { 'Content-Type': 'application/json' },
  });
};
