// Facet WASM workbench host (README.md): LVGL + any number of "stages" (one lv_display each, its
// own size, theme and pointer), so the page can show a big preview and a grid of variants from ONE module.
// Same LVGL 9.5 tree + lv_conf as the panel (build.sh), RGB565 like the glass. No lab chrome, no specs, no HA.
//   fw_init()                         once
//   fw_meta() -> JSON                 every registered component: [{id,name,section,meta}]
//   fw_stage(w, h) -> id              a new display, w x h device pixels
//   fw_stage_free(id)
//   fw_render(id, comp, props, light) build component `comp` with props JSON on stage `id` -> "" or an error
//   fw_update(id, comp, props, light, accent) the update path: new props in place, animated, silent -> "" / "rebuild" / error
//   fw_demo_state(id) -> -1 / 0 / 1   demos on that stage: none reported / all at their preset (skip the demo reset) / one moved away
//   fw_touch(id, x, y, down)          pointer on that stage (device pixels)
//   fw_tick(ms)                       LVGL timers/refresh for all stages
//   fw_dirty(id) / fw_frame(id)       redrawn since last read? / RGBA pixels (w*h*4)
//   fw_areas(id) -> int32 [n, x1, y1, x2, y2...]  areas flushed since the last call (copy only these)
//   fw_anims() -> n                   LVGL animations running (frame stats)
//   fw_events() -> JSON               events + sound requests since the last call (fw::emit / fw::sound): [{stage,kind,name,detail}] or ""
//   fw_ha_wants() -> JSON [[e, a]...]  HA pairs the built screens bind (core/ha.h); fw_ha_put(e, a, v) = a value from the host
//   fw_ha_asks() -> JSON [{id, service, data}]  HA actions an adapter wants the reply of (drained); fw_ha_answer(id, ok, json)
//   fw_img_wants() -> JSON [url...]  image URLs to fetch (core/images.h); fw_img_put(url, w, h, rgba) / fw_img_fail(url, why); fw_img_stats() -> JSON
//   fw_audit(id) -> JSON              every object of the stage for tools/audit.mjs (core/audit.h, README "Audit")
//   fw_unclip(id)                     let every object draw outside its box (audit: what differs was clipped)
#include <emscripten.h>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <functional>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "spec.h"
#include "device.h"
#include "images.h"
#include "fx.h"
#include "sheet.h"
#include "core/lv_obj_style_private.h"  // the style list (fw_lab names the token styles)
// LVGL private (src/core/lv_obj_draw_private.h): the ext draw size an object holds (fw::bleed sets it)
extern "C" int32_t lv_obj_get_ext_draw_size(const lv_obj_t *obj);
#include <map>
#include "core/lv_obj_private.h"  // ext_click_pad (audit)
#include "misc/lv_text_private.h"  // lv_text_encoded_next (audit)
namespace fx { std::string theme_lab(const std::string &cmd, const std::string &arg); }  // core/theme_lab.cpp (fw_lab theme_*)
namespace fx { std::string font_lab(const std::string &cmd, const std::string &arg); }   // core/font_lab.cpp (fw_lab font_*)
namespace fx { std::string pen_lab(lv_display_t *d, const std::string &cmd, const std::string &arg); }  // core/pen_lab.cpp (fw_lab pen_*)


