# ESPHome external component `facet_wb` (firmware/bundle.mjs copies this as __init__.py next to the bundled sources):
# the Facet WASM workbench core + foundations + components on a real panel. YAML:
#   facet_wb:
#     board: guition         # id the workbench uses for this device
#     width: 480             # screen size in device px
#     height: 480
#     probation: 3min        # optional, = safe_mode boot_is_good_after (on-screen countdown after an OTA)
# Needs lvgl (one display) and web_server (port 80): GET /fw/info, GET /fw/meta, POST /fw/show (firmware/fw_device.h).
import esphome.codegen as cg
import esphome.config_validation as cv

DEPENDENCIES = ["lvgl", "web_server_base"]
AUTO_LOAD = ["json"]
CODEOWNERS = []

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required("board"): cv.string,
        cv.Required("width"): cv.int_range(min=64, max=2048),
        cv.Required("height"): cv.int_range(min=64, max=2048),
        # = safe_mode boot_is_good_after: how long the "Update settling" chip counts down after an OTA
        cv.Optional("probation", default="180s"): cv.positive_time_period_seconds,
    }
)


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/facet_wb/fw_device.h"'))
    cg.add_define("FW_DEVICE_BOARD", config["board"])
    cg.add_define("FW_DEVICE_W", config["width"])
    cg.add_define("FW_DEVICE_H", config["height"])
    cg.add_define("FW_PROBATION_S", config["probation"].total_seconds)
