/**
 * Docs search. The index is a JSON file built with the site (src/pages/search-index.json.ts);
 * everything else happens here, in the browser, with no external service.
 */

interface SearchRecord {
  title: string;
  heading: string;
  text: string;
  url: string;
}

interface Prepared extends SearchRecord {
  titleN: string;
  headingN: string;
  textN: string;
  words: string[];
}

interface Result {
  record: Prepared;
  score: number;
}

const MAX_RESULTS = 8;

function normalise(value: string): string {
  return value.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
}

function escapeHtml(value: string): string {
  return value.replace(/[&<>"']/g, (char) => `&#${char.charCodeAt(0)};`);
}

function prepare(record: SearchRecord): Prepared {
  const titleN = normalise(record.title);
  const headingN = normalise(record.heading);
  const textN = normalise(record.text);
  const words = [...new Set(`${titleN} ${headingN} ${textN}`.split(/[^a-z0-9]+/).filter(Boolean))];
  return { ...record, titleN, headingN, textN, words };
}

/** True when `term` starts a word somewhere in `haystack`. */
function matchesWordStart(haystack: string, term: string): boolean {
  let index = haystack.indexOf(term);
  while (index !== -1) {
    if (index === 0 || !/[a-z0-9]/.test(haystack[index - 1] ?? '')) return true;
    index = haystack.indexOf(term, index + 1);
  }
  return false;
}

/** Whether two words are at most one edit apart (insert, delete, substitute or swap). */
function withinOneEdit(a: string, b: string): boolean {
  if (Math.abs(a.length - b.length) > 1) return false;
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  if (i === a.length || i === b.length) return Math.abs(a.length - b.length) <= 1;
  const restA = a.slice(i + 1);
  const restB = b.slice(i + 1);
  return (
    restA === restB || // substitution
    a.slice(i) === b.slice(i + 1) || // insertion
    a.slice(i + 1) === b.slice(i) || // deletion
    (a[i] === b[i + 1] && a[i + 1] === b[i] && a.slice(i + 2) === b.slice(i + 2)) // swap
  );
}

function scoreTerm(record: Prepared, term: string): number {
  let score = 0;
  if (matchesWordStart(record.titleN, term)) score += 12;
  else if (record.titleN.includes(term)) score += 8;
  if (matchesWordStart(record.headingN, term)) score += 8;
  else if (record.headingN.includes(term)) score += 5;
  if (matchesWordStart(record.textN, term)) score += 3;
  else if (record.textN.includes(term)) score += 2;
  if (score > 0) return score;

  // Fuzzy fallback for typos: a word one edit away, or a word starting with a near miss.
  if (term.length >= 4) {
    for (const word of record.words) {
      if (withinOneEdit(term, word) || withinOneEdit(term, word.slice(0, term.length))) return 1.5;
    }
  }
  return 0;
}

function search(records: Prepared[], query: string): Result[] {
  const terms = normalise(query).split(/\s+/).filter(Boolean);
  if (terms.length === 0) return [];

  const results: Result[] = [];
  for (const record of records) {
    let total = 0;
    let matchedAll = true;
    for (const term of terms) {
      const score = scoreTerm(record, term);
      if (score === 0) {
        matchedAll = false;
        break;
      }
      total += score;
    }
    // Page intros rank slightly above their sections when the scores tie.
    if (matchedAll) results.push({ record, score: total + (record.heading ? 0 : 0.5) });
  }
  return results.sort((a, b) => b.score - a.score).slice(0, MAX_RESULTS);
}

/** Wrap every occurrence of the terms in <mark>, escaping everything else. */
function highlight(text: string, terms: string[]): string {
  const normalised = normalise(text);
  const ranges: [number, number][] = [];
  for (const term of terms) {
    let index = normalised.indexOf(term);
    while (index !== -1) {
      ranges.push([index, index + term.length]);
      index = normalised.indexOf(term, index + term.length);
    }
  }
  if (ranges.length === 0) return escapeHtml(text);

  ranges.sort((a, b) => a[0] - b[0]);
  let html = '';
  let cursor = 0;
  for (const [start, end] of ranges) {
    if (end <= cursor) continue;
    const from = Math.max(start, cursor);
    html +=
      escapeHtml(text.slice(cursor, from)) + `<mark>${escapeHtml(text.slice(from, end))}</mark>`;
    cursor = end;
  }
  return html + escapeHtml(text.slice(cursor));
}

/** A short excerpt around the first match. */
function excerpt(text: string, terms: string[]): string {
  const normalised = normalise(text);
  const first = Math.min(
    ...terms.map((term) => normalised.indexOf(term)).filter((index) => index !== -1),
  );
  const start = Number.isFinite(first) ? Math.max(0, first - 40) : 0;
  const end = Math.min(text.length, start + 160);
  return `${start > 0 ? '…' : ''}${text.slice(start, end).trim()}${end < text.length ? '…' : ''}`;
}

export function initSearch(): void {
  const dialog = document.querySelector<HTMLDialogElement>('[data-search]');
  const input = dialog?.querySelector<HTMLInputElement>('[data-search-input]');
  const list = dialog?.querySelector<HTMLUListElement>('[data-search-results]');
  if (!dialog || !input || !list) return;

  let records: Prepared[] | null = null;
  let loading: Promise<void> | null = null;
  let results: Result[] = [];
  let active = 0;

  const load = () => {
    loading ??= fetch(dialog.dataset.index ?? '')
      .then((response) => response.json() as Promise<SearchRecord[]>)
      .then((data) => {
        records = data.map(prepare);
        render();
      })
      .catch(() => {
        list.innerHTML = '<li class="search__empty">Search is unavailable right now.</li>';
      });
    return loading;
  };

  const setActive = (index: number) => {
    active = index;
    list.querySelectorAll<HTMLElement>('[role="option"]').forEach((option, i) => {
      const selected = i === active;
      option.setAttribute('aria-selected', String(selected));
      if (selected) {
        input.setAttribute('aria-activedescendant', option.id);
        option.scrollIntoView({ block: 'nearest' });
      }
    });
  };

  const render = () => {
    const query = input.value.trim();
    if (!records) {
      list.innerHTML = query ? '<li class="search__empty">Loading…</li>' : '';
      return;
    }
    if (!query) {
      list.innerHTML = '';
      results = [];
      return;
    }
    results = search(records, query);
    if (results.length === 0) {
      list.innerHTML = `<li class="search__empty">No results for “${escapeHtml(query)}”</li>`;
      return;
    }
    const terms = normalise(query).split(/\s+/).filter(Boolean);
    list.innerHTML = results
      .map(({ record }, i) => {
        const location = record.heading
          ? `${highlight(record.title, terms)} <span aria-hidden="true">›</span> ${highlight(record.heading, terms)}`
          : highlight(record.title, terms);
        return `<li id="search-result-${i}" role="option" aria-selected="false">
          <a href="${escapeHtml(record.url)}" tabindex="-1">
            <span class="search__title">${location}</span>
            <span class="search__text">${highlight(excerpt(record.text, terms), terms)}</span>
          </a>
        </li>`;
      })
      .join('');
    setActive(0);
  };

  const open = () => {
    if (dialog.open) return;
    dialog.showModal();
    input.select();
    void load();
  };

  const close = () => dialog.close();

  for (const button of document.querySelectorAll('[data-search-open]')) {
    button.addEventListener('click', open);
  }
  dialog.querySelector('[data-search-close]')?.addEventListener('click', close);

  // Close when clicking the backdrop (outside the panel).
  dialog.addEventListener('click', (event) => {
    if (event.target === dialog) close();
  });

  input.addEventListener('input', render);

  input.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowDown' && results.length) {
      event.preventDefault();
      setActive((active + 1) % results.length);
    } else if (event.key === 'ArrowUp' && results.length) {
      event.preventDefault();
      setActive((active - 1 + results.length) % results.length);
    } else if (event.key === 'Enter') {
      const target = results[active];
      if (target) {
        event.preventDefault();
        close();
        window.location.href = target.record.url;
      }
    }
  });

  list.addEventListener('mousemove', (event) => {
    const option = (event.target as HTMLElement).closest<HTMLElement>('[role="option"]');
    if (!option) return;
    const index = [...list.children].indexOf(option);
    if (index !== active) setActive(index);
  });

  list.addEventListener('click', (event) => {
    if ((event.target as HTMLElement).closest('a')) close();
  });

  document.addEventListener('keydown', (event) => {
    const target = event.target as HTMLElement;
    const typing =
      target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName);
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault();
      if (dialog.open) close();
      else open();
    } else if (event.key === '/' && !typing && !dialog.open) {
      event.preventDefault();
      open();
    }
  });
}