namespace {
struct Stage {
  lv_display_t *d = nullptr;
  lv_indev_t *in = nullptr;
  int w = 0, h = 0;
  uint8_t *buf = nullptr;
  std::vector<uint32_t> rgba;
  bool dirty = false, down = false;
  std::vector<int32_t> areas;  // areas flushed since the last fw_areas (x1, y1, x2, y2 each): the host copies only these
  lv_point_t pos{};
};
std::vector<Stage *> stages;
uint32_t now_ms = 0;
std::string out;

Stage *get(int id) { return id >= 0 && id < (int) stages.size() ? stages[id] : nullptr; }
uint32_t tick() { return now_ms; }
void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  auto *s = (Stage *) lv_display_get_user_data(d);
  auto *p = reinterpret_cast<uint16_t *>(px);
  const int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; ++y)
    for (int x = a->x1; x <= a->x2; ++x) {
      uint16_t c = p[(y - a->y1) * w + (x - a->x1)];
      uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      s->rgba[y * s->w + x] = 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (r << 3 | r >> 2);
    }
  s->dirty = true;
  if (s->areas.size() < 4 * 64) s->areas.insert(s->areas.end(), {a->x1, a->y1, a->x2, a->y2});
  else s->areas.assign({0, 0, s->w - 1, s->h - 1});  // too many: the whole stage
  lv_display_flush_ready(d);
}
void read(lv_indev_t *i, lv_indev_data_t *data) {
  auto *s = (Stage *) lv_indev_get_user_data(i);
  data->point = s->pos;
  data->state = s->down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
// fw::emit / fw::sound -> a queue the page drains every frame (fw_events)
ArduinoJson::JsonDocument evq;
std::string evout;
void on_event(lv_obj_t *obj, const char *kind, const char *name, const char *detail) {
  lv_display_t *d = obj ? lv_obj_get_display(obj) : nullptr;
  int id = -1;
  for (size_t i = 0; i < stages.size(); i++)
    if (stages[i] && stages[i]->d == d) id = (int) i;
  if (!evq.is<ArduinoJson::JsonArray>()) evq.to<ArduinoJson::JsonArray>();
  auto o = evq.as<ArduinoJson::JsonArray>().add<ArduinoJson::JsonObject>();
  o["stage"] = id;
  o["kind"] = kind;
  o["name"] = name;
  o["detail"] = detail;
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void fw_init() {
  lv_init();
  lv_tick_set_cb(tick);
  fw::set_sink(on_event);
}

EMSCRIPTEN_KEEPALIVE const char *fw_events() {
  evout.clear();
  if (evq.is<ArduinoJson::JsonArray>() && evq.as<ArduinoJson::JsonArray>().size()) serializeJson(evq, evout);
  evq.clear();
  return evout.c_str();
}

// Home Assistant data (core/ha.h): the host polls the wanted pairs and puts their values (hub /api/panel/entities)
EMSCRIPTEN_KEEPALIVE const char *fw_ha_wants() {
  static std::string out;
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonArray a = d.to<ArduinoJson::JsonArray>();
  for (const auto &p : fw::ha::wants()) {
    ArduinoJson::JsonArray x = a.add<ArduinoJson::JsonArray>();
    x.add(p.first);
    x.add(p.second);
  }
  out.clear();
  serializeJson(d, out);
  return out.c_str();
}
// Images (core/images.h): the host polls the wanted URLs, fetches + decodes them and puts RGBA8888 pixels (or a failure)
EMSCRIPTEN_KEEPALIVE const char *fw_img_wants() {
  static std::string out;
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonArray a = d.to<ArduinoJson::JsonArray>();
  for (const auto &u : fw::img::wants()) a.add(u);
  out.clear();
  serializeJson(d, out);
  return out.c_str();
}
EMSCRIPTEN_KEEPALIVE void fw_img_put(const char *src, int w, int h, const uint8_t *rgba) { fw::img::put(src, w, h, rgba); }
EMSCRIPTEN_KEEPALIVE void fw_img_fail(const char *src, const char *why) { fw::img::fail(src, why); }
EMSCRIPTEN_KEEPALIVE const char *fw_img_stats() {
  static std::string out;
  out = fw::img::stats();
  return out.c_str();
}
EMSCRIPTEN_KEEPALIVE void fw_ha_put(const char *e, const char *a, const char *v) { fw::ha::put(e ? e : "", a ? a : "", v ? v : ""); }
// asks (core/ha.h, CONTRACT.md C51 adapters): the host takes the queued HA actions whose reply an adapter reads
// (fw_ha_asks -> JSON [{id, service, data}], drained) and hands each reply back (fw_ha_answer(id, ok, json))
std::vector<std::string> &ask_queue() {
  static std::vector<std::string> q;
  static const bool reg = (fw::ha::on_ask([](uint32_t id, const char *svc, const char *data) {
    ArduinoJson::JsonDocument d;
    d["id"] = id, d["service"] = svc;
    ArduinoJson::JsonDocument dd;
    deserializeJson(dd, data);
    d["data"] = dd;
    std::string o;
    serializeJson(d, o);
    q.push_back(o);
  }), true);
  (void) reg;
  return q;
}
[[maybe_unused]] const bool ask_registered = (ask_queue(), true);
EMSCRIPTEN_KEEPALIVE const char *fw_ha_asks() {
  static std::string out;
  out = "[";
  for (size_t i = 0; i < ask_queue().size(); i++) out += (i ? "," : "") + ask_queue()[i];
  out += "]";
  ask_queue().clear();
  return out.c_str();
}
EMSCRIPTEN_KEEPALIVE void fw_ha_answer(int id, int ok, const char *json) { fw::ha::answer((uint32_t) id, ok != 0, json ? json : ""); }

// One panel setting from the host (the docs site's live demo: its theme / accent / sound prefs and the demo's own
// Settings stay one value), through the ONE set() path (core/device.h) as HA or the panel API: "" applied, "same",
// else the error. value null = read: the current value ("" = unknown id).
EMSCRIPTEN_KEEPALIVE const char *fw_setting(const char *id, const char *value) {
  static std::string out;
  if (!value) { const char *v = fw::dev::setting(id ? id : ""); out = v ? v : ""; }
  else out = fw::dev::set(id ? id : "", value, fw::dev::HOST);
  return out.c_str();
}

// Screen specs (core/spec.h): fw_spec(cmd, arg) -> JSON. cmd: check / check_dev (a screen or deck spec: the errors, the
// components it uses, nodes, binds), screen / screen_dev (dev = may replace a built-in) / deck (apply: validate, build once off screen, store, swap), revert
// (arg = id | deck | all), hello, act (arg = one action JSON: {ok, why?, state?}, CONTRACT.md C50), actions (the documented
// action list), list (every live spec). Storage here = memory (fw_spec_storage: fault injection +
// reboot for the power-loss test, tools/spec_test.mjs).
namespace {
std::map<std::string, std::string> mem_store;
int fault_at = -1, writes = 0;  // the write that loses power (-1 = none); after it every write is lost too
std::string fault_mode = "drop";
void mem_io() {
  auto &io = fw::spec::S().io;
  io.load = [](const std::string &k, std::string &out) { auto it = mem_store.find(k); if (it == mem_store.end()) return false; out = it->second; return true; };
  io.save = [](const std::string &k, const std::string &v) {
    const int n = writes++;
    if (fault_at >= 0 && n >= fault_at) {
      if (n == fault_at && fault_mode == "torn") mem_store[k] = v.substr(0, v.size() / 2);  // a write cut in half
      return true;  // the device thinks it wrote (power is going): nothing after this lands
    }
    mem_store[k] = v;
    return true;
  };
  io.erase = [](const std::string &k) { mem_store.erase(k); };
}
}  // namespace
EMSCRIPTEN_KEEPALIVE const char *fw_spec(const char *cmd, const char *arg) {
  static std::string out;
  if (!fw::spec::S().io.save) mem_io();
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonObject o = d.to<ArduinoJson::JsonObject>();
  const std::string c = cmd ? cmd : "", a = arg ? arg : "";
  if (c == "check" || c == "check_dev") {
    ArduinoJson::JsonDocument s;
    fw::spec::Errs e;
    if (deserializeJson(s, a)) fw::spec::err(e, "", "bad_json", "not JSON");
    else if (s["pages"].is<ArduinoJson::JsonArray>() || s["screens"].is<ArduinoJson::JsonArray>()) fw::spec::check_deck(s.as<ArduinoJson::JsonVariantConst>(), a.size(), e);
    else { fw::spec::check_screen(s.as<ArduinoJson::JsonVariantConst>(), a.size(), e); fw::spec::no_built_in(s["id"] | "", c == "check_dev", e); }
    ArduinoJson::JsonDocument ed;
    deserializeJson(ed, fw::spec::errs_json(e));
    o["ok"] = e.empty();
    o["errors"] = ed;
    auto comps = o["components"].to<ArduinoJson::JsonArray>();
    int binds = 0;
    for (ArduinoJson::JsonVariantConst n : s["nodes"].as<ArduinoJson::JsonArrayConst>()) {
      bool have = false;
      for (ArduinoJson::JsonVariantConst x : comps) have = have || x == n["c"];
      if (!have) comps.add(n["c"]);
      for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) binds += kv.value()["e"].is<const char *>();
    }
    o["nodes"] = s["nodes"].size();
    o["binds"] = binds;
  } else if (c == "screen" || c == "screen_dev" || c == "deck") {
    lv_display_t *disp = lv_display_get_default();
    lv_obj_t *dry = disp ? lv_obj_create(lv_display_get_screen_active(disp)) : nullptr;
    if (dry) { lv_obj_add_flag(dry, LV_OBJ_FLAG_HIDDEN); }
    fw::spec::apply(c.c_str(), a, dry, o);
    if (dry) lv_obj_delete(dry);
  } else if (c == "revert") fw::spec::revert(a.empty() ? "all" : a, o);
  else if (c == "hello") fw::spec::hello(o);
  else if (c == "act") {  // one action from outside a spec (CONTRACT.md C50: the same as POST /fw/act, facet_act)
    std::string why;
    ArduinoJson::JsonDocument st;
    o["ok"] = fw::spec::act(a, why, st.to<ArduinoJson::JsonObject>());
    if (!why.empty()) o["why"] = why;
    if (st.size()) o["state"] = st;
  } else if (c == "actions") fw::spec::actions_json(o["actions"].to<ArduinoJson::JsonArray>());
  else if (c == "list") {
    for (const auto &f : fw::spec::FACTORY) o["screens"][f.id] = fw::spec::screen_json(f.id);
    for (const auto &kv : fw::spec::S().over) o["screens"][kv.first] = kv.second;
    o["deck"] = fw::spec::deck_json();
  } else { o["ok"] = false; o["error"] = "check | screen | deck | revert | hello | act | actions | list"; }
  out.clear();
  serializeJson(d, out);
  return out.c_str();
}
// fault <n> [torn|drop] (the nth storage write from now loses power) | reboot (load the store again) | reset | dump
EMSCRIPTEN_KEEPALIVE const char *fw_spec_storage(const char *cmd, const char *arg) {
  static std::string out;
  if (!fw::spec::S().io.save) mem_io();
  const std::string c = cmd ? cmd : "";
  ArduinoJson::JsonDocument d;
  if (c == "fault") { fault_at = writes + atoi(arg ? arg : "0"); fault_mode = arg && strstr(arg, "torn") ? "torn" : "drop"; }
  else if (c == "reboot") { fault_at = -1; fw::spec::load(); d["log"].to<ArduinoJson::JsonArray>(); for (auto &x : fw::spec::S().log) d["log"].add(x); }
  else if (c == "reset") { mem_store.clear(); fault_at = -1; fw::spec::load(); }
  for (const auto &kv : mem_store) d["keys"][kv.first] = (int) kv.second.size();
  out.clear();
  serializeJson(d, out);
  return out.c_str();
}
// the catalogue (core/spec.h) + a cost per component: objects in its default build (tools/gen_catalogue.mjs)
EMSCRIPTEN_KEEPALIVE const char *fw_catalogue() {
  static std::string out;
  ArduinoJson::JsonDocument d;
  fw::spec::catalogue(d.to<ArduinoJson::JsonObject>());
  char h[12];
  snprintf(h, sizeof h, "%08x", (unsigned) fw::spec::catalogue_hash());
  d["hash"] = h;
  lv_display_t *disp = lv_display_get_default();
  if (disp) {
    lv_obj_t *tmp = lv_obj_create(lv_display_get_screen_active(disp));
    lv_obj_add_flag(tmp, LV_OBJ_FLAG_HIDDEN);
    for (const fw::Component &c : fw::reg()) {
      if (strcmp(c.section, "components")) continue;
      ArduinoJson::JsonDocument none;
      none.to<ArduinoJson::JsonObject>();
      lv_obj_clean(tmp);
      {
        fw::Quiet q;
        c.build(tmp, none.as<fw::Props>());
      }
      std::function<int(lv_obj_t *)> count = [&](lv_obj_t *o) { int n = 1; for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) n += count(lv_obj_get_child(o, i)); return n; };
      const int objs = count(tmp) - 1;
      d["components"][c.id]["cost"]["objects"] = objs;
      d["components"][c.id]["cost"]["bytes_est"] = objs * 160;  // LVGL obj + styles, rough (priced on the panel later)
    }
    lv_obj_delete(tmp);
  }
  out.clear();
  serializeJson(d, out);
  return out.c_str();
}

