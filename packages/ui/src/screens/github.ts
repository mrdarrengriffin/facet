// GitHub: how many pull requests need you (the hero) and the top three, then "All pull requests" opens the full
// list (Needs you, Your PRs) and a row opens the detail view with a QR code to carry on on your phone. Back steps out
// one level at a time.
import { frame as L, size as Z } from '@facet/tokens';
import { setText, statusChip } from '../atoms.js';
import { h } from '../dom.js';
import { age } from '../format.js';
import {
  at,
  CW,
  content,
  header,
  heroBlock,
  layout,
  moreRow,
  page,
  row,
  subView,
} from '../layout.js';
import {
  checkRow,
  emptyState,
  prRow,
  qrBlock,
  sectionHeader,
  stateChip,
  type ChipKind,
} from '../lists.js';
import type { GithubData, PullRequest, Tone } from '../types.js';
import type { Screen, ScreenContext, ScreenOptions } from './common.js';

const ciChip = (ci: PullRequest['ci']): ChipKind | null =>
  ci === 'pass' ? 'ci-pass' : ci === 'fail' ? 'ci-fail' : ci === 'pending' ? 'ci-pending' : null;

/** The chips for a pull request: why it needs you (or its review state, for your own), draft, CI. */
export function chipsFor(p: PullRequest, kind: 'needs' | 'mine'): ChipKind[] {
  const out: (ChipKind | null)[] =
    kind === 'needs'
      ? [
          p.reason === 'assigned' ? 'assigned' : 'review',
          p.reason === 'both' ? 'assigned' : null,
          p.draft ? 'draft' : null,
          ciChip(p.ci),
        ]
      : [p.draft ? 'draft' : p.review, ciChip(p.ci)];
  return out.filter((x): x is ChipKind => !!x);
}

/** One line for a Home card: "3 to review · 2 open". */
export function githubSummary(g?: GithubData | null): {
  status: string;
  tone: Tone;
  active: boolean;
} {
  if (!g) return { status: 'no data', tone: 'dim', active: false };
  if (!g.needs && g.error) return { status: 'offline', tone: 'error', active: false };
  const n = g.counts.needs;
  return {
    status: [n ? `${n} to review` : 'nothing to review', `${g.counts.mine} open`].join(' · '),
    tone: n ? 'warn' : 'dim',
    active: n > 0,
  };
}

const rowStatus = (p: PullRequest, kind: 'needs' | 'mine'): { tone: Tone; text: string } | null =>
  p.ci === 'fail'
    ? { tone: 'error', text: 'ci fail' }
    : kind === 'mine' && p.review === 'approved'
      ? { tone: 'ok', text: 'approved' }
      : kind === 'mine' && p.review === 'changes'
        ? { tone: 'error', text: 'changes' }
        : p.draft
          ? { tone: 'idle', text: 'draft' }
          : kind === 'needs'
            ? { tone: 'warn', text: p.reason === 'assigned' ? 'assigned' : 'review' }
            : null;

