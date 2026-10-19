# @facet/ui

Facet components and screens, written in plain TypeScript. No UI framework: each component is
a function that returns a DOM element, and components that change have a `set()` method.

```ts
import '@facet/tokens/tokens.css';
import '@facet/icons/weather.css';
import '@facet/ui/styles.css';
import { button, createDevice, createStore } from '@facet/ui';

document.body.append(button({ kind: 'primary', text: 'Start', onTap: () => start() }));
```

Screens read a store (`createStore`) and call actions, so they work the same on a mock house,
in a test or on real data:

```ts
const store = createStore({ room: 'Kitchen', light: { on: true, brightness: 60 } });
const device = createDevice({ store, actions: { lightBrightness: (pct) => setLight(pct) } });
document.body.append(device.el);
```

- `createDevice()`: every screen, navigation, toasts and the idle clock.
- `mountScreen(id, ctx)`: one screen in its own frame, without a back button.
- `homeScreen()`, `mediaScreen()`, `lightScreen()`, ...: the screens on their own.

The data shapes are exported as types (`PanelData`, `MediaState`, `WeatherData`, ...). Try
everything in the [UI explorer](https://mrdarrengriffin.github.io/facet/components/).
