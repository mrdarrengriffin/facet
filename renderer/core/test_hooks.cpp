// Behaviour test hooks (tools/behaviour.mjs, README "Behaviour tests"): what the headless behaviour runner reads or
// sets that the workbench API has no call for. WASM + native test builds only: not in the panel bundle
// (firmware/bundle.mjs lists its core files). Reads the default display (fw_render makes the stage it builds on the
// default; the runner keeps one stage per module instance).
//   fw_test("stack", "")          -> JSON [{stack: [ids], builds, objects}] every mounted device (core/device.h state)
//   fw_test("setting", "id=value") -> "" applied / "same" / the error (dev::set from the host, e.g. sounds=false)
//   fw_test("look", "light,accent") -> "" : the theme / accent switched LIVE through the contract's shared styles (fx::look,
//                                    no rebuild): the colour + pixel proofs compare this with a fresh build
//   fw_test("inset", "px")        -> "" : the page inset as if its token said px (fx::inset; "-1" = the token)
//   fw_test("bounds", ""|"all")   -> JSON ["..."] every object in a Page body (the contract part of a screen) that is
//                                    not inside its parent's content box (bleed skipped): the bounds proof
//   fw_test("scrolls", "")        -> JSON [{x, y, w, h, sx, sy, t, b, l, r, dir}] every shown object that can scroll
//                                    (scrollable flag + room to scroll or scrolled), screen then top layer, tree order
#include <emscripten.h>
#include <string>
#include "fw.h"
#include "device.h"
#include "fx.h"
#include "core/lv_obj_private.h"  // the style list (the bounds proof names the token styles)
#include "core/lv_obj_style_private.h"

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
bool has_style(lv_obj_t *o, const char *name) {
  for (uint32_t i = 0; i < o->style_cnt; i++) { const char *n = fx::style_name(o->styles[i].style); if (n && !strcmp(n, name)) return true; }
  return false;
}
void bounds(lv_obj_t *o, bool in_body, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    const bool body = in_body || has_style(c, "body");
    if (in_body && !has_style(c, "bleed")) {
      lv_area_t a, p;
      lv_obj_get_coords(c, &a);
      if (lv_obj_check_type(c, &lv_label_class)) { const int32_t ty = lv_obj_get_style_translate_y(c, LV_PART_MAIN); a.y1 -= ty; a.y2 -= ty; }  // the type's text-box translate is not layout
      lv_obj_get_content_coords(o, &p);
      if (a.x1 < p.x1 || a.y1 < p.y1 || a.x2 > p.x2 || a.y2 > p.y2) {
        char b[200];
        snprintf(b, sizeof b, "%dx%d at %d,%d not inside its parent's content box %dx%d at %d,%d%s%s",
                 (int) lv_area_get_width(&a), (int) lv_area_get_height(&a), (int) a.x1, (int) a.y1, (int) lv_area_get_width(&p),
                 (int) lv_area_get_height(&p), (int) p.x1, (int) p.y1, lv_obj_check_type(c, &lv_label_class) ? " text " : "",
                 lv_obj_check_type(c, &lv_label_class) ? lv_label_get_text(c) : "");
        arr.add(b);
      }
    }
    bounds(c, body, arr);
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
  } else if (c == "look") {
    const int light = atoi(a.c_str()), acc = a.find(',') == std::string::npos ? -1 : atoi(a.c_str() + a.find(',') + 1);
    fw::T = fw::theme(light, acc);
    if (lv_display_t *disp = lv_display_get_default()) {
      fx::look(disp, fw::T);
      // the stage is the host's canvas (a workbench stage paints its own bg); a panel Page paints bg:bg as a token
      lv_obj_set_style_bg_color(lv_display_get_screen_active(disp), fw::T->bg, 0);
      lv_obj_invalidate(lv_display_get_screen_active(disp));
    }
    return "";
  } else if (c == "inset") {
    fx::inset(atoi(a.c_str()));
    if (lv_display_t *disp = lv_display_get_default()) lv_obj_update_layout(lv_display_get_screen_active(disp));
    return "";
  } else if (c == "bounds") {
    lv_display_t *disp = lv_display_get_default();
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    if (disp) { lv_obj_update_layout(lv_display_get_screen_active(disp)); bounds(lv_display_get_screen_active(disp), a == "all", arr); }  // "all" = a component mounted alone: the whole screen is its parent
  } else return "stack | setting | scrolls | look | inset | bounds";
  serializeJson(d, out);
  return out.c_str();
}
