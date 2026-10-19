// 3D print: the model in an isometric view, filling up from the bed as the layers go. The printed part is solid, the
// last layers printed wear the accent, the rest is a ghost. Hero = the printer's own % (it matches the printer's app);
// the fill cuts at the current layer, so the picture is what is on the plate. Under it: status and finish time, the
// job name, the layer and the time left.
// Content 656 x 576: the model 656 x 408 at the top, then (one group, 48) the hero 96 on the left and three lines on
// the right. The model is one image: on a device that can't draw it, a server renders the frames with the same code.
import { h } from '../dom.js';
import { CW, at, content, header, page, text } from '../layout.js';
import { layerTop } from '../print3d/grid.js';
import { palette, render } from '../print3d/render.js';
import type { PrintData } from '../types.js';
import { missing, type Screen, type ScreenContext, type ScreenOptions } from './common.js';

export interface PrintOptions extends ScreenOptions {
  /** Shading: one tone per face direction (default) or smooth bands. */
  shade?: 'faces' | 'smooth';
  /** The solid part's colour: neutral (default), steel or the filament colour. */
  solid?: 'neutral' | 'steel' | 'filament';
  /** How many layers below the cut wear the accent (default 4). */
  hot?: number;
}

const MW = CW,
  MH = 408;
const WORD: Record<PrintData['status'], string> = {
  printing: 'printing',
  paused: 'paused',
  preparing: 'preparing',
  done: 'done',
  failed: 'failed',
  idle: 'idle',
};

/** "1 h 05 min", "12 min". */
export const printLeft = (min: number) =>
  min >= 60 ? `${Math.floor(min / 60)} h ${String(min % 60).padStart(2, '0')} min` : `${min} min`;

export function printScreen(ctx: ScreenContext, o: PrintOptions = {}): Screen {
  const cv = h('canvas', { class: 'fs-print-model', width: MW, height: MH }) as HTMLCanvasElement;
  const g2 = cv.getContext('2d');
  const hero = text('hero', 'fs-print-hero', '--');
  const lbl = text('label', 'fs-print-label', '');
  const title = text('title', 'fs-print-title', '');
  const body = text('body', 'fs-print-body', '');
  const el = page(
    'fs-print',
    header({ label: '3D print', onBack: o.onBack }),
    content(
      at(0, 0, MW, MH, cv),
      at(0, 456, 288, 96, hero),
      at(304, 456, CW - 304, 24, lbl),
      at(304, 480, CW - 304, 40, title),
      at(304, 520, CW - 304, 32, body),
    ),
  );

  let p: PrintData | null | undefined;
  let lastKey = '',
    busy = false,
    dirty = true;
  const theme = (): 'dark' | 'light' => {
    const t = el.closest('[data-facet-theme]')?.getAttribute('data-facet-theme');
    if (t === 'light' || t === 'dark') return t;
    return matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
  };

  function draw() {
    if (busy || !dirty || !g2) return;
    dirty = false;
    const g = p?.model;
    if (!p || !g) {
      g2.clearRect(0, 0, MW, MH);
      lastKey = '';
      return;
    }
    const active = p.status === 'printing' || p.status === 'paused' || p.status === 'preparing';
    const layer = active ? p.layer : g.layerZ.length;
    const solid = o.solid === 'filament' ? (p.filament ?? 'neutral') : (o.solid ?? 'neutral');
    const key = [g.name, g.nx, g.nz, layer, theme(), solid].join('|');
    if (key === lastKey) return;
    busy = true;
    requestAnimationFrame(() => {
      const px = render(g, {
        w: MW,
        h: MH,
        cutZ: layerTop(g, layer),
        pal: palette(theme(), solid),
        shade: o.shade ?? 'faces',
        hotSlices: o.hot ?? 4,
      });
      const img = new ImageData(MW, MH);
      img.data.set(px);
      g2.putImageData(img, 0, 0);
      lastKey = key;
      busy = false;
      if (dirty) draw();
    });
  }

  function words() {
    if (!p) {
      hero.textContent = '--';
      lbl.textContent = missing(ctx.store);
      title.textContent = body.textContent = '';
      return;
    }
    const active = p.status === 'printing' || p.status === 'paused' || p.status === 'preparing';
    const word = p.status === 'preparing' && p.stage ? p.stage : WORD[p.status];
    const end = p.endTs
      ? new Date(p.endTs * 1000).toLocaleTimeString('en-GB', {
          hour: '2-digit',
          minute: '2-digit',
          hour12: false,
        })
      : '';
    hero.textContent = active
      ? `${Math.round(p.progress)}%`
      : p.status === 'done'
        ? 'Done'
        : WORD[p.status];
    hero.classList.toggle('is-word', !active);
    lbl.textContent = active && end ? `${word} · ends ${end}` : word;
    title.textContent = p.task ?? '';
    body.textContent = active
      ? `layer ${p.layer} of ${p.layers}${p.remainingMin != null ? ` · ${printLeft(p.remainingMin)} left` : ''}`
      : p.layers
        ? `${p.layers} layers`
        : '';
    el.classList.toggle('is-paused', p.status === 'paused');
  }

  const redraw = () => {
    dirty = true;
    draw();
  };
  const themes = new MutationObserver(redraw);
  themes.observe(document.documentElement, {
    subtree: true,
    attributes: true,
    attributeFilter: ['data-facet-theme'],
  });
  matchMedia('(prefers-color-scheme: light)').addEventListener('change', redraw);

  return {
    el,
    update(s) {
      p = s.print;
      words();
      redraw();
    },
    enter: redraw,
  };
}
