/**
 * Facet UI sounds: procedural (Web Audio, no samples), six named packs, and hooks that play them on real taps.
 *
 * ```ts
 * import { installSoundHooks, setEnabled, setPack, unlockOnGesture } from '@facet/sounds';
 * unlockOnGesture();
 * installSoundHooks();
 * setPack('felt'); // optional: Keycap is the default
 * setEnabled(true);
 * ```
 */
export * from './synth.js';
export * from './engine.js';
export { hasSound, installSoundHooks, SOUND_SELECTOR, type SoundHookOptions } from './hooks.js';