EMSCRIPTEN_KEEPALIVE const char *fw_meta() {
  out.clear();
  fw::meta_json(out);
  return out.c_str();
}

EMSCRIPTEN_KEEPALIVE int fw_stage(int w, int h) {
  auto *s = new Stage();
  s->w = w;
  s->h = h;
  s->rgba.assign((size_t) w * h, 0xFF000000u);
  size_t n = ((size_t) w * h * 2 + 63) & ~(size_t) 63;
  s->buf = (uint8_t *) std::aligned_alloc(64, n);  // LV_DRAW_BUF_ALIGN 64 (the panel's lv_conf)
  s->d = lv_display_create(w, h);
  lv_display_set_user_data(s->d, s);
  lv_display_set_color_format(s->d, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s->d, flush);
  lv_display_set_buffers(s->d, s->buf, nullptr, n, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_theme(s->d, nullptr);  // clean slate: components style everything themselves
  s->in = lv_indev_create();
  lv_indev_set_type(s->in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(s->in, read);
  lv_indev_set_user_data(s->in, s);
  lv_indev_set_display(s->in, s->d);
  for (size_t i = 0; i < stages.size(); i++)
    if (!stages[i]) { stages[i] = s; return (int) i; }
  stages.push_back(s);
  return (int) stages.size() - 1;
}

EMSCRIPTEN_KEEPALIVE void fw_stage_free(int id) {
  Stage *s = get(id);
  if (!s) return;
  lv_indev_delete(s->in);
  lv_display_delete(s->d);
  std::free(s->buf);
  delete s;
  stages[id] = nullptr;
}

EMSCRIPTEN_KEEPALIVE const char *fw_render(int id, const char *comp, const char *props, int light, int accent) {
  Stage *s = get(id);
  if (!s) return "no such stage";
  lv_display_set_default(s->d);
  return fw::render(lv_display_get_screen_active(s->d), comp, props, light, accent);
}

// new props for what stage `id` shows, in place (fw::update): "" = applied (the component animates there, silently),
// "rebuild" = call fw_render instead (another component / theme / accent, no update path, or a structural prop)
EMSCRIPTEN_KEEPALIVE const char *fw_update(int id, const char *comp, const char *props, int light, int accent) {
  Stage *s = get(id);
  if (!s) return "no such stage";
  lv_display_set_default(s->d);
  return fw::update(lv_display_get_screen_active(s->d), comp, props, light, accent);
}

// demo state of stage `id` (fw::demo_state): -1 no demo reported, 0 every demo at its preset (hosts skip the demo
// reset), 1 a demo moved away (reset as usual)
EMSCRIPTEN_KEEPALIVE int fw_demo_state(int id) {
  Stage *s = get(id);
  return s ? fw::demo_state(lv_display_get_screen_active(s->d)) : -1;
}

// the colour tokens file (JSON) the build was generated from
EMSCRIPTEN_KEEPALIVE const char *fw_tokens() { return fw::tokens::JSON; }

// height in device px the last build of stage `id` needs: lets the page size an "auto height" stage (meta "frame":
// [w, 0]) to its content. The screen centres its content (flex column), so content taller than the stage sticks out
// at BOTH ends: measure top to bottom of the children (not 0 to the bottom, which clipped a 96 px List row in a 64 px
// first stage to 80) plus the screen's padding, and widen that to any bled mark (fw::bleed, focus ring outline) that
// reaches further. fw::bleed puts the same ext draw size on the whole ancestor chain, so a mark is counted on the
// deepest object holding it (an object whose children hold as much is only passing it up). Same answer whatever the
// stage's current height. Labels don't count (glyph overhang, see reach()).
namespace {
void reach(lv_obj_t *o, int32_t &top, int32_t &bot) {
  int32_t kid = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    kid = LV_MAX(kid, lv_obj_get_ext_draw_size(c));
    reach(c, top, bot);
  }
  const int32_t ext = lv_obj_get_ext_draw_size(o);
  // labels skipped: their ext is the font's glyph overhang (the text-box rule cuts line boxes to caps on purpose)
  if (ext <= 0 || kid >= ext || lv_obj_check_type(o, &lv_label_class)) return;
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  top = LV_MIN(top, a.y1 - ext);
  bot = LV_MAX(bot, a.y2 + 1 + ext);
}
}  // namespace
EMSCRIPTEN_KEEPALIVE int fw_content_h(int id) {
  Stage *s = get(id);
  if (!s) return 0;
  lv_obj_t *scr = lv_display_get_screen_active(s->d);
  int32_t top = INT32_MAX, bot = INT32_MIN;
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_t *c = lv_obj_get_child(scr, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    top = LV_MIN(top, a.y1 - lv_obj_get_style_pad_top(scr, LV_PART_MAIN));
    bot = LV_MAX(bot, a.y2 + 1 + lv_obj_get_style_pad_bottom(scr, LV_PART_MAIN));
    reach(c, top, bot);
  }
  return top == INT32_MAX ? 0 : bot - top;
}

// every object under stage `id`'s screen (depth first): [{d: depth (1 = the screen's children), x, y, w, h}] in device
// px, coords as drawn (translate included): the workbench Pixel grid view outlines them (hub src/wasm/pixgrid.ts)
namespace {
void rects(lv_obj_t *o, int depth, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    auto r = arr.add<ArduinoJson::JsonObject>();
    r["d"] = depth;
    r["x"] = a.x1;
    r["y"] = a.y1;
    r["w"] = lv_area_get_width(&a);
    r["h"] = lv_area_get_height(&a);
    rects(c, depth + 1, arr);
  }
}
}  // namespace
// every tap target on stage `id`'s screen (the workbench "Tap areas" view, hub src/wasm/taps.ts): each visible clickable
// object with its hit area = its box grown by the ext click area (fx::hit, C32): [{x, y, w, h, bx, by, bw, bh, min}]
// in device px (b* = the drawn box), min = touch:min now, so the page marks a target smaller than it
namespace {
void taps(lv_obj_t *o, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    // a scrolling container (the Body) is clickable only so a drag scrolls: not a tap target
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_CLICKABLE) && !(lv_obj_has_flag(c, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_child_count(c))) {
      lv_area_t a;
      lv_obj_get_coords(c, &a);
      const int32_t e = c->spec_attr ? c->spec_attr->ext_click_pad : 0;
      auto r = arr.add<ArduinoJson::JsonObject>();
      r["x"] = a.x1 - e; r["y"] = a.y1 - e; r["w"] = lv_area_get_width(&a) + 2 * e; r["h"] = lv_area_get_height(&a) + 2 * e;
      r["bx"] = a.x1; r["by"] = a.y1; r["bw"] = lv_area_get_width(&a); r["bh"] = lv_area_get_height(&a);
      r["min"] = fx::size("touch:min");
    }
    taps(c, arr);
  }
}
}  // namespace
EMSCRIPTEN_KEEPALIVE const char *fw_taps(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  ArduinoJson::JsonDocument doc;
  taps(lv_display_get_screen_active(s->d), doc.to<ArduinoJson::JsonArray>());
  serializeJson(doc, out);
  return out.c_str();
}
EMSCRIPTEN_KEEPALIVE const char *fw_rects(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  ArduinoJson::JsonDocument doc;
  rects(lv_display_get_screen_active(s->d), 1, doc.to<ArduinoJson::JsonArray>());
  serializeJson(doc, out);
  return out.c_str();
}

