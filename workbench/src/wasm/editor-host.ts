// Facet editor host adapter: the ONLY place the editor (src/wasm/editor.ts) talks to whatever serves it. In the workbench =
// this file's `hubHost` (/wasm/editor). Served FROM a panel (plain HTTP, Guition first) = editor-host-device.ts, swapped
// in for this module by the device bundle (scripts/editor-device.mjs): the panel's own /fw/* routes, Save sends the deck.
// Keep editor.ts free of fetch() and paths: anything host-specific goes here (HA states / actions too: `ha`).
import { hubHa, type HaBridge } from './engine';
import type { DeviceId } from './engine';

/** one entity option for a picker */
export type Option = { id: string; name: string };
/** the deck a panel runs (fw_spec deck v2, core/spec.h; notes screen-model.md): its pages in order, the start page,
 *  each Home's cards. No on / off: a page the panel doesn't have is left out */
export type DeckCard = { t: string; e?: string; size: 's' | 'm' | 'l'; tap?: string; page?: string; label?: string };
export type DeckPage = { id: string; kind: string; name?: string; source?: string; params?: Record<string, string>; cards?: DeckCard[] };
export type Deck = { v: 2; rev: number; start: string; pages: DeckPage[] };
/** the editor's row for a page (not a Home): on = in the deck (an off row stays listed in the editor, left out of what is sent) */
export type DeckEntry = DeckPage & { on?: boolean };
/** what the host knows about this house: per screen what each source found and the params each found source fills,
 *  entity options per domain, and the deck to start from */
export type Discovery = {
  deck: Deck;
  /** screens sent to this panel (the API, an agent; GET /fw/spec "sent") that aren't built-in kinds */
  sent?: { id: string; name: string; role?: string }[];
  options: Record<string, Option[]>;
  screens: Record<string, {
    source?: string | null;
    found: Record<string, { found: boolean; evidence: string }>;
    /** per found source: param -> value it fills (entity lists comma-joined, one entity) */
    fills?: Record<string, Record<string, string>>;
  }>;
};
export type PanelStatus = { online: boolean; ip?: string };
export type SaveResult = { sent: boolean; note: string };

export interface EditorHost {
  /** the panels this host can edit (the device toggle); a panel serving the editor = only itself */
  devices: DeviceId[];
  /** true = Save really sends (a failed send keeps the edits unsaved); false = a concept that only shows the deck */
  live: boolean;
  /** Save dialog's confirm button */
  saveLabel: string;
  /** HA states for bound components + their actions (engine HaBridge); null states = sample data */
  ha: HaBridge;
  /** where the renderer's files live for a density ('1x' | '1.5x'): manifest.json, fw.mjs, fw.wasm */
  wasmBase(den: string): string;
  /** this house's sources + options + the deck to edit; null = none (the engine's factory deck) */
  discover(): Promise<Discovery | null>;
  /** is the panel being edited reachable */
  status(device: string): Promise<PanelStatus>;
  /** send the deck to the panel (applied in one step, the last one kept to roll back) */
  save(device: string, deck: Deck): Promise<SaveResult>;
  /** remove a sent screen from the panel (POST /fw/spec {kind: revert, id}); absent = this host can't */
  removeSent?(device: string, id: string): Promise<boolean>;
  /** the line under Save's change list: what Save does on this host */
  saveNote: string;
  /** the deck being edited, after every change (before the engine builds it): a host whose HA can't unsubscribe (the
   *  panel) keeps its live values to the entities this deck names, not the sample screens' demo ones */
  deckChanged?(deck: Deck): void;
}

export const hubHost: EditorHost = {
  devices: ['guition', 'waveshare'],
  live: false,
  saveLabel: 'Mark as saved',
  ha: hubHa,
  wasmBase: (den) => `/facet-wasm/${den}/`,
  async discover() {
    try { return await (await fetch('/api/wasm/sources', { cache: 'no-store' })).json(); } catch { return null; }
  },
  async status(device) {
    try {
      const r = await (await fetch(`/api/wasm/show?device=${device}`, { cache: 'no-store' })).json();
      return { online: !!r.online, ip: r.ip };
    } catch { return { online: false }; }
  },
  async save() { return { sent: false, note: 'Concept: nothing is sent.' }; },
  saveNote: 'Concept: nothing is sent. On the panel this applies the deck in one step and keeps the last one to roll back.',
};

export const host: EditorHost = hubHost;
