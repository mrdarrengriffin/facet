// Theme editor (theme engine T4, facet-wasm renderer/CONTRACT.md §10): a theme file edited live on the workbench.
// The JSON goes through the SAME parser + guards the panel runs (fw_lab theme_load / theme_contrast / theme_apply,
// core/theme_file.cpp): 300 ms after a change it is loaded; accepted = made live (every contract component restyles,
// no rebuild); refused = the live look stays as it was and every error is listed with its path (click = its line).
// Guard panel: the parser's checks, the contrast rows (every mode x accent set, RGB565: pass / fail, failures listed
// with where they are), the file size against the panel's 16 KB budget and the load's peak memory against its budget
// (core/theme_slot.h, theme_slot_status). Load = a .json from disk, Save = download, Base = start from a built-in.
// Workbench only (the test bench): a panel gets a theme from Home Assistant (facet_theme) and checks it itself.
type Call = (cmd: string, arg?: string) => string;
export interface ThemeEditorHost {
  call: Call;              // fw_lab
  builtins: () => string[];
  text: () => string;      // the file to start from (the custom draft, else the live built-in)
  applied: (json: string) => void;  // a file was accepted and made live (the host keeps it + redraws)
}
interface Err { path: string; msg: string; line: number }
type Row = [string, string, string, string, number, number, number, number, number, boolean];
const h = (tag: string, cls = '', text = '') => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
const parse = <T>(s: string, d: T): T => { try { return JSON.parse(s) as T; } catch { return d; } };

