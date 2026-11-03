// The release quality checklist (RELEASING.md): what a person checks on the glass of fresh panels before a tag.
// Every line is a yes or the release waits. Shown and ticked at /wasm/checklist; ticks kept by /api/wasm/checklist.
export const PANELS = [['waveshare', 'Waveshare'], ['guition', 'Guition']] as const;
export type Panel = (typeof PANELS)[number][0];
export type Line = { id: string; check: string; look: string; on: Panel[] };
export type Group = { title: string; lines: Line[] };
const both: Panel[] = ['waveshare', 'guition'];

export const GROUPS: Group[] = [
  { title: 'Install and set up', lines: [
    { id: 'install', check: 'Install from the docs site in Chrome on a panel that has never run Facet', look: 'The Install page finds the panel, writes the image, and the first boot reaches set up without help.', on: both },
    { id: 'install-test', check: 'Install from both Install and install-test on a fresh panel', look: 'Note which one works; that one becomes Install.', on: both },
    { id: 'wifi', check: 'Set up: join Wi-Fi', look: 'The panel shows its address and moves on to linking Home Assistant.', on: both },
    { id: 'link', check: 'Set up: link Home Assistant from the /link page', look: '"Linked" with the address; no token shown back.', on: both },
    { id: 'home', check: 'Land on Home with the factory deck', look: 'Every card filled with real data, no sample data.', on: both },
  ] },
  { title: 'Editor', lines: [
    { id: 'editor-load', check: "Open the panel's address in a browser", look: 'The editor loads with this panel\'s deck.', on: both },
    { id: 'editor-reorder', check: 'Reorder the deck', look: 'The new order on the glass within a few seconds.', on: both },
    { id: 'editor-entity', check: "Change one screen's entity", look: 'The new entity on the glass within a few seconds.', on: both },
    { id: 'editor-revert', check: 'Revert', look: 'The deck as it was before, on the glass.', on: both },
  ] },
  { title: 'Screens', lines: [
    { id: 'real', check: 'Every shipped screen with real data', look: 'Values match Home Assistant; nothing stale after a change in Home Assistant.', on: both },
    { id: 'not-found', check: 'Every shipped screen with its entity missing', look: '"Not found"; no crash, no stale paint.', on: both },
    { id: 'unavailable', check: 'Every shipped screen with its entity unavailable', look: '"Unavailable"; no crash, no stale paint.', on: both },
    { id: 'loading', check: 'Every shipped screen while it loads', look: 'The loading state, then the data; never a blank or a half-drawn screen.', on: both },
  ] },
  { title: 'Look', lines: [
    { id: 'dark', check: 'Dark theme', look: 'Every screen readable; no light patches left over.', on: both },
    { id: 'light', check: 'Light theme', look: 'Every screen readable; no dark patches left over.', on: both },
    { id: 'auto', check: 'Auto theme', look: 'Follows the setting it is tied to and switches without a reboot.', on: both },
    { id: 'accents', check: 'Two accents', look: 'The accent changes everywhere it is used, in both themes.', on: both },
    { id: 'clip', check: 'Text at this size', look: 'No text clipped or cut off on any screen.', on: both },
  ] },
  { title: 'Network and updates', lines: [
    { id: 'offline', check: 'Pull the network', look: 'Screens show their offline states.', on: both },
    { id: 'recover', check: 'Restore the network', look: 'Screens recover without a reboot.', on: both },
    { id: 'update', check: 'An update arrives in Home Assistant and installs', look: 'The probation chip shows, then clears.', on: both },
    { id: 'rollback', check: 'Pull power during probation', look: 'The previous image boots.', on: both },
    { id: 'soak', check: 'Leave it running 24 hours on the release candidate', look: 'No reboot; heap stable in /fw/info.', on: both },
  ] },
  { title: 'Touch and sound', lines: [
    { id: 'gestures', check: 'Tap, swipe and long press on every screen', look: 'Each does what it says; a disabled control does nothing.', on: both },
    { id: 'sound', check: 'Taps with sound on', look: 'One sound per tap, never two.', on: ['waveshare'] },
  ] },
  { title: 'Read as a stranger', lines: [
    { id: 'readme', check: 'The README', look: 'Nothing points at private tooling; every link resolves.', on: [] },
    { id: 'install-page', check: 'The Install page', look: 'Every step works as written; every picture matches the build.', on: [] },
    { id: 'contributing', check: 'The Contributing page', look: 'Every command works on a fresh checkout; every link resolves.', on: [] },
  ] },
];

// a tick's key: <line id> for a line on no panel, <line id>@<panel> otherwise
export const KEYS = new Set(GROUPS.flatMap((g) => g.lines.flatMap((l) => (l.on.length ? l.on.map((p) => `${l.id}@${p}`) : [l.id]))));