// the component instances on stage `id` (fw::comps_json): the workbench Components view outlines them
EMSCRIPTEN_KEEPALIVE const char *fw_comps(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  fw::comps_json(lv_display_get_screen_active(s->d), out);
  return out.c_str();
}

// the workbench Components view: every meta slot (C8a) shown on stage `id`, [{n, x, y, w, h}] (fx::slot)
EMSCRIPTEN_KEEPALIVE const char *fw_slots(int id) {
  Stage *s = get(id);
  if (!s) return "[]";
  // on a spec screen each slot also says whose it is (the editor adds nodes there): "in" = the node index, or
  // "header" for the screen's header (spec "in": k | "header", "slot": n)
  out = fx::slots_json(lv_display_get_screen_active(s->d), [](lv_obj_t *o) {
    for (fw::spec::Screen *sc : fw::spec::live()) {
      bool inside = false;
      for (lv_obj_t *p = o; p && !inside; p = lv_obj_get_parent(p)) inside = p == sc->root;
      if (!inside) continue;
      fw::spec::Node *n = fw::spec::node_of(sc, o);
      return n ? (int) (n - sc->nodes.data()) : -1;
    }
    return -2;
  });
  return out.c_str();
}

// the editor's click-to-edit (core/spec.h anchors): every anchored region of the spec screen on stage `id`
// [{n, c, prop?, params, x, y, w, h}] (device px, as drawn); fw_hit = the smallest one at (x, y), "null" if none
EMSCRIPTEN_KEEPALIVE const char *fw_hits(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  ArduinoJson::JsonDocument doc;
  fw::spec::hits(s->d, doc.to<ArduinoJson::JsonArray>());
  serializeJson(doc, out);
  return out.c_str();
}
EMSCRIPTEN_KEEPALIVE const char *fw_hit(int id, int x, int y) {
  out.clear();
  Stage *s = get(id);
  ArduinoJson::JsonDocument doc;
  if (!s || !fw::spec::hit(s->d, x, y, doc.to<ArduinoJson::JsonObject>())) return "null";
  serializeJson(doc, out);
  return out.c_str();
}

