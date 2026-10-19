// Media visuals for the Media screen: a cassette, a CD, a vinyl LP or a reel-to-reel deck in place of the disc, drawn
// to the real sizes (specs.ts) and turning at the real tape and disc speeds while the track plays.
import { h } from '../dom.js';
import { mediaPos } from '../screens/common.js';
import type { MediaState } from '../types.js';
import { MAKERS, animate, type Feed, type Visual, type VisualId } from './visuals.js';

export type { Feed as MediaVisualFeed } from './visuals.js';
export { CASSETTE, CD, LP, REEL, ARM, PLACE as MEDIA_VISUAL_SLOT } from './specs.js';

/** What sits in the Media screen's hero slot. `disc` is the plain disc with the progress arc. */
export type MediaVisualId = 'disc' | VisualId;
export const MEDIA_VISUALS: { id: MediaVisualId; name: string; what: string }[] = [
  { id: 'disc', name: 'Disc', what: 'The disc with the progress arc.' },
  {
    id: 'cassette',
    name: 'Cassette',
    what: 'A C60 Compact Cassette, cut off by the left edge of the screen. The spools turn at the real tape speed and the tape moves from one to the other as the track plays.',
  },
  {
    id: 'cd',
    name: 'CD',
    what: 'A CD with the cover art printed on it. It spins at the real speed, slowed down 16 times.',
  },
  {
    id: 'vinyl',
    name: 'Vinyl',
    what: 'A 12" LP with the cover art on the label, at 33⅓ rpm. The tonearm moves across the record as the track plays.',
  },
  {
    id: 'reel',
    name: 'Reel-to-reel',
    what: 'Two 7" reels with the tape running over the heads, at half the real speed. The tape moves from one reel to the other as the track plays.',
  },
];
export const isMediaVisual = (v: unknown): v is MediaVisualId =>
  MEDIA_VISUALS.some((x) => x.id === v);

// ---------------------------------------------------------------- palette from the cover art
/** Without art (or until it has loaded): token colours. */
const FALLBACK = [
  'var(--facet-color-accent)',
  'var(--facet-color-blue)',
  'var(--facet-color-warn)',
  'var(--facet-color-steel)',
];
const palettes = new Map<string, string[] | 'loading'>();

/**
 * Four colours from the art: a 24 × 24 sample in 4-bit buckets, sorted by how much of the cover they cover (weighted
 * towards saturated colours), each one clearly different from the others. Async: returns the fallback until the
 * art has loaded, then calls `done`.
 */
export function artPalette(url: string | null | undefined, done?: () => void): string[] {
  if (!url) return FALLBACK;
  const p = palettes.get(url);
  if (Array.isArray(p)) return p;
  if (p === 'loading') return FALLBACK;
  palettes.set(url, 'loading');
  const img = new Image();
  if (!url.startsWith('data:')) img.crossOrigin = 'anonymous';
  const finish = (cols: string[]) => {
    palettes.set(url, cols);
    done?.();
  };
  img.onload = () => {
    try {
      const N = 24,
        cv = document.createElement('canvas');
      cv.width = cv.height = N;
      const g = cv.getContext('2d', { willReadFrequently: true })!;
      g.drawImage(img, 0, 0, N, N);
      const px = g.getImageData(0, 0, N, N).data;
      const bins = new Map<number, { n: number; r: number; g: number; b: number }>();
      for (let i = 0; i < px.length; i += 4) {
        const r = px[i]!,
          gg = px[i + 1]!,
          b = px[i + 2]!;
        const key = ((r >> 4) << 8) | ((gg >> 4) << 4) | (b >> 4);
        const bin = bins.get(key) ?? { n: 0, r: 0, g: 0, b: 0 };
        bin.n++;
        bin.r += r;
        bin.g += gg;
        bin.b += b;
        bins.set(key, bin);
      }
      const cols = [...bins.values()]
        .map((b) => {
          const r = b.r / b.n,
            gg = b.g / b.n,
            bb = b.b / b.n,
            mx = Math.max(r, gg, bb),
            mn = Math.min(r, gg, bb);
          return { r, g: gg, b: bb, score: b.n * (0.35 + (mx ? (mx - mn) / mx : 0)) };
        })
        .sort((a, b) => b.score - a.score);
      const out: typeof cols = [];
      for (const c of cols) {
        if (out.every((o) => Math.abs(o.r - c.r) + Math.abs(o.g - c.g) + Math.abs(o.b - c.b) > 90))
          out.push(c);
        if (out.length === 4) break;
      }
      const hex = out.map((c) => `rgb(${c.r | 0} ${c.g | 0} ${c.b | 0})`);
      finish([...hex, ...FALLBACK].slice(0, 4));
    } catch {
      finish(FALLBACK); // a cover from another origin without CORS can't be read
    }
  };
  img.onerror = () => finish(FALLBACK);
  img.src = url;
  return FALLBACK;
}

