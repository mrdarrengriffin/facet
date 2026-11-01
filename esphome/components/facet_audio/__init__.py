# ESPHome external component `facet_audio` (packages/audio_waveshare-p4-86.yaml): the Facet UI sounds on the Waveshare
# ESP32-P4-86 speaker (ES8311 DAC -> NS4150B amp -> speaker). The sounds = facet_sounds.bin, rendered from
# renderer/tokens/sound.json by firmware/sound_bank.mjs (generated, not committed).
# ESPHome configures the codec (audio_dac es8311, 24 kHz) and reserves the I2S port (i2s_audio); this component drives
# the port itself (TX only) from its own task. YAML:
#   facet_audio:
#     amp_pin: 53          # NS4150B enable (active high), driven by the audio task: on while sounds play / touches
#     amp_idle: 30s        # amp off after this long with no sound and no touch (no hiss); a touch turns it back on
import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = []
DEPENDENCIES = ["i2s_audio"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("amp_pin", default=53): cv.int_range(min=0, max=63),
        cv.Optional("amp_idle", default="30s"): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/facet_audio/facet_audio.h"'))
    cg.add_define("FACET_AUDIO_AMP_PIN", config["amp_pin"])
    cg.add_define("FACET_AUDIO_AMP_IDLE_MS", config["amp_idle"].total_milliseconds)
