// GitHub: how many pull requests need you (the hero) and the top three, then "All pull requests" opens the full
// list (Need you, Your PRs) and a row opens the detail view: a left column of text, a QR code to carry on on your
// phone, and the checks in two columns. Back steps out one level at a time.
import { layout as L, size as Z } from '@facet/tokens';
import { fit8, setText, statusDot } from '../atoms.js';
import { h } from '../dom.js';
import { age } from '../format.js';
import {
  at,
  CW,
  content,
  header,
  heroBlock,
  moreRow,
  page,
  row,
  subView,
  text,
} from '../layout.js';
import { qrBlock, type ChipKind } from '../lists.js';
import type { GithubData, PullRequest, Tone } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

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
export function githubSummary(
  g?: GithubData | null,
  loading = false,
): {
  status: string;
  tone: Tone;
  active: boolean;
} {
  if (!g) return { status: loading ? 'loading' : 'no data', tone: 'dim', active: false };
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

type Kind = 'needs' | 'mine';
type Status = { tone: Tone; text: string };

/** The detail view's status line: why it needs you (or its review state, for your own), draft, CI. */
const statusesFor = (p: PullRequest, kind: Kind): Status[] => {
  const out: (Status | null)[] =
    kind === 'needs'
      ? [
          { tone: 'warn', text: p.reason === 'assigned' ? 'assigned' : 'review' },
          p.reason === 'both' ? { tone: 'info', text: 'assigned' } : null,
        ]
      : [
          p.draft
            ? null
            : p.review === 'approved'
              ? { tone: 'ok', text: 'approved' }
              : p.review === 'changes'
                ? { tone: 'error', text: 'changes' }
                : { tone: 'idle', text: 'review pending' },
        ];
  out.push(
    p.draft ? { tone: 'idle', text: 'draft' } : null,
    p.ci === 'fail'
      ? { tone: 'error', text: 'ci fail' }
      : p.ci === 'pass'
        ? { tone: 'ok', text: 'ci pass' }
        : p.ci === 'pending'
          ? { tone: 'working', text: 'ci running' }
          : null,
  );
  return out.filter((x): x is Status => !!x);
};

const checkTone = (s: string): Tone =>
  s === 'pass' ? 'ok' : s === 'fail' ? 'error' : s === 'pending' ? 'working' : 'idle';

export function githubScreen(
  ctx: ScreenContext,
  o: ScreenOptions = {},
): Screen & { showList(): void; openPr(p: PullRequest, kind: Kind): void } {
  const get = () => ctx.store.get().github;
  const prRow = (p: PullRequest, kind: Kind, now: number) =>
    row({
      label: `${p.repo} · #${p.number} · ${age(p.created, now)}`,
      title: p.title,
      status: rowStatus(p, kind),
      onTap: () => showDetail(p, kind),
    });
  const empty = (t: string) => text('body', 'fs-empty', t);

  // ---- summary
  const hero = heroBlock({ label: 'need you' });
  const rows = h('div', { class: 'fs-rows' });
  const more = moreRow('all pull requests', () => show('list'));
  const sumView = subView(
    header({ label: 'GitHub', onBack: o.onBack }),
    content(at(0, 0, CW, 96, hero), at(0, 96 + L.group, CW, null, rows, more)),
  );

  // ---- list: a section label (24, a dot when it matters), 16, the rows; sections 48 apart
  const section = (t: string) => {
    const lbl = text('label', '', t);
    const el = h('div', { class: 'fs-sec' }, statusDot('warn'), lbl);
    return Object.assign(el, {
      set(v: string, accent = false) {
        lbl.textContent = v;
        el.classList.toggle('is-accent', accent);
      },
    });
  };
  const needsHd = section('need you'),
    mineHd = section('your prs');
  const needsBox = h('div', { class: 'fs-rows' }),
    mineBox = h('div', { class: 'fs-rows' });
  const listView = subView(
    header({ label: 'Pull requests', onBack: () => show('sum') }),
    content(at(0, 0, CW, null, needsHd, needsBox, mineHd, mineBox)),
  );

  // ---- detail: left column (repo line, title × 2, by line, status, body × 3), QR block on the right, checks below
  const PW = L['pr-col-w'],
    QW = Z['qr-block-w'];
  const dMeta = text('label', 'fs-d-meta');
  const dTitle = h('div', { class: 'fst-headline fs-d-title' });
  const dBy = text('body', 'fs-d-by');
  const dStatus = h('div', { class: 'fs-d-status' });
  const dBody = h('div', { class: 'fst-body fs-d-body' });
  const qr = qrBlock({ text: '', caption: 'scan to open' }); // 12 caps of mono 18 fit the 176 code width
  const ckK = text('label', 'fs-d-checks-k', 'checks'),
    ckMore = text('label', 'fs-d-more', '');
  const checks = h('div', { class: 'fs-checks' });
  const dHeader = header({ label: '#', onBack: () => show(from) });
  const detailView = subView(
    dHeader,
    content(
      at(0, 0, PW, 24, dMeta),
      at(0, 40, PW, 96, dTitle),
      at(0, 152, PW, 32, dBy),
      at(0, 200, PW, 24, dStatus),
      at(0, 248, PW, 96, dBody),
      at(CW - QW, 0, QW, null, qr),
      at(0, 376, CW, 24, h('div', { class: 'fs-d-checks-hd' }, ckK, ckMore)),
      at(0, 416, CW, 5 * L['check-row-h'], checks),
    ),
  );

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
  function showDetail(p: PullRequest, kind: Kind) {
    if (view !== 'detail') from = view === 'list' ? 'list' : 'sum';
    open = p;
    setText(dHeader.querySelector('.fs-hlabel')!, `#${p.number}`);
    dMeta.textContent = `${p.repo} · ${p.head} → ${p.base}`;
    dTitle.textContent = p.title;
    dBy.textContent = `${p.author} · ${age(p.created)} · ${p.files} files · +${p.additions} −${p.deletions}`;
    dStatus.replaceChildren(
      ...statusesFor(p, kind).map((st) =>
        h(
          'span',
          { class: 'fs-status', 'data-tone': st.tone },
          statusDot(st.tone),
          fit8(text('label', '', st.text)),
        ),
      ),
    );
    // the first three lines with text
    dBody.textContent = (p.body || 'No description.')
      .split('\n')
      .filter((l) => l.trim())
      .slice(0, 3)
      .join('\n');
    qr.set(p.url);
    const MAX = 10; // 2 columns × 5 rows
    const fails = p.checks.filter((c) => c.state === 'fail').length;
    ckK.textContent = p.checks.length
      ? `checks · ${p.checks.length}${fails ? ` · ${fails} failing` : ''}`
      : 'checks';
    // fails first, then the first MAX (or MAX - 1 and "+N more")
    const sorted = [...p.checks].sort(
      (a, b) => Number(b.state === 'fail') - Number(a.state === 'fail'),
    );
    const shown = sorted.length > MAX ? sorted.slice(0, MAX - 1) : sorted;
    ckMore.textContent = sorted.length > MAX ? `+${sorted.length - shown.length} more` : '';
    checks.replaceChildren(
      ...(shown.length
        ? shown.map((c) =>
            h(
              'div',
              { class: 'fs-check', 'data-tone': checkTone(c.state) },
              statusDot(checkTone(c.state)),
              text('body', '', c.name),
            ),
          )
        : [empty('No checks on the head commit.')]),
    );
    show('detail');
  }
  show('sum');

  let sig = '';
  function render() {
    const g: GithubData | null | undefined = get(),
      now = Date.now();
    const key = JSON.stringify([
      g?.at,
      g?.error,
      g?.needs?.length,
      g?.mine?.length,
      ctx.store.loading(),
      Math.floor(now / 60000),
    ]);
    if (key === sig) return;
    sig = key;
    if (!g?.needs) {
      hero.set({
        n: '--',
        label: g?.error ? 'github offline' : missing(ctx.store),
        line: g?.error?.slice(0, 40) ?? '',
        accent: false,
      });
      rows.replaceChildren();
      more.hidden = true;
      needsHd.set(g?.error ? 'github offline' : missing(ctx.store));
      needsBox.replaceChildren(empty(g?.error ? g.error.slice(0, 60) : 'Waiting for GitHub'));
      mineHd.hidden = true;
      mineBox.replaceChildren();
      return;
    }
    // summary
    const n = g.needs.length,
      kind: Kind = n ? 'needs' : 'mine',
      top = n ? g.needs : g.mine;
    hero.set({
      n: String(n),
      label: n ? (n === 1 ? 'needs you' : 'need you') : 'nothing needs you',
      line: n
        ? `${g.counts.review} review${g.counts.review === 1 ? '' : 's'} · ${g.counts.assigned} assigned · updated ${age(g.at, now)} ago`
        : `${g.mine.length} of your PRs open`,
      accent: n > 0,
    });
    rows.replaceChildren(...top.slice(0, 3).map((p) => prRow(p, kind, now)));
    const total = g.needs.length + g.mine.length,
      rest = total - Math.min(3, top.length);
    more.hidden = total === 0;
    more.set(rest > 0 ? `all pull requests · ${rest} more` : 'all pull requests');
    // list
    needsHd.set(n ? `need you · ${n}${g.stale ? ' · stale' : ''}` : 'nothing needs you', n > 0);
    needsBox.replaceChildren(
      ...(n
        ? g.needs.map((p) => prRow(p, 'needs', now))
        : [empty('No review requests or assignments')]),
    );
    mineHd.hidden = false;
    mineHd.set(`your prs · ${g.mine.length}`);
    mineBox.replaceChildren(
      ...(g.mine.length
        ? g.mine.map((p) => prRow(p, 'mine', now))
        : [empty('Nothing of yours is waiting')]),
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
