// Tap areas view (sidebar Settings "Pixel grid" > Tap areas, localStorage wb.pixgrid = taps; the editor lab's "Tap areas"
// toggle): every tap target on a stage (core fw_taps) drawn over the preview: the hit area filled (green = at least
// touch:min both ways, red = smaller), the drawn box dashed inside it, and the size. Overlay canvas, any zoom, polled
// while the page lives (like outline.ts).
import type { Stage, TapBox } from './engine';

const OK = '#22c55e', SMALL = '#ef4444';
export const tapSmall = (t: TapBox) => t.w < t.min || t.h < t.min;
export function tapSummary(list: TapBox[]) {
  const small = list.filter(tapSmall).length;
  return `${list.length} tap targets${small ? `, ${small} smaller than the touch minimum (${list[0].min} px)` : ', all at least the touch minimum'}`;
}
/** draw the targets on canvas cv laid over a w x h stage drawn at k px per device px */
export function drawTaps(g: CanvasRenderingContext2D, list: TapBox[], k: number, dpr: number) {
  const lw = Math.max(1, Math.round(dpr)), fs = Math.round(11 * dpr);
  g.font = `600 ${fs}px ui-monospace, monospace`;
  g.textBaseline = 'top';
  for (const t of list) {
    const col = tapSmall(t) ? SMALL : OK;
    const x = Math.round(t.x * k), y = Math.round(t.y * k), w = Math.round(t.w * k), h = Math.round(t.h * k);
    g.globalAlpha = 0.16; g.fillStyle = col; g.fillRect(x, y, w, h);
    g.globalAlpha = 1; g.strokeStyle = col; g.lineWidth = lw; g.strokeRect(x + lw / 2, y + lw / 2, w - lw, h - lw);
    if (t.w !== t.bw || t.h !== t.bh) {  // the drawn box inside a bigger hit area
      g.setLineDash([3 * dpr, 3 * dpr]);
      g.strokeRect(Math.round(t.bx * k) + lw / 2, Math.round(t.by * k) + lw / 2, Math.round(t.bw * k) - lw, Math.round(t.bh * k) - lw);
      g.setLineDash([]);
    }
    const label = `${t.w}×${t.h}`, tw = g.measureText(label).width + 6 * dpr;
    if (w >= tw && h >= fs + 4 * dpr) {
      g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(x + lw, y + lw, tw, fs + 4 * dpr);
      g.fillStyle = col; g.fillText(label, x + lw + 3 * dpr, y + lw + 2 * dpr);
    }
  }
}
/** the workbench overlay: wraps the stage canvas like outlineView */
export function tapView(s: Stage, onSummary: (t: string) => void) {
  const box = document.createElement('div'), cv = document.createElement('canvas');
  box.className = 'wb-ol';
  s.canvas.replaceWith(box);
  box.append(s.canvas, cv);
  const g = cv.getContext('2d')!;
  let list: TapBox[] = [], key = '', last = 0;
  const tick = (t: number) => {
    if (!box.isConnected && last) return;
    requestAnimationFrame(tick);
    if (!last || t - last > 150) {
      last = t || 1;
      list = s.taps();
      const nk = JSON.stringify(list);
      if (nk !== key) { key = nk; onSummary(tapSummary(list)); }
    }
    const r = s.canvas.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
    const W = Math.max(1, Math.round(r.width * dpr)), H = Math.max(1, Math.round(r.height * dpr));
    if (cv.width !== W || cv.height !== H) { cv.width = W; cv.height = H; }
    g.clearRect(0, 0, W, H);
    drawTaps(g, list, W / s.w, dpr);
  };
  requestAnimationFrame(tick);
  return box;
}
