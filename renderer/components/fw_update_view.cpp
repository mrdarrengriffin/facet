// Firmware update view (facet-196): what the panel shows just before it installs new firmware, on the top layer over
// everything (firmware/fw_device.h ota_*). Neither board can draw while flash is written (the framebuffer in PSRAM
// stalls: the Waveshare's DSI flashes cyan, the Guition goes black), so the panel shows this notice for 2 s and then
// turns the backlight off until it restarts. A centred column in the content box: an icon (ic::feature), the title
// (ty::title, text), one line (ty::body, text-dim, wraps). States: updating = power icon (accent-text), "Updating",
// the version line + "The screen turns off until it restarts. Don't unplug."; failed = alert icon (error), "Update
// failed", the note ("Your panel keeps the version it has"). No taps. Update path: a new state / text fades (refill).
// Contrast pairs: "fw_update_view".
#include "fw.h"

namespace {
lv_obj_t *line(lv_obj_t *c, fw::ty::Id st, const char *s, lv_color_t col) {
  lv_obj_t *l = fw::text(c, st, s, col);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(l, fw::lay::content_w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root, fw::sp::s4, 0);
  const std::string st = p["state"] | "updating";
  const char *note = p["note"] | "";
  if (st == "failed") {
    fw::icon(root, "alert", fw::ic::feature, t.error);
    line(root, fw::ty::title, "Update failed", t.text);
    line(root, fw::ty::body, *note ? note : "Your panel keeps the version it has", t.text_dim);
    return;
  }
  fw::icon(root, "power", fw::ic::feature, t.accent_text);
  line(root, fw::ty::title, "Updating", t.text);
  const char *ver = p["version"] | "";
  if (*ver) line(root, fw::ty::body, ver, t.text_dim);
  line(root, fw::ty::body, *note ? note : "The screen turns off until it restarts. Don't unplug.", t.text_dim);
}
}  // namespace

FW_COMPONENT(fw_update_view, "Firmware update", "components", R"({
  "about": "What the panel shows just before it installs new firmware (then the screen turns off), or a clear failure.",
  "frame": [720, 720],
  "icons": ["power@feature", "alert@feature"],
  "props": [
    {"name": "state", "about": "Where the update is: updating (about to install) or failed.", "type": "select", "options": ["updating", "failed"], "default": "updating"},
    {"name": "version", "about": "The version being installed. Empty hides it.", "type": "text", "default": "0.4.0.2610261200"},
    {"name": "note", "about": "A line that replaces the default one, such as why it failed.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "updating", "props": {}},
    {"label": "failed", "props": {"state": "failed"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);
}

FW_UPDATE(fw_update_view) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });
  return true;
}