// the workbench Layout lab (hub src/wasm/lablayout.ts): fw_lab(id, cmd, arg)
//   "tokens", ""           -> [[name, token px, now px]] every size token (fx::tunables)
//   "tune", "name=px"      -> "" : that token as if it said px, live on every stage (fx::tune; "=" alone = all back)
//   "uses", ""             -> ["name"]: the size tokens stage id's objects read (their shared styles + the pen while
//                             drawing; page ones only with a Page on it): the lab shows exactly these
//   "probe", "<squeeze>"   -> {name: 1 | 0}: which size tokens (+ "squeeze") change stage id's pixels (each moved
//                             and put back, rendered synchronously): the lab shows what this component really uses
//   "theme_*", <json>      -> the theme engine: load, apply, dump, contrast, builtins (core/theme_lab.cpp)
//   "proof", ""            -> {bounds, overlap, trunc: [{x, y, w, h, why}]} for stage id: Body objects outside their
//                             parent's content box (bleed skipped), clickable areas that overlap (not nested), text
//                             wider than its label (cut to "...")
namespace {
bool lab_has(lv_obj_t *o, const char *name) {
  for (uint32_t i = 0; i < o->style_cnt; i++) { const char *n = fx::style_name(o->styles[i].style); if (n && !strcmp(n, name)) return true; }
  return false;
}
void lab_box(ArduinoJson::JsonArray arr, const lv_area_t &a, const std::string &why) {
  auto r = arr.add<ArduinoJson::JsonObject>();
  r["x"] = a.x1; r["y"] = a.y1; r["w"] = lv_area_get_width(&a); r["h"] = lv_area_get_height(&a); r["why"] = why;
}
std::string lab_name(lv_obj_t *o) {
  if (lv_obj_check_type(o, &lv_label_class)) return std::string("text \"") + lv_label_get_text(o) + "\"";
  for (uint32_t i = 0; i < o->style_cnt; i++) if (const char *n = fx::style_name(o->styles[i].style)) return std::string("[") + n + "]";
  return "object";
}
bool qa_clips_on_purpose(lv_obj_t *o);  // fw::qa::clips(o, why): o clips its children on purpose (below)
void lab_walk(lv_obj_t *o, bool in_body, ArduinoJson::JsonArray bounds, ArduinoJson::JsonArray trunc, std::vector<lv_obj_t *> &clicks) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    const bool body = in_body || lab_has(c, "body");
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    if (lv_obj_check_type(c, &lv_label_class)) { const int32_t ty = lv_obj_get_style_translate_y(c, LV_PART_MAIN); a.y1 -= ty; a.y2 -= ty; }  // text-box translate is not layout
    if (in_body && !lab_has(c, "bleed") && fw::sheet::panel(o) != c) {  // a sheet's panel overhangs the screen edges on purpose (core/sheet.h)
      lv_area_t p, bb;
      lv_obj_get_content_coords(o, &p);
      lv_obj_get_coords(o, &bb);
      // the same rules as the audit's bounds proof (core/test_hooks.cpp): a floating mark inside its parent's border
      // box (corner ticks, a toast's timer bar) is inside; a parent that clips on purpose (qa::clips: a sparkline's
      // track one slot past its box for the shift) shows only its own box
      const bool edge_mark = lv_obj_has_flag(c, LV_OBJ_FLAG_IGNORE_LAYOUT) && a.x1 >= bb.x1 && a.y1 >= bb.y1 && a.x2 <= bb.x2 && a.y2 <= bb.y2;
      if (!edge_mark && !qa_clips_on_purpose(o) && (a.x1 < p.x1 || a.y1 < p.y1 || a.x2 > p.x2 || a.y2 > p.y2))
        lab_box(bounds, a, lab_name(c) + " " + std::to_string(lv_area_get_width(&a)) + "x" + std::to_string(lv_area_get_height(&a)) +
                " outside its parent's content box " + std::to_string(lv_area_get_width(&p)) + "x" + std::to_string(lv_area_get_height(&p)));
    }
    if (lv_obj_check_type(c, &lv_label_class)) {
      const char *t = lv_label_get_text(c);
      lv_point_t sz;
      lv_text_get_size(&sz, t, lv_obj_get_style_text_font(c, LV_PART_MAIN), lv_obj_get_style_text_letter_space(c, LV_PART_MAIN),
                       lv_obj_get_style_text_line_space(c, LV_PART_MAIN), LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      const int cw = lv_obj_get_content_width(c);
      if (t && *t && sz.x > cw) lab_box(trunc, a, lab_name(c) + " needs " + std::to_string(sz.x) + " px, has " + std::to_string(cw));
    }
    if (body && lv_obj_has_flag(c, LV_OBJ_FLAG_CLICKABLE)) clicks.push_back(c);
    lab_walk(c, body, bounds, trunc, clicks);
  }
}
bool lab_inside(lv_obj_t *o, lv_obj_t *anc) { for (; o; o = lv_obj_get_parent(o)) if (o == anc) return true; return false; }
}  // namespace
EMSCRIPTEN_KEEPALIVE const char *fw_lab(int id, const char *cmd, const char *arg) {
  out.clear();
  const std::string c = cmd, a = arg ? arg : "";
  if (!c.compare(0, 6, "theme_")) { out = fx::theme_lab(c, a); return out.c_str(); }  // theme files (core/theme_lab.cpp)
  if (!c.compare(0, 5, "font_")) { out = fx::font_lab(c, a); return out.c_str(); }    // downloaded fonts (core/font_lab.cpp)
  if (!c.compare(0, 4, "pen_")) { Stage *s = get(id); out = fx::pen_lab(s ? s->d : lv_display_get_default(), c, a); return out.c_str(); }  // snapped drawing probe (core/pen_lab.cpp)
  if (c == "tokens") { out = fx::tunables(); return out.c_str(); }
  if (c == "tune") {
    const size_t eq = a.find('=');
    const std::string n = a.substr(0, eq);
    fx::tune(n.c_str(), eq == std::string::npos || eq + 1 >= a.size() ? -1 : atoi(a.c_str() + eq + 1));
    for (Stage *s : stages) if (s && s->d) { lv_obj_update_layout(lv_display_get_screen_active(s->d)); lv_obj_invalidate(lv_display_get_screen_active(s->d)); }
    return "";
  }
  if (c == "uses") { Stage *s = get(id); out = s ? fx::uses(s->d) : "[]"; if (s) { s->areas.assign({0, 0, s->w - 1, s->h - 1}); s->dirty = true; } return out.c_str(); }
  if (c == "probe") {  // which size tokens change stage id's pixels: {"name": 1 | 0, ..., "squeeze": 1 | 0}; arg = the squeeze now
    Stage *s = get(id);
    if (!s) return "{}";
    lv_obj_t *scr = lv_display_get_screen_active(s->d);
    auto shot = [&] {
      lv_obj_update_layout(scr); lv_obj_invalidate(scr); lv_refr_now(s->d);
      uint64_t h = 1469598103934665603ull;
      for (uint32_t p : s->rgba) h = (h ^ p) * 1099511628211ull;
      return h;
    };
    ArduinoJson::JsonDocument all, doc;
    deserializeJson(all, fx::tunables());
    const uint64_t base = shot();
    for (ArduinoJson::JsonVariant t : all.as<ArduinoJson::JsonArray>()) {
      const std::string n = t[0].as<const char *>();
      const int b = t[1], now = t[2], step = std::max(4, now / 2);
      fx::tune(n.c_str(), now + step);  // a look size clamps to its range: then the other way too
      bool hit = shot() != base;
      if (!hit) { fx::tune(n.c_str(), std::max(0, now - step)); hit = shot() != base; }
      fx::tune(n.c_str(), now == b ? -1 : now);
      doc[n] = hit ? 1 : 0;
    }
    const int sq = a.empty() ? -1 : atoi(a.c_str());
    fx::tune("squeeze", s->w / 2);
    doc["squeeze"] = shot() != base ? 1 : 0;
    fx::tune("squeeze", sq > 0 ? sq : -1);
    shot();
    s->areas.assign({0, 0, s->w - 1, s->h - 1});  // the host copies the stage once, not every probe frame
    s->dirty = true;
    serializeJson(doc, out);
    return out.c_str();
  }
  if (c != "proof") return "tokens | tune | uses | probe | proof | theme_*";
  Stage *s = get(id);
  if (!s) return "{}";
  lv_obj_t *scr = lv_display_get_screen_active(s->d);
  lv_obj_update_layout(scr);
  ArduinoJson::JsonDocument doc;
  auto bounds = doc["bounds"].to<ArduinoJson::JsonArray>(), overlap = doc["overlap"].to<ArduinoJson::JsonArray>(),
       trunc = doc["trunc"].to<ArduinoJson::JsonArray>();
  std::vector<lv_obj_t *> clicks;
  // a component mounted alone (no Page on the stage) is all "body": the stage's content box is its parent
  lv_obj_t *first = lv_obj_get_child(scr, 0);
  lab_walk(scr, first && !lab_has(first, "page"), bounds, trunc, clicks);
  for (size_t i = 0; i < clicks.size(); i++)
    for (size_t j = i + 1; j < clicks.size(); j++) {
      if (lab_inside(clicks[i], clicks[j]) || lab_inside(clicks[j], clicks[i])) continue;
      lv_area_t p, q, r;
      lv_obj_get_click_area(clicks[i], &p);
      lv_obj_get_click_area(clicks[j], &q);
      r.x1 = LV_MAX(p.x1, q.x1); r.y1 = LV_MAX(p.y1, q.y1); r.x2 = LV_MIN(p.x2, q.x2); r.y2 = LV_MIN(p.y2, q.y2);
      if (r.x1 <= r.x2 && r.y1 <= r.y2) lab_box(overlap, r, lab_name(clicks[i]) + " and " + lab_name(clicks[j]) + " touch areas overlap");
    }
  serializeJson(doc, out);
  return out.c_str();
}

