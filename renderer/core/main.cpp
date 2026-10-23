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
// LVGL private (src/core/lv_obj_draw_private.h): the ext draw size an object holds (fw::bleed sets it)
extern "C" int32_t lv_obj_get_ext_draw_size(const lv_obj_t *obj);
#include <map>
#include "core/lv_obj_private.h"  // ext_click_pad (audit)
#include "misc/lv_text_private.h"  // lv_text_encoded_next (audit)


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
EMSCRIPTEN_KEEPALIVE void fw_ha_put(const char *e, const char *a, const char *v) { fw::ha::put(e ? e : "", a ? a : "", v ? v : ""); }

// Screen specs (core/spec.h): fw_spec(cmd, arg) -> JSON. cmd: check (a screen or deck spec: the errors, the
// components it uses, nodes, binds), screen / deck (apply: validate, build once off screen, store, swap), revert
// (arg = id | deck | all), hello, list (every live spec). Storage here = memory (fw_spec_storage: fault injection +
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
  if (c == "check") {
    ArduinoJson::JsonDocument s;
    fw::spec::Errs e;
    if (deserializeJson(s, a)) fw::spec::err(e, "", "bad_json", "not JSON");
    else if (s["screens"].is<ArduinoJson::JsonArray>()) fw::spec::check_deck(s.as<ArduinoJson::JsonVariantConst>(), a.size(), e);
    else fw::spec::check_screen(s.as<ArduinoJson::JsonVariantConst>(), a.size(), e);
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
  } else if (c == "screen" || c == "deck") {
    lv_display_t *disp = lv_display_get_default();
    lv_obj_t *dry = disp ? lv_obj_create(lv_display_get_screen_active(disp)) : nullptr;
    if (dry) { lv_obj_add_flag(dry, LV_OBJ_FLAG_HIDDEN); }
    fw::spec::apply(c.c_str(), a, dry, o);
    if (dry) lv_obj_delete(dry);
  } else if (c == "revert") fw::spec::revert(a.empty() ? "all" : a, o);
  else if (c == "hello") fw::spec::hello(o);
  else if (c == "list") {
    for (const auto &f : fw::spec::FACTORY) o["screens"][f.id] = fw::spec::screen_json(f.id);
    for (const auto &kv : fw::spec::S().over) o["screens"][kv.first] = kv.second;
    o["deck"] = fw::spec::deck_json();
  } else { o["ok"] = false; o["error"] = "check | screen | deck | revert | hello | list"; }
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
EMSCRIPTEN_KEEPALIVE const char *fw_rects(int id) {
  out.clear();
  Stage *s = get(id);
  if (!s) return "[]";
  ArduinoJson::JsonDocument doc;
  rects(lv_display_get_screen_active(s->d), 1, doc.to<ArduinoJson::JsonArray>());
  serializeJson(doc, out);
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
  if (lv_label_get_long_mode(l) == LV_LABEL_LONG_MODE_CLIP) {
    lv_point_t sz;
    lv_text_get_size(&sz, t, f, lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    *right = LV_MAX(0, sz.x - lv_area_get_width(&c));
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
    if (!strcmp(type_of(c), "label")) {
      int right = 0;
      const int cut = label_cut(c, &right);
      if (cut > 0) j["cut"] = cut;
      if (right > 0) j["cutr"] = right;
      j["text"] = lv_label_get_text(c);
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
  audit_walk(lv_display_get_screen_active(s->d), -1, 1, doc.to<ArduinoJson::JsonArray>());
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
