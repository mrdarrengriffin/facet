// The agent guide a panel serves at /agents.md (tools/gen_agents.mjs; the docs site serves the docs build's copy): its
// example screen spec is what an agent copies first, so it must pass the panel's own check (POST /fw/spec kind check),
// with no unknown prop (it named a List row "label" once, refused as unknown_prop).
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const GEN = fileURLToPath(new URL('../../tools/gen_agents.mjs', import.meta.url));
export default {
  id: 'agents_guide', family: 'layout', about: 'The agent guide\'s example spec passes the panel\'s check.',
  tests: [
    { name: 'the guide\'s example spec passes the check', den: '1.5x', async run(t) {
      const md = execFileSync('node', [GEN, '--flavour', 'docs', '--print', 'agents.md'], { encoding: 'utf8' });
      const ex = md.match(/## Screen spec\n```json\n([\s\S]*?)```/)?.[1];
      t.ok(!!ex, 'the guide has a Screen spec example');
      if (!ex) return;
      await t.mount('label', { text: 'x' });
      const r = JSON.parse(t.b.spec('check', ex) || '{}');
      t.eq(r.errors ?? r.status, [], 'example checked');
    } },
  ],
};
