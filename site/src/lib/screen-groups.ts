// Screen groups for the Screens index and sidebar, in the roadmap's order (Core, Home control, Media, Weather and sky,
// Info). A screen the build or the specs add that this list doesn't know goes under "Other".
export const SCREEN_GROUPS: [string, string[]][] = [
  ['Core', ['home', 'clock', 'timer']],
  ['Home control', ['light']],
  ['Media', ['media']],
  ['Weather and sky', ['weather', 'weather_days', 'moon', 'overhead']],
  ['Info', ['calendar', 'energy']],
];