// resize a stage (display + buffers), keeps its id
EMSCRIPTEN_KEEPALIVE void fw_stage_resize(int id, int w, int h) {
  Stage *s = get(id);
  if (!s || (s->w == w && s->h == h)) return;
  s->w = w;
  s->h = h;
  s->rgba.assign((size_t) w * h, 0xFF000000u);
  size_t n = ((size_t) w * h * 2 + 63) & ~(size_t) 63;
  uint8_t *nb = (uint8_t *) std::aligned_alloc(64, n);
  lv_display_set_buffers(s->d, nb, nullptr, n, LV_DISPLAY_RENDER_MODE_PARTIAL);
  std::free(s->buf);
  s->buf = nb;
  lv_display_set_resolution(s->d, w, h);
}

EMSCRIPTEN_KEEPALIVE void fw_touch(int id, int x, int y, int down) {
  if (Stage *s = get(id)) { s->pos = {x, y}; s->down = down; }
}
EMSCRIPTEN_KEEPALIVE void fw_tick(uint32_t ms) {
  now_ms = ms;
  lv_timer_handler();
}
// LVGL animations running now (the browser targets' frame stats count frames only while this is > 0, like /fw/perf)
EMSCRIPTEN_KEEPALIVE int fw_anims() { return lv_anim_count_running(); }
EMSCRIPTEN_KEEPALIVE int fw_dirty(int id) {
  Stage *s = get(id);
  if (!s || !s->dirty) return 0;
  s->dirty = false;
  return 1;
}
// the areas flushed since the last call: [n, x1, y1, x2, y2, ...] (int32; the host copies only these to its canvas
// instead of the whole stage every frame); cleared by the call
EMSCRIPTEN_KEEPALIVE const int32_t *fw_areas(int id) {
  static std::vector<int32_t> out;
  Stage *s = get(id);
  out.assign(1, 0);
  if (!s) return out.data();
  out[0] = (int32_t) s->areas.size() / 4;
  out.insert(out.end(), s->areas.begin(), s->areas.end());
  s->areas.clear();
  return out.data();
}
EMSCRIPTEN_KEEPALIVE const uint32_t *fw_frame(int id) {
  Stage *s = get(id);
  return s ? s->rgba.data() : nullptr;
}

