# @facet/sounds

Facet's UI sounds: procedural, made with Web Audio (no samples), in six named packs. Every
pack has the same ten sounds (Tap, Open, Back, Toggle on, Toggle off, Slider tick, Confirm,
Timer done, Error, Notify).

| Pack       | Engine | Character                                                      |
| ---------- | ------ | -------------------------------------------------------------- |
| `keycap`   | basic  | Default. Short tactile clicks, like a mechanical key or a dial |
| `halo`     | basic  | Sine tones with bell partials: calm and rounded                |
| `arcade`   | basic  | Square and pulse waves, stepped arpeggios: retro-digital       |
| `graphite` | modal  | Dry, precise thocks; chimes are a dry glass marimba            |
| `crystal`  | modal  | Soft crystalline tones with feather-light strikes              |
| `felt`     | modal  | Warm wooden bodies struck through felt; chimes are a marimba   |

The default comes from the tokens (`sound.pack`).

```ts
import { installSoundHooks, play, setEnabled, setPack, unlockOnGesture } from '@facet/sounds';

unlockOnGesture(); // browsers only allow audio after a gesture
installSoundHooks(); // taps, toggles, back, slider ticks on .f-screen elements
setPack('felt'); // optional: Keycap is the default
setEnabled(true); // sounds are off until you turn them on

play('confirm'); // from the current pack
play('notify', { pack: 'crystal', always: true }); // any pack, even with sounds off
```

Sounds play only on real taps: a press that moved or scrolled stays silent. The same rendered
buffer can be exported as a WAV (`render(soundParams(pack, id), 16000)` + `wav()`) for panel
firmware. All packs and their parameters live in `src/packs.json`.

Docs with every sound playable: https://mrdarrengriffin.github.io/facet/docs/sounds/
