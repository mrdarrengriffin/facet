// The Facet checkout this workbench belongs to (workbench/..), and the build it shows: $FW_OUT (env or the repo's
// local.env), else renderer/out, as renderer/build.sh writes it.
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

export const REPO = process.env.FACET_REPO ?? resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const localEnv = (k: string) => {
  const p = join(REPO, 'local.env');
  return existsSync(p) ? readFileSync(p, 'utf8').match(new RegExp(`^${k}=(.*)$`, 'm'))?.[1]?.trim() : undefined;
};
export const FW_OUT = resolve(REPO, process.env.FW_OUT ?? localEnv('FW_OUT') ?? 'renderer/out');
export const FW_CACHE = resolve(REPO, process.env.FW_CACHE ?? localEnv('FW_CACHE') ?? 'renderer/.cache');