export function githubScreen(
  ctx: ScreenContext,
  o: ScreenOptions = {},
): Screen & { showList(): void; openPr(p: PullRequest, kind: 'needs' | 'mine'): void } {
  const GUT = L.gutter;
  const get = () => ctx.store.get().github;

  // ---- summary
  const hero = heroBlock({ label: 'need you' });
  const rows = h('div', { class: 'fs-rows' });
  const more = moreRow('all pull requests', () => show('list'));
  const sumView = subView(
    header({ label: 'GitHub', onBack: o.onBack }),
    content(at(0, 0, CW, 96, hero), at(0, 96 + layout.group, CW, null, rows, more)),
  );

  // ---- list
  const needsHd = sectionHeader({ title: 'Needs you', count: 0, tone: 'warn' });
  const mineHd = sectionHeader({ title: 'Your PRs', count: 0, tone: 'info' });
  const needsBox = h('div', { class: 'f-stack', style: { gap: `${GUT}px` } });
  const mineBox = h('div', { class: 'f-stack', style: { gap: `${GUT}px` } });
  const list = h(
    'div',
    {
      class: 'f-abs f-content f-scroll f-stack',
      style: {
        left: `${L.inset}px`,
        top: `${L.top}px`,
        width: `${L.width}px`,
        height: `${L.height}px`,
        gap: `${GUT}px`,
      },
    },
    needsHd,
    needsBox,
    mineHd,
    mineBox,
  );
  const cReview = statusChip('warn', '0 to review');
  const listView = subView(
    header({ label: 'Pull requests', onBack: () => show('sum'), right: [cReview] }),
    list,
  );

  // ---- detail
  const W = 464;
  const dMeta = h('div', {
    class: 'f-abs ft-meta f-dmeta',
    style: { left: `${L.inset}px`, top: `${L.top}px`, width: `${W}px` },
  });
  const dTitle = h('div', {
    class: 'f-abs ft-title f-clamp f-clamp--title',
    style: { left: `${L.inset}px`, top: `${L.top + 24}px`, width: `${W}px` },
  });
  const dBy = h('div', {
    class: 'f-abs ft-meta f-dmeta',
    style: { left: `${L.inset}px`, top: `${L.top + 112}px`, width: `${W}px` },
  });
  const dChips = h('div', {
    class: 'f-abs f-chips',
    style: { left: `${L.inset}px`, top: `${L.top + 136}px`, width: `${W}px`, marginTop: '0' },
  });
  const dBody = h('div', {
    class: 'f-abs ft-body f-dbody',
    style: { left: `${L.inset}px`, top: `${L.top + 184}px`, width: `${W}px` },
  });
  const qr = qrBlock({ text: '', caption: 'open on your phone' });
  qr.classList.add('f-abs');
  qr.style.left = `${L.right - Z['qr-block-w']}px`;
  qr.style.top = `${L.top}px`;
  const checksHd = sectionHeader({ title: 'Checks', count: 0, tone: 'ok' });
  const checks = h('div', { class: 'f-checks' });
  const checksBox = h(
    'div',
    {
      class: 'f-abs',
      style: { left: `${L.inset}px`, top: `${L.top + 320}px`, width: `${L.width}px` },
    },
    checksHd,
    checks,
  );
  const dHeader = header({ label: '#', onBack: () => show(from), right: [] });
  const detailView = subView(dHeader, dMeta, dTitle, dBy, dChips, dBody, qr, checksBox);

  const el = page('fs-github', sumView, listView, detailView);
  let view: 'sum' | 'list' | 'detail' = 'sum';
  let from: 'sum' | 'list' = 'sum';
  let open: PullRequest | null = null;
  function show(v: typeof view) {
    if (v !== 'detail') open = null;
    view = v;
    sumView.hidden = v !== 'sum';
    listView.hidden = v !== 'list';
    detailView.hidden = v !== 'detail';
  }
  function showDetail(p: PullRequest, kind: 'needs' | 'mine') {
    if (view !== 'detail') from = view === 'list' ? 'list' : 'sum';
    open = p;
    setText(dHeader.querySelector('.fs-hlabel')!, `#${p.number}`);
    dMeta.textContent = `${p.repoFull} · ${p.head} → ${p.base}`;
    dTitle.textContent = p.title;
    dBy.textContent = `${p.author} · ${age(p.created)} · ${p.files} files · +${p.additions} −${p.deletions}`;
    dChips.replaceChildren(...chipsFor(p, kind).map(stateChip));
    dBody.textContent = p.body || 'No description.';
    qr.set(p.url);
    const MAX = 14; // 2 columns × 7 rows
    const fails = p.checks.filter((c) => c.state === 'fail').length;
    checksHd.set({
      count: p.checks.length,
      tone: fails ? 'error' : p.ci === 'pending' ? 'working' : 'ok',
      right: p.checks.length > MAX ? `+${p.checks.length - MAX + 1} more` : '',
    });
    const shown = p.checks.length > MAX ? p.checks.slice(0, MAX - 1) : p.checks;
    checks.replaceChildren(
      ...(shown.length
        ? shown.map((c) => checkRow(c.name, c.state))
        : [h('span', { class: 'ft-meta f-dmeta' }, 'No checks on the head commit.')]),
    );
    show('detail');
  }
  show('sum');

  let sig = '';
  function render() {
    const g = get(),
      now = Date.now();
    const key = JSON.stringify([
      g?.at,
      g?.error,
      g?.needs?.length,
      g?.mine?.length,
      Math.floor(now / 60000),
    ]);
    if (key === sig) return;
    sig = key;
    // summary
    if (!g?.needs) {
      hero.set({
        n: '--',
        label: g?.error ? 'github offline' : 'no data',
        line: g?.error?.slice(0, 40) ?? '',
        accent: false,
      });
      rows.replaceChildren();
      more.hidden = true;
    } else {
      const n = g.needs.length,
        kind: 'needs' | 'mine' = n ? 'needs' : 'mine',
        top = n ? g.needs : g.mine;
      hero.set({
        n: String(n),
        label: n ? (n === 1 ? 'needs you' : 'need you') : 'nothing needs you',
        line: n
          ? `${g.counts.review} review${g.counts.review === 1 ? '' : 's'} · ${g.counts.assigned} assigned · updated ${age(g.at, now)} ago`
          : `${g.mine.length} of your PRs open`,
        accent: n > 0,
      });
      rows.replaceChildren(
        ...top.slice(0, 3).map((p) =>
          row({
            label: `${p.repo} · #${p.number} · ${age(p.created, now)}`,
            title: p.title,
            status: rowStatus(p, kind),
            onTap: () => showDetail(p, kind),
          }),
        ),
      );
      const total = g.needs.length + g.mine.length,
        rest = total - Math.min(3, top.length);
      more.hidden = total === 0;
      more.set(rest > 0 ? `all pull requests · ${rest} more` : 'all pull requests');
    }
    // list
    setText(cReview.lastElementChild!, g?.counts ? `${g.counts.needs} to review` : '--');
    if (!g?.needs) {
      needsHd.set({ count: 0 });
      mineHd.set({ count: 0 });
      needsBox.replaceChildren(
        g?.error
          ? emptyState({
              icon: 'alert',
              tone: 'error',
              title: 'GitHub unavailable',
              body: g.error.slice(0, 60),
            })
          : emptyState({
              icon: 'pr',
              tone: 'idle',
              title: 'No data yet',
              body: 'Waiting for GitHub',
            }),
      );
      mineBox.replaceChildren();
      return;
    }
    needsHd.set({
      count: g.needs.length,
      tone: g.needs.length ? 'warn' : 'ok',
      right: g.stale ? 'stale' : `updated ${age(g.at, now)}`,
    });
    mineHd.set({ count: g.mine.length, tone: 'info' });
    needsBox.replaceChildren(
      ...(g.needs.length
        ? g.needs.map((p) =>
            prRow(
              {
                repo: p.repo,
                number: p.number,
                title: p.title,
                age: age(p.created, now),
                chips: chipsFor(p, 'needs'),
              },
              () => showDetail(p, 'needs'),
            ),
          )
        : [
            emptyState({
              icon: 'done',
              tone: 'ok',
              title: 'Nothing needs you',
              body: 'No review requests or assignments',
            }),
          ]),
    );
    mineBox.replaceChildren(
      ...(g.mine.length
        ? g.mine.map((p) =>
            prRow(
              {
                repo: p.repo,
                number: p.number,
                title: p.title,
                age: age(p.created, now),
                chips: chipsFor(p, 'mine'),
              },
              () => showDetail(p, 'mine'),
            ),
          )
        : [
            emptyState({
              icon: 'pr',
              tone: 'idle',
              title: 'No open PRs',
              body: 'Nothing of yours is waiting',
            }),
          ]),
    );
    if (open) {
      const p = [...g.needs, ...g.mine].find((x) => x.id === open!.id);
      if (p) showDetail(p, g.needs.includes(p) ? 'needs' : 'mine');
    }
  }
  return {
    el,
    update: render,
    tick: render,
    enter() {
      ctx.actions?.refreshGithub?.();
    },
    leave() {
      show('sum');
    },
    showList: () => show('list'),
    openPr: showDetail,
  };
}
