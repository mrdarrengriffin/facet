// Facet editor v1's host: the ONLY place editor-v1.ts talks to whatever serves it. This file = the workbench
// (/wasm/editor-v1): nothing is sent, "the panel" is the page's memory, entity pickers list the test fixture, Home
// Assistant is a stub. A panel's bundle (scripts/editor-device.mjs) swaps in editor-v1-host-device.ts: its /fw/* routes.
import type { DeviceId, HaBridge } from './engine';

export type Opt = { id: string; name: string };
/** the Home Assistant link: off (no address + token), set, checking, linked, failed; hint = the token's last four */
export type HaLink = { url: string; hint: string; state: string; why?: string };
/** what the panel runs: its deck (fw_spec deck v2), its settings' values, its Home Assistant link */
export type PanelState = { deck: { rev: number; start: string; pages: any[] }; settings: Record<string, unknown>; ha: HaLink };
export type Err = { path: string; code: string; msg: string };
export type SaveOut = { ok: boolean; errors?: Err[]; why?: string };
/** the pickers' lists: null = this host has none of its own (the workbench: the fixture) */
export type Entities = { state: 'ok' | 'loading' | 'off' | 'failed'; why?: string; lists: Record<string, Opt[]> } | null;

export interface V1Host {
  /** true = served by a panel: Save sends to it, and it is the only panel */
  panel: boolean;
  devices: DeviceId[];
  wasmBase(den: string): string;
  /** live values for the previews (a panel: its own Home Assistant link) */
  ha: HaBridge;
  /** what the panel runs now; null = none (the workbench starts from the factory deck file) */
  load(): Promise<PanelState | null>;
  entities(domains: string[]): Promise<Entities>;
  /** Save: the deck, the settings that changed, the Home Assistant link when it changed (token "" = keep the one set) */
  save(deck: object, settings: Record<string, unknown>, ha: { url: string; token: string } | null): Promise<SaveOut>;
  /** Back to factory (kind revert, the deck) */
  factory(): Promise<SaveOut>;
  /** the deck being edited after each change: a panel keeps its live values to the entities it names */
  deckChanged?(deck: object): void;
}

const noHa: HaBridge = { states: async () => null, action: async () => undefined, ask: async () => null };
export const host: V1Host = {
  panel: false,
  devices: ['waveshare', 'guition'],
  wasmBase: (den) => `/facet-wasm/${den}/`,
  ha: noHa,
  load: async () => null,
  entities: async () => null,
  save: async () => ({ ok: true }),
  factory: async () => ({ ok: true }),
};
