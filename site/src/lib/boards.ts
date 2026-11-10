// The supported panels: boards/<id>.json, read at build time (status target = a release firmware is built for it),
// largest screen first. One source for Install, Panels and the home page.
export type Board = {
  id: string; name: string; chip: string; radio: string | null; bluetooth: boolean; resolution: { w: number; h: number };
  inches: number; scale: string; touch: string; flash: string; speaker: boolean; status: string; memory?: { keep_pages: number; keep_floor_kb?: number };
  performance?: { tap_to_frame_ms?: [number, number]; render_ms?: [number, number]; boot_to_kept_s?: number; psram_free_mb_kept?: number; about?: string };
  links?: { label: string; url: string; maker: boolean }[];
};
export const BOARDS = Object.values(import.meta.glob<Board>('../../../boards/*.json', { eager: true, import: 'default' }))
  .filter((b) => b.status === 'target')
  .sort((a, b) => b.resolution.w - a.resolution.w || a.id.localeCompare(b.id));
/** "4 inch, 720 x 720" */
export const screenOf = (b: Board) => `${b.inches} inch, ${b.resolution.w} x ${b.resolution.h}`;
/** how Wi-Fi can be set: the ways the firmware offers on this board */
export const wifiWays = (b: Board) => (b.bluetooth ? 'USB, Bluetooth or the panel hotspot' : 'USB or the panel hotspot (no Bluetooth on this board)');
