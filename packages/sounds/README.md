# @facet/sounds

Facet's UI sounds: procedural, made with Web Audio (no samples), in versioned sets. The set in
use comes from the tokens (`sound.chosen`, v1 · Crisp & mechanical).

```ts
import { installSoundHooks, setEnabled, unlockOnGesture } from '@facet/sounds';

unlockOnGesture(); // browsers only allow audio after a gesture
installSoundHooks(); // taps, toggles, back, slider ticks on .f-screen elements
setEnabled(true); // sounds are off until you turn them on
```

Sounds play only on real taps: a press that moved or scrolled stays silent. The same rendered
buffer can be exported as a WAV (`render()` + `wav()`) for panel firmware.
