// Component groups for the components sidebar, in the original site's explorer order (main: site/src/scripts/
// explorer/registry.ts). A component the build registers but this list doesn't know goes under "Other".
export const COMPONENT_GROUPS: [string, string[]][] = [
  ['Atoms', ['label', 'status_dot', 'status_chip', 'corner_ticks', 'focus_ring']],
  ['Buttons', ['button', 'icon_button', 'back_button']],
  ['Controls', ['slider', 'dial', 'toggle', 'segmented']],
  ['Screen parts', ['header', 'card', 'tile', 'hero_block', 'list_row', 'more_row', 'stat']],
  ['Lists', ['count_badge', 'section_header']],
  ['Pickers', ['radio', 'picker_row', 'speaker_button', 'bottom_sheet', 'accent_swatches']],
  ['Toasts', ['toast', 'toast_layer']],
];