/** The feed for a visual from the store's media. Nothing playing = still, empty, at the start. */
export function mediaFeed(
  m: MediaState | null | undefined,
  refresh?: () => void,
  now = Date.now(),
): Feed {
  const on = !!m && (m.state === 'playing' || m.state === 'paused');
  const art = (on && m?.art) || '';
  return {
    progress: on ? mediaPos(m, now).frac : 0,
    playing: m?.state === 'playing',
    art,
    title: (on && m?.title) || '',
    artist: (on && m?.artist) || '',
    palette: artPalette(art, refresh),
  };
}

// ---------------------------------------------------------------- one visual on its own
export interface MediaVisualProps {
  visual: VisualId;
  /** 0-100. */
  progress?: number;
  playing?: boolean;
  title?: string;
  artist?: string;
  /** Cover art URL or data URI. */
  art?: string | null;
}
export type MediaVisualElement = HTMLElement & { set(p: Partial<MediaVisualProps>): void };

/**
 * One visual on its own, whole (not cut off by the screen edge), in a box its own size. For docs and catalogues; the
 * Media screen uses `mediaVisualSlot`.
 */
export function mediaVisual(p: MediaVisualProps): MediaVisualElement {
  const v = animate(MAKERS[p.visual]());
  v.el.style.left = '0px';
  v.el.style.top = '0px';
  const box = h(
    'div',
    {
      class: 'f-mv-box',
      'data-visual': p.visual,
      style: { width: `${Math.ceil(v.box.w)}px`, height: `${Math.ceil(v.box.h)}px` },
    },
    v.el,
  );
  let cur: MediaVisualProps = { ...p };
  const push = () =>
    v.feed({
      progress: Math.min(100, Math.max(0, cur.progress ?? 0)) / 100,
      playing: !!cur.playing,
      art: cur.art || '',
      title: cur.title ?? '',
      artist: cur.artist ?? '',
      palette: artPalette(cur.art, push),
    });
  const el: MediaVisualElement = Object.assign(box, {
    set(n: Partial<MediaVisualProps>) {
      cur = { ...cur, ...n };
      push();
    },
  });
  push();
  return el;
}

// ---------------------------------------------------------------- the Media screen's slot
/**
 * The Media screen's hero slot. For `disc` it keeps the disc; for any other visual it hides the disc and shows the
 * visual in a layer over the whole page (the visual runs off the screen's left edge, so it can't live in the content
 * box). Put `layer` first in the page so the header and controls sit on top.
 */
export function mediaVisualSlot(disc: HTMLElement, visual: MediaVisualId = 'disc') {
  const layer = h('div', { class: 'f-mv-layer', 'aria-hidden': 'true' });
  const made: Partial<Record<VisualId, Visual>> = {};
  let media: MediaState | null | undefined,
    cur: MediaVisualId | null = null;
  const vis = () => (visual === 'disc' ? null : (made[visual] ??= animate(MAKERS[visual]())));
  const push = () => vis()?.feed(mediaFeed(media, push));
  const show = () => {
    if (cur === visual) return;
    cur = visual;
    disc.style.visibility = visual === 'disc' ? '' : 'hidden';
    const v = vis();
    layer.replaceChildren(...(v ? [v.el] : []));
    layer.dataset.visual = visual;
    push();
  };
  show();
  return {
    layer,
    get visual() {
      return visual;
    },
    /** Switch to another visual. */
    setVisual(id: MediaVisualId) {
      if (!isMediaVisual(id)) return;
      visual = id;
      show();
    },
    /** New media from the store. */
    set(m: MediaState | null | undefined) {
      media = m;
      push();
    },
    /** Once a second: progress moves on. */
    tick: () => push(),
    /** The screen was shown again: back into the animation loop. */
    enter() {
      const v = vis();
      if (v) animate(v);
    },
  };
}
