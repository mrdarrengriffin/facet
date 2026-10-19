# @facet/demo-data

A fictional mock house for Facet demos, docs and tests: a study with a desk lamp, a few
speakers, weather, a three-band energy tariff, pull requests on made-up repositories, a
thunderstorm that crosses the area once an hour, agents, a timer and sample toasts.

```ts
import { createMockHouse } from '@facet/demo-data';
import { createDevice } from '@facet/ui';

const house = createMockHouse();
document.body.append(createDevice(house).el);
```

`createMockHouse()` fills a store and implements the panel actions, so playing, pausing,
changing the volume, dimming the light and starting timers all work.
