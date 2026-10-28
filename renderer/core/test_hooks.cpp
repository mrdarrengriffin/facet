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
//   fw_test("inks", "")          -> JSON [{x, y, w, h, k, min, s}] every shown text (k "text") and icon layer (k "ui")
//                                    on the active screen (an open sheet: only its overlay), tree order; min = the WCAG minimum it must meet (text 4.5,
//                                    large text >= 24 px or bold >= 18.66 px 3, ui 3, disabled (fw::st::OFF) 3); the
//                                    pixel contrast proof (tools/pixel_contrast.mjs) veils each one and compares pixels
//   fw_test("veil", "i,1|0")     -> "" : item i of the last "inks" list drawn fully transparent (1) or as before (0);
//                                    no layout change, only its own pixels go
//   fw_test("gfx", "")           -> JSON [{i, x, y, w, h, s, on}] every shown state-carrying graphic object (a style named
//                                    in GFX: the toggle knob, a segment), indexed into the same list "veil" uses
//                                    (WCAG 1.4.11, the graphical-object half of tools/pixel_contrast.mjs)
//   fw_test("veil_part", "knob") -> "" : the Pen skips that drawn part (knob, track-fill, arc-fill; "" = none) and the
//                                    screen repaints: the drawn half of the same proof
//   fw_test("timer", "{op, name?, seconds?, id?}") -> JSON {why, timers: [{id, name, left, total, state}] soonest first,
//                                    shown: id, store: {attr: value} (the panel.timer pairs), changes: ["start Pasta", ...]
//                                    since the last call}; op = start | extend | cancel | stop | select | find | reset |
//                                    list (core/device.h "the panel's timers")
//   fw_test("scrolls", "")        -> JSON [{x, y, w, h, sx, sy, t, b, l, r, dir}] every shown object that can scroll
//                                    (scrollable flag + room to scroll or scrolled), screen then top layer, tree order
#include <emscripten.h>
#include <string>
#include <vector>
#include "fw.h"
#include "device.h"
#include "fx.h"
#include "sheet.h"
#include "toasts.h"
#include "ha.h"
#include "core/lv_obj_private.h"  // the style list (the bounds proof names the token styles)
#include "core/lv_obj_style_private.h"