let open: HTMLElement | null = null;
export function themeEditor(host: ThemeEditorHost) {
  if (open) { open.remove(); open = null; return; }
  const box = h('section', 'te');
  box.setAttribute('aria-label', 'Theme editor');
  const head = h('div', 'te-head');
  const title = h('div', 'te-title', 'Theme editor');
  const base = h('select', 'te-base') as HTMLSelectElement;
  base.title = 'Start from a built-in theme';
  base.append(new Option('Start from…', ''), ...host.builtins().map((id) => new Option(id, id)));
  const load = h('button', 'te-btn', 'Load') as HTMLButtonElement, save = h('button', 'te-btn', 'Save') as HTMLButtonElement, close = h('button', 'te-btn', 'Close') as HTMLButtonElement;
  close.setAttribute('aria-label', 'Close the theme editor');
  const file = h('input') as HTMLInputElement; file.type = 'file'; file.accept = '.json,application/json'; file.hidden = true;
  head.append(title, base, load, save, close, file);
  const status = h('div', 'te-status'); status.setAttribute('role', 'status');
  const edit = h('div', 'te-edit'), gutter = h('pre', 'te-gutter'), ta = h('textarea', 'te-text') as HTMLTextAreaElement;
  ta.spellcheck = false; ta.setAttribute('aria-label', 'Theme file (JSON)'); ta.value = host.text();
  edit.append(gutter, ta);
  const errs = h('ul', 'te-errs'), guard = h('div', 'te-guard');
  box.append(head, status, edit, errs, guard);
  document.body.append(box);
  open = box;

  const lines = () => { const n = ta.value.split('\n').length; gutter.textContent = Array.from({ length: n }, (_, i) => String(i + 1)).join('\n'); gutter.scrollTop = ta.scrollTop; };
  ta.addEventListener('scroll', () => { gutter.scrollTop = ta.scrollTop; });
  const jump = (line: number) => {
    const L = ta.value.split('\n'); let at = 0;
    for (let i = 0; i < Math.max(0, line - 1) && i < L.length; i++) at += L[i].length + 1;
    ta.focus(); ta.setSelectionRange(at, at + (L[line - 1]?.length ?? 0));
    ta.scrollTop = Math.max(0, (line - 4) * parseFloat(getComputedStyle(ta).lineHeight || '18'));
  };
  const check = () => {
    lines();
    const json = ta.value, bytes = new TextEncoder().encode(json).length;
    const r = parse<{ ok: boolean; id?: string; checks?: number; peak?: number; errors?: Err[] }>(host.call('theme_load', json), { ok: false, errors: [{ path: '', msg: 'the workbench could not run the parser', line: 0 }] });
    const lim = parse<{ max_bytes: number; peak_max: number }>(host.call('theme_slot_status'), { max_bytes: 16384, peak_max: 98304 });
    errs.replaceChildren(...(r.errors ?? []).map((e) => {
      const li = h('li'), b = h('button', 'te-err') as HTMLButtonElement;
      b.textContent = `${e.line ? `line ${e.line} · ` : ''}${e.path ? `${e.path}: ` : ''}${e.msg}`;
      b.dataset.path = e.path; if (e.line) b.addEventListener('click', () => jump(e.line)); else b.disabled = true;
      li.append(b); return li;
    }));
    // guard panel: the same code path the panel runs (theme_contrast = the rows theme_load refuses on)
    const c = parse<{ ok: boolean; rows?: Row[] }>(host.call('theme_contrast', json), { ok: false });
    const rows = c.rows ?? [], fails = rows.filter((x) => !x[9]);
    const kinds = new Map<string, [number, number]>();
    for (const x of rows) { const k = kinds.get(x[3]) ?? [0, 0]; k[x[9] ? 0 : 1]++; kinds.set(x[3], k); }
    const tile = (label: string, value: string, bad: boolean) => { const t = h('div', `te-tile${bad ? ' bad' : ''}`); t.append(h('span', 'dot'), h('b', '', value), h('span', '', label)); return t; };
    const tiles = h('div', 'te-tiles');
    tiles.append(
      tile('structure + ranges', r.ok ? `${r.checks ?? 0} checks` : `${r.errors?.length ?? 0} refused`, !r.ok),
      tile('file size', `${(bytes / 1024).toFixed(1)} / ${lim.max_bytes / 1024} KB`, bytes > lim.max_bytes),
      ...(r.ok ? [tile('load memory', `${Math.round((r.peak ?? 0) / 1024)} / ${Math.round(lim.peak_max / 1024)} KB`, (r.peak ?? 0) > lim.peak_max)] : []),
      ...[...kinds].map(([k, [p, f]]) => tile(`contrast · ${k}`, `${p} pass${f ? `, ${f} fail` : ''}`, f > 0)),
    );
    const fl = h('ul', 'te-fails');
    for (const x of fails.slice(0, 40)) fl.append(h('li', '', `${x[0]} · ${x[1]} on ${x[2]} · ${x[4] ? 'light' : 'dark'} set ${x[5]} · ${Math.min(x[6], x[7]).toFixed(2)} < ${x[8]}`));
    if (fails.length > 40) fl.append(h('li', '', `… ${fails.length - 40} more`));
    guard.replaceChildren(h('div', 'te-sub', 'Guards (the same checks a panel runs)'), tiles, ...(fails.length ? [fl] : []));
    if (r.ok) {
      const a = parse<{ ok: boolean; id?: string }>(host.call('theme_apply', json), { ok: false });
      status.className = `te-status ${a.ok ? 'ok' : 'bad'}`;
      status.replaceChildren(h('span', 'dot'), document.createTextNode(a.ok ? `Live: ${r.id} (a panel would accept this file)` : 'Refused on a panel'));
      if (a.ok) host.applied(json);
    } else {
      status.className = 'te-status bad';
      status.replaceChildren(h('span', 'dot'), document.createTextNode(`Refused on a panel: ${r.errors?.length ?? 0} error${r.errors?.length === 1 ? '' : 's'}; the live look is unchanged`));
    }
  };
  let timer = 0;
  ta.addEventListener('input', () => { lines(); clearTimeout(timer); timer = window.setTimeout(check, 300); });
  base.addEventListener('change', () => { if (!base.value) return; const b = parse<{ ok: boolean; json?: string }>(host.call('theme_slot_builtin', base.value), { ok: false }); if (b.ok && b.json) { ta.value = b.json; check(); } base.value = ''; });
  load.addEventListener('click', () => file.click());
  file.addEventListener('change', async () => { const f = file.files?.[0]; if (!f) return; ta.value = await f.text(); file.value = ''; check(); });
  save.addEventListener('click', () => {
    const id = parse<{ id?: string }>(ta.value, {}).id || 'theme';
    const a = h('a') as HTMLAnchorElement; a.href = URL.createObjectURL(new Blob([ta.value], { type: 'application/json' })); a.download = `${id}.json`; a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  });
  close.addEventListener('click', () => { box.remove(); open = null; });
  check();
}
export const themeEditorOpen = () => !!open;
