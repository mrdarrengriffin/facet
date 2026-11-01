// Facet UI sounds on the Waveshare ESP32-P4-86 speaker. The renderer decides WHICH sound plays
// and WHEN (fw::sound -> the host's sink, tokens/sound.json types); this plays it, in the user's pack, at the user's
// volume, with the same samples as the workbench and the docs (facet_sounds_gen.h: every type x pack, rendered by the
// Facet synth at 24 kHz, stored in flash).
//   - UI task (sink, LVGL context): ui_sound() only checks the settings and puts a request in a lock-free queue (no
//     locks, no allocation, no I/O): never blocks the UI
//   - audio task (own FreeRTOS task, priority 19): up to VOICES sounds mixed (int32, saturating), pitched sounds
//     resampled (linear interpolation), 64-frame I2S blocks (2.7 ms) x 3 DMA descriptors. Between sounds it sleeps on
//     a task notification; the DMA keeps sending zeros (auto_clear), so a new sound lands in the next free block:
//     request -> DAC about 3-8 ms (+ the codec's ~1 ms)
//   - volume rule = the workbench / docs engine: a master gain = Volume / 100 on samples loudness-aligned by the
//     synth (sound.json levels); the codec sits at a fixed level (audio_waveshare.yaml sound_dac_level)
//   - pitch rule = the workbench: sounds with track > 0 (slider-tick, slider-landmark) shift by sound.json pitch
//     low..high semitones over the control's range (the sound's detail "0.420" = its position 0..1)
//   - amp (NS4150B, FACET_AUDIO_AMP_PIN): switched on by a touch (LVGL indev PRESSED, before the tapped object hears
//     it) or a sound, off after FACET_AUDIO_AMP_IDLE_MS with neither (no idle hiss)
#pragma once
#include <cstdint>

namespace facet_audio {
// once from on_boot, after the codec's setup (i2s_audio port from id(i2s_bus).get_port())
void setup(int i2s_port);
// a UI sound request from the renderer's sink: id = sound.json type ("tap"), detail = "" or the position "0.420";
// sounds = the UI sounds setting ("true" / "false"), pack = the Sound pack setting (pack id), volume = 0..100 text.
// Any of the settings may be nullptr (= default: on, keycap, 80).
void ui_sound(const char *id, const char *detail, const char *sounds, const char *pack, const char *volume);
// play by index (tests, HA): pack / type indices of facet_sounds_gen.h, pos < 0 = not pitched, gain 0..1
void play(int pack, int type, float pos, float gain);
// a touch: amp on now (and kept on for FACET_AUDIO_AMP_IDLE_MS)
void wake();
// one-line state for logs / diagnostics
const char *status();
}  // namespace facet_audio