namespace fw { const Component *owner_of(lv_obj_t *o); bool in_legacy(lv_obj_t *o); }  // core/fw_core.cpp (hosts only)
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
    if (in_body && !has_style(c, "bleed") && fw::sheet::panel(o) != c) {  // a sheet's panel overhangs on purpose
      lv_area_t a, p;
      lv_obj_get_coords(c, &a);
      if (lv_obj_check_type(c, &lv_label_class)) { const int32_t ty = lv_obj_get_style_translate_y(c, LV_PART_MAIN); a.y1 -= ty; a.y2 -= ty; }  // the type's text-box translate is not layout
      lv_obj_get_content_coords(o, &p);
      // a floating mark on its parent's edge (the secondary Button's corner ticks): inside the border box is inside
      lv_area_t bb;
      lv_obj_get_coords(o, &bb);
      const bool edge_mark = lv_obj_has_flag(c, LV_OBJ_FLAG_IGNORE_LAYOUT) && a.x1 >= bb.x1 && a.y1 >= bb.y1 && a.x2 <= bb.x2 && a.y2 <= bb.y2;
      if (!edge_mark && (a.x1 < p.x1 || a.y1 < p.y1 || a.x2 > p.x2 || a.y2 > p.y2)) {
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
std::vector<lv_obj_t *> ink_objs;
void forget_ink(lv_event_t *e) {
  for (auto &o : ink_objs) if (o == lv_event_get_current_target(e)) o = nullptr;
}
float wcag_min(lv_obj_t *o, bool text) {
  for (lv_obj_t *a = o; a; a = lv_obj_get_parent(a)) if (lv_obj_has_state(a, fw::st::OFF)) return 3;  // disabled / unavailable
  if (!text) return 3;
  const lv_font_t *f = lv_obj_get_style_text_font(o, LV_PART_MAIN);
  for (int i = 0; i < fw::ty::N; i++)
    if (fw::font((fw::ty::Id) i) == f) {
      const fw::ty::Style &st = fw::ty::STYLES[i];
      return st.size >= 24 || (st.weight >= 700 && st.size * 3 >= 56) ? 3 : 4.5f;
    }
  return 4.5f;
}
void inks(lv_obj_t *o, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_style_opa_recursive(c, LV_PART_MAIN) <= LV_OPA_MIN) continue;
    const bool text = lv_obj_check_type(c, &lv_label_class), img = lv_obj_check_type(c, &lv_image_class);
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    if (((text && *lv_label_get_text(c)) || img) && !fw::toasts::under(c)) {
      if (lv_area_get_width(&a) > 0 && lv_area_get_height(&a) > 0) {
        auto r = arr.add<ArduinoJson::JsonObject>();
        r["i"] = (int) ink_objs.size(); r["x"] = a.x1; r["y"] = a.y1; r["w"] = lv_area_get_width(&a); r["h"] = lv_area_get_height(&a);
        r["k"] = text ? "text" : "ui"; r["min"] = wcag_min(c, text);
        if (img) r["grp"] = (double) (uintptr_t) lv_obj_get_parent(c);  // an icon's layers: what is behind = the icon veiled
        if (text) r["s"] = lv_label_get_text(c);
        // the colour it draws in (RGB565 word): text colour, or an icon layer's recolour (-1 = a picture's own colours)
        const lv_color_t k = text ? lv_obj_get_style_text_color(c, LV_PART_MAIN) : lv_obj_get_style_image_recolor(c, LV_PART_MAIN);
        r["ink"] = !text && lv_obj_get_style_image_recolor_opa(c, LV_PART_MAIN) < LV_OPA_COVER ? -1 : (int) (((k.red >> 3) << 11) | ((k.green >> 2) << 5) | (k.blue >> 3));
        const fw::Component *ow = fw::owner_of(c);  // the innermost component that built it
        r["c"] = ow ? ow->id : "";
        r["legacy"] = !fx::migrated(ow) || fw::in_legacy(c);
        ink_objs.push_back(c);
        lv_obj_add_event_cb(c, forget_ink, LV_EVENT_DELETE, nullptr);
      }
    }
    inks(c, arr);
  }
}
const char *const GFX[] = {"toggle-knob", "seg-part"};  // the objects that show a state (the drawn ones: Pen parts)
void gfx(lv_obj_t *o, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_style_opa_recursive(c, LV_PART_MAIN) <= LV_OPA_MIN) continue;
    // a knob the theme took away (switch shape check: the checkbox's tick is checked as the drawn part "switch")
    if (has_style(c, "toggle-knob") && lv_obj_get_style_bg_opa(c, LV_PART_MAIN) <= LV_OPA_MIN) continue;
    for (const char *g : GFX)
      if (has_style(c, g) && !fw::toasts::under(c)) {
        lv_area_t a;
        lv_obj_get_coords(c, &a);
        auto r = arr.add<ArduinoJson::JsonObject>();
        r["i"] = (int) ink_objs.size(); r["x"] = a.x1; r["y"] = a.y1; r["w"] = lv_area_get_width(&a); r["h"] = lv_area_get_height(&a);
        r["s"] = g; r["on"] = lv_obj_has_state(c, LV_STATE_CHECKED); r["min"] = wcag_min(c, false);
        ink_objs.push_back(c);
        lv_obj_add_event_cb(c, forget_ink, LV_EVENT_DELETE, nullptr);
        break;
      }
    gfx(c, arr);
  }
}
std::vector<std::string> timer_changes;
void timer_seen(fw::dev::timers::Change w, const fw::dev::TimerItem &t) {
  static const char *const N[] = {"start", "extend", "cancel", "stop", "done", "select"};
  timer_changes.push_back(std::string(N[w]) + " " + t.name);
}
void timer_cmd(const std::string &a, ArduinoJson::JsonDocument &d) {
  namespace tm = fw::dev::timers;
  static bool seen = false;
  if (!seen) { seen = true; tm::observe(timer_seen); }
  ArduinoJson::JsonDocument q;
  deserializeJson(q, a);
  const std::string op = q["op"] | "list";
  const int id = q["id"].is<int>() ? (int) q["id"] : tm::find(q["name"] | "");
  const char *why = "";
  if (op == "start") why = tm::start(q["name"] | "", q["seconds"] | 0);
  else if (op == "extend") why = tm::extend(id, q["seconds"] | 60);
  else if (op == "cancel") why = tm::cancel(id);
  else if (op == "stop") why = tm::stop(id);
  else if (op == "select") why = tm::select(id);
  else if (op == "reset") { tm::reset(); timer_changes.clear(); }
  d["why"] = why;
  d["found"] = id;
  ArduinoJson::JsonArray arr = d["timers"].to<ArduinoJson::JsonArray>();
  for (const fw::dev::TimerItem &t : tm::all()) {
    ArduinoJson::JsonObject j = arr.add<ArduinoJson::JsonObject>();
    j["id"] = t.id, j["name"] = t.name, j["left"] = tm::left(t), j["total"] = t.total, j["state"] = t.state == 1 ? "running" : "done";
  }
  const fw::dev::TimerItem *sh = tm::shown();
  d["shown"] = sh ? sh->id : 0;
  for (const char *k : {"", "id", "name", "left", "left_s", "total", "share", "line", "count", "running", "free", "others", "all", "bar"})
    if (const std::string *v = fw::ha::get(fw::dev::TIMER_ENTITY, k)) d["store"][*k ? k : "state"] = *v;
  std::string lj;
  tm::list_json(lj);
  d["ha_list"] = lj;
  const fw::dev::Timer &g = fw::dev::timer();
  d["legacy"]["state"] = g.state, d["legacy"]["label"] = g.label;
  ArduinoJson::JsonArray ch = d["changes"].to<ArduinoJson::JsonArray>();
  for (const std::string &c : timer_changes) ch.add(c);
  timer_changes.clear();
}
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE const char *fw_test(const char *cmd, const char *arg) {
  static std::string out;
  out.clear();
  const std::string c = cmd ? cmd : "", a = arg ? arg : "";
  ArduinoJson::JsonDocument d;
  if (c == "stack") fw::dev::state(d.to<ArduinoJson::JsonArray>());
  else if (c == "timer") timer_cmd(a, d);
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
      lv_obj_set_style_bg_color(lv_display_get_screen_active(disp), fx::bg(disp), 0);  // = T->bg in Facet
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
  } else if (c == "inks") {
    for (lv_obj_t *o : ink_objs) if (o) lv_obj_remove_event_cb(o, forget_ink);
    ink_objs.clear();
    lv_display_t *disp = lv_display_get_default();
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    if (disp) {  // an open sheet: only its overlay (the screen under the scrim is dimmed on purpose, C4)
      lv_obj_t *scr = lv_display_get_screen_active(disp);
      lv_obj_update_layout(scr);
      lv_obj_t *ov = fw::sheet::open_on(scr);
      inks(ov ? ov : scr, arr);
    }
  } else if (c == "gfx") {  // appends to the "inks" list (call "inks" first: it clears it)
    lv_display_t *disp = lv_display_get_default();
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    if (disp) {
      lv_obj_t *scr = lv_display_get_screen_active(disp);
      lv_obj_update_layout(scr);
      lv_obj_t *ov = fw::sheet::open_on(scr);
      gfx(ov ? ov : scr, arr);
    }
  } else if (c == "veil_part") {
    fx::pen_veil = a;
    if (lv_display_t *disp = lv_display_get_default()) lv_obj_invalidate(lv_display_get_screen_active(disp));
    return "";
  } else if (c == "veil") {
    const int i = atoi(a.c_str()), on = a.find(",1") != std::string::npos;
    if (i < 0 || i >= (int) ink_objs.size() || !ink_objs[i]) return "gone";
    if (on) lv_obj_set_style_opa(ink_objs[i], LV_OPA_TRANSP, 0); else lv_obj_remove_local_style_prop(ink_objs[i], LV_STYLE_OPA, 0);
    lv_obj_invalidate(ink_objs[i]);
    return "";
  } else return "stack | timer | setting | scrolls | look | inset | bounds | inks | gfx | veil | veil_part";
  serializeJson(d, out);
  return out.c_str();
}