// ---- component audit (core/audit.h, tools/audit.mjs)
}  // extern "C"
namespace {
struct Mark {
  std::string free_why, clip_why;
  bool circle = false, inside = false;
  int axes = 0, depth = 0;
  lv_obj_t *ref = nullptr;
  float cx = 0, cy = 0, r = 0;
};
std::map<const lv_obj_t *, Mark> marks;
}  // namespace
extern "C" bool qa_clips_on_purpose(lv_obj_t *o) { auto it = marks.find(o); return it != marks.end() && !it->second.clip_why.empty(); }
namespace {
Mark &mark(lv_obj_t *o) {
  auto it = marks.find(o);
  if (it == marks.end()) {
    lv_obj_add_event_cb(o, [](lv_event_t *e) { marks.erase((const lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
    it = marks.emplace(o, Mark{}).first;
  }
  return it->second;
}
const char *type_of(const lv_obj_t *o) {
  if (lv_obj_check_type(o, &lv_label_class)) return "label";
  if (lv_obj_check_type(o, &lv_image_class)) return "image";
  if (lv_obj_check_type(o, &lv_arc_class)) return "arc";
  if (lv_obj_check_type(o, &lv_line_class)) return "line";
  return "obj";
}
bool scrolls(lv_obj_t *o) {
  return lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && (lv_obj_get_scroll_top(o) > 0 || lv_obj_get_scroll_bottom(o) > 0 ||
                                                        lv_obj_get_scroll_left(o) > 0 || lv_obj_get_scroll_right(o) > 0);
}
// a label's glyph ink that falls below its box (LVGL clips label text at the box bottom): px cut, 0 = none
int label_cut(lv_obj_t *l, int *right) {
  *right = 0;
  const lv_font_t *f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
  const char *t = lv_label_get_text(l);
  if (!f || !t || !*t) return 0;
  lv_area_t a, c;
  lv_obj_get_coords(l, &a);
  lv_obj_get_content_coords(l, &c);
  const int lh = lv_font_get_line_height(f), asc = lh - f->base_line;
  const int pitch = lh + lv_obj_get_style_text_line_space(l, LV_PART_MAIN);
  int line = 0, cut = 0;
  uint32_t i = 0;
  lv_font_glyph_dsc_t g;
  while (t[i]) {
    const uint32_t ch = lv_text_encoded_next(t, &i);
    if (ch == '\n') { line++; continue; }
    if (!lv_font_get_glyph_dsc(f, &g, ch, 0) || !g.box_h) continue;
    const int bottom = c.y1 + line * pitch + asc - g.ofs_y;  // the glyph's lowest ink row + 1
    cut = LV_MAX(cut, bottom - (LV_MAX(a.y2, c.y2) + 1));  // LVGL cuts at the content box bottom (fw::text_style)
  }
  if (lv_label_get_long_mode(l) == LV_LABEL_LONG_MODE_CLIP) {  // the INK cut at either side (a glyph's advance may be
    // wider than its box, as the Time's colon cell: its ink is what must fit), the line placed by its text align
    lv_point_t sz;
    const int ls = lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), w = lv_area_get_width(&c);
    lv_text_get_size(&sz, t, f, ls, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_text_align_t al = lv_obj_get_style_text_align(l, LV_PART_MAIN);
    int x = al == LV_TEXT_ALIGN_CENTER ? (w - sz.x) / 2 : al == LV_TEXT_ALIGN_RIGHT ? w - sz.x : 0, lo = 0, hi = 0;
    for (uint32_t k = 0; t[k];) {
      const uint32_t ch = lv_text_encoded_next(t, &k);
      if (lv_font_get_glyph_dsc(f, &g, ch, 0)) {
        if (g.box_w) { lo = LV_MIN(lo, x + g.ofs_x); hi = LV_MAX(hi, x + g.ofs_x + g.box_w - w); }
        x += g.adv_w + ls;
      }
    }
    *right = LV_MAX(0, LV_MAX(hi, -lo));
  }
  return cut;
}
void audit_walk(lv_obj_t *o, int parent, int depth, ArduinoJson::JsonArray arr) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    ArduinoJson::JsonObject j = arr.add<ArduinoJson::JsonObject>();
    const int me = (int) arr.size() - 1;
    j["p"] = parent;
    j["d"] = depth;
    j["x"] = a.x1, j["y"] = a.y1, j["w"] = lv_area_get_width(&a), j["h"] = lv_area_get_height(&a);
    j["t"] = type_of(c);
    j["ext"] = lv_obj_get_ext_draw_size(c);
    j["ov"] = lv_obj_has_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    j["sc"] = scrolls(c);
    j["cc"] = (bool) lv_obj_get_style_clip_corner(c, LV_PART_MAIN) && lv_obj_get_style_radius(c, LV_PART_MAIN) > 0;
    j["opa"] = lv_obj_get_style_opa_recursive(c, LV_PART_MAIN);
    j["clk"] = lv_obj_has_flag(c, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_event_count(c) > 0;
    j["pad"] = c->spec_attr ? c->spec_attr->ext_click_pad : 0;
    j["box"] = lv_obj_get_style_bg_opa(c, LV_PART_MAIN) > LV_OPA_TRANSP || lv_obj_get_style_border_width(c, LV_PART_MAIN) > 0;
    j["bw"] = lv_obj_get_style_border_width(c, LV_PART_MAIN);
    auto hexc = [](lv_color_t k) { char h[8]; snprintf(h, sizeof h, "#%02X%02X%02X", k.red, k.green, k.blue); return std::string(h); };
    if (j["box"].as<bool>()) j["bg"] = hexc(lv_obj_get_style_bg_color(c, LV_PART_MAIN));  // functional tests: filled when on
    for (lv_obj_t *k = c; k; k = lv_obj_get_parent(k))  // drawn disabled / unavailable (fw::st::OFF on it or an ancestor)
      if (lv_obj_has_state(k, fw::st::OFF)) { j["off"] = true; break; }
    if (!strcmp(type_of(c), "label")) {
      int right = 0;
      const int cut = label_cut(c, &right);
      if (cut > 0) j["cut"] = cut;
      if (right > 0) j["cutr"] = right;
      j["text"] = lv_label_get_text(c);
      j["ink"] = hexc(lv_obj_get_style_text_color(c, LV_PART_MAIN));
      if (lv_obj_get_style_text_opa(c, LV_PART_MAIN) < LV_OPA_COVER) j["topa"] = lv_obj_get_style_text_opa(c, LV_PART_MAIN);  // functional: faded text
      if (lv_obj_get_style_text_decor(c, LV_PART_MAIN) & LV_TEXT_DECOR_STRIKETHROUGH) j["strike"] = true;
    }
    auto it = marks.find(c);
    if (it != marks.end()) {
      const Mark &m = it->second;
      if (!m.free_why.empty()) j["free"] = m.free_why;
      if (!m.clip_why.empty()) j["clips"] = m.clip_why;
      if (m.axes) { j["axes"] = m.axes; j["depth"] = m.depth; }
      if (m.circle || m.inside) {
        lv_area_t ra;
        lv_obj_t *ref = m.ref;
        if (ref) lv_obj_get_coords(ref, &ra);
        ArduinoJson::JsonObject pj = j["path"].to<ArduinoJson::JsonObject>();
        pj["kind"] = m.circle ? "circle" : "inside";
        if (m.circle) { pj["cx"] = (ref ? ra.x1 : 0) + m.cx; pj["cy"] = (ref ? ra.y1 : 0) + m.cy; pj["r"] = m.r; }
        if (ref) { pj["rx"] = ra.x1, pj["ry"] = ra.y1, pj["rw"] = lv_area_get_width(&ra), pj["rh"] = lv_area_get_height(&ra); }
      }
    }
    audit_walk(c, me, depth + 1, arr);
  }
}
void unclip_cb(lv_event_t *e) { lv_event_set_ext_draw_size(e, 2048); }
void unclip_walk(lv_obj_t *o) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    auto it = marks.find(c);
    const bool keep = (it != marks.end() && !it->second.clip_why.empty()) || scrolls(c);
    if (!keep) {
      lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
      lv_obj_add_event_cb(c, unclip_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
      lv_obj_refresh_ext_draw_size(c);
      unclip_walk(c);
    }
  }
}
}  // namespace
namespace fw::qa {
void free(lv_obj_t *o, const char *why) { mark(o).free_why = why; }
void clips(lv_obj_t *o, const char *why) { mark(o).clip_why = why; }
void circle(lv_obj_t *o, lv_obj_t *ref, float cx, float cy, float r) { Mark &m = mark(o); m.circle = true, m.ref = ref, m.cx = cx, m.cy = cy, m.r = r; }
void radial(lv_obj_t *o, uint8_t axes, int depth) { Mark &m = mark(o); m.axes = axes, m.depth = depth; }
void inside(lv_obj_t *o, lv_obj_t *ref) { Mark &m = mark(o); m.inside = true, m.ref = ref ? ref : lv_obj_get_parent(o); }
}  // namespace fw::qa
extern "C" {
EMSCRIPTEN_KEEPALIVE const char *fw_audit(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonArray arr = doc.to<ArduinoJson::JsonArray>();
  audit_walk(lv_display_get_screen_active(s->d), -1, 1, arr);
  // the top layer too (a device's Toast layer lives there), only while it shows something: a layer with toasts in it
  lv_obj_t *top = lv_display_get_layer_top(s->d);
  bool any = false;
  for (uint32_t i = 0; i < lv_obj_get_child_count(top) && !any; i++) any = lv_obj_get_child_count(lv_obj_get_child(top, i)) > 0;
  if (any) audit_walk(top, -1, 1, arr);
  serializeJson(doc, out);
  return out.c_str();
}
// draw what is pending now, without moving time (audit: a timer that changed something after this tick's refresh)
EMSCRIPTEN_KEEPALIVE void fw_refresh(int id) {
  if (Stage *s = get(id)) lv_refr_now(s->d);
}
// a full redraw now (audit: what a partial redraw left behind differs from it = stale pixels)
EMSCRIPTEN_KEEPALIVE void fw_redraw(int id) {
  Stage *s = get(id);
  if (!s) return;
  lv_obj_invalidate(lv_display_get_screen_active(s->d));
  lv_refr_now(s->d);
}
EMSCRIPTEN_KEEPALIVE void fw_unclip(int id) {
  Stage *s = get(id);
  if (!s) return;
  lv_obj_t *scr = lv_display_get_screen_active(s->d);
  unclip_walk(scr);
  lv_obj_invalidate(scr);
  lv_refr_now(s->d);  // now, at the same time: no animation moves between the two frames the audit compares
}
}
