// Behaviour test hooks (tools/behaviour.mjs, README "Behaviour tests"): what the headless behaviour runner reads or
// sets that the workbench API has no call for. WASM + native test builds only: not in the panel bundle
// (firmware/bundle.mjs lists its core files). Reads the default display (fw_render makes the stage it builds on the
// default; the runner keeps one stage per module instance).
//   fw_test("stack", "")          -> JSON [{stack: [ids], builds, objects}] every mounted device (core/device.h state)
//   fw_test("setting", "id=value") -> "" applied / "same" / the error (dev::set from the host, e.g. sounds=false)
//   fw_test("scrolls", "")        -> JSON [{x, y, w, h, sx, sy, t, b, l, r, dir}] every shown object that can scroll
//                                    (scrollable flag + room to scroll or scrolled), screen then top layer, tree order
#include <emscripten.h>
#include <string>
#include "fw.h"
#include "device.h"

namespace {
void scrolls(lv_obj_t *o, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_SCROLLABLE)) {
      const int32_t t = lv_obj_get_scroll_top(c), b = lv_obj_get_scroll_bottom(c), l = lv_obj_get_scroll_left(c),
                    r = lv_obj_get_scroll_right(c), sx = lv_obj_get_scroll_x(c), sy = lv_obj_get_scroll_y(c);
      if (t > 0 || b > 0 || l > 0 || r > 0 || sx || sy) {
        lv_area_t a;
        lv_obj_get_coords(c, &a);
        ArduinoJson::JsonObject j = arr.add<ArduinoJson::JsonObject>();
        j["x"] = a.x1, j["y"] = a.y1, j["w"] = lv_area_get_width(&a), j["h"] = lv_area_get_height(&a);
        j["sx"] = sx, j["sy"] = sy, j["t"] = t, j["b"] = b, j["l"] = l, j["r"] = r;
        j["dir"] = (int) lv_obj_get_scroll_dir(c);
      }
    }
    scrolls(c, arr);
  }
}
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE const char *fw_test(const char *cmd, const char *arg) {
  static std::string out;
  out.clear();
  const std::string c = cmd ? cmd : "", a = arg ? arg : "";
  ArduinoJson::JsonDocument d;
  if (c == "stack") fw::dev::state(d.to<ArduinoJson::JsonArray>());
  else if (c == "setting") {
    const size_t eq = a.find('=');
    if (eq == std::string::npos) return "id=value";
    out = fw::dev::set(a.substr(0, eq).c_str(), a.substr(eq + 1).c_str(), fw::dev::HOST);
    return out.c_str();
  } else if (c == "scrolls") {
    lv_display_t *disp = lv_display_get_default();
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    if (disp) {
      scrolls(lv_display_get_screen_active(disp), arr);
      scrolls(lv_display_get_layer_top(disp), arr);
    }
  } else return "stack | setting | scrolls";
  serializeJson(d, out);
  return out.c_str();
}
