// Screen groups for the Screens index and sidebar, in the roadmap's order (Core, Home control, Media, Weather and sky,
// Info, Making, Sport). A screen the build or the specs add that this list doesn't know goes under "Other".
export const SCREEN_GROUPS: [string, string[]][] = [
  ['Core', ['home', 'clock', 'timer', 'settings', 'screens', 'screen_set', 'setup']],
  ['Home control', ['living_room', 'light']],
  ['Media', ['media']],
  ['Weather and sky', ['weather', 'weather_days', 'moon']],
  ['Info', ['calendar', 'energy']],
  ['Making', ['print']],
  ['Sport', ['football_live', 'tennis_live', 'f1_live']],
];
