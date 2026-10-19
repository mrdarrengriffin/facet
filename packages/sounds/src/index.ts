/**
 * Facet UI sounds: procedural (Web Audio, no samples), versioned sets, and hooks that play them on real taps.
 *
 * ```ts
 * import { installSoundHooks, setEnabled, unlockOnGesture } from '@facet/sounds';
 * unlockOnGesture();
 * installSoundHooks();
 * setEnabled(true);
 * ```
 */
export * from './synth.js';
export * from './engine.js';
export { installSoundHooks, type SoundHookOptions } from './hooks.js';
