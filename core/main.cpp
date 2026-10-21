// Facet WASM workbench host (README.md): LVGL + any number of "stages" (one lv_display each, its
// own size, theme and pointer), so the page can show a big preview and a grid of variants from ONE module.
// Same LVGL 9.5 tree + lv_conf as the panel (build.sh), RGB565 like the glass. No lab chrome, no specs, no HA.
//   fw_init()                         once
//   fw_meta() -> JSON                 every registered component: [{id,name,section,meta}]
//   fw_stage(w, h) -> id              a new display, w x h device pixels
//   fw_stage_free(id)
//   fw_render(id, comp, props, light) build component `comp` with props JSON on stage `id` -> "" or an error
//   fw_touch(id, x, y, down)          pointer on that stage (device pixels)
//   fw_tick(ms)                       LVGL timers/refresh for all stages
//   fw_dirty(id) / fw_frame(id)       redrawn since last read? / RGBA pixels (w*h*4)
#include <emscripten.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "fw.h"

namespace fw {
static lv_color_t H(uint32_t c) { return lv_color_hex(c); }
// @facet/tokens color.* (dark, light)
static const Theme DARK{false, H(0x000000), H(0x0A0B0E), H(0x15171C), H(0x22262D), H(0x3A404A), H(0xF2F1EE), H(0x8A919B),
                        H(0x565C66), H(0xE8542A), H(0xC2441F), H(0x5A2414), H(0x0A0B0E), H(0x9CC3E6), H(0x5DBE7E),
                        H(0xE8B33A), H(0xE5484D)};
static const Theme LIGHT{true, H(0xF4F2EE), H(0xFFFFFF), H(0xE9E6E0), H(0xDDD9D2), H(0xB9B4AB), H(0x16181C), H(0x5C636D),
                         H(0x9A9FA6), H(0xE8542A), H(0xC2441F), H(0xF5CDBF), H(0x16181C), H(0x2F6A9E), H(0x2E8A50),
                         H(0x9A6B00), H(0xC9302C)};
const Theme *T = &DARK;
static std::vector<Component> &reg() {
  static std::vector<Component> r;
  return r;
}
int add(const Component &c) {
  reg().push_back(c);
  return (int) reg().size();
}
}  // namespace fw

namespace {
struct Stage {
  lv_display_t *d = nullptr;
  lv_indev_t *in = nullptr;
  int w = 0, h = 0;
  uint8_t *buf = nullptr;
  std::vector<uint32_t> rgba;
  bool dirty = false, down = false;
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
  lv_display_flush_ready(d);
}
void read(lv_indev_t *i, lv_indev_data_t *data) {
  auto *s = (Stage *) lv_indev_get_user_data(i);
  data->point = s->pos;
  data->state = s->down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void fw_init() {
  lv_init();
  lv_tick_set_cb(tick);
}

EMSCRIPTEN_KEEPALIVE const char *fw_meta() {
  ArduinoJson::JsonDocument doc;
  auto arr = doc.to<ArduinoJson::JsonArray>();
  for (auto &c : fw::reg()) {
    auto o = arr.add<ArduinoJson::JsonObject>();
    o["id"] = c.id;
    o["name"] = c.name;
    o["section"] = c.section;
    o["meta"] = ArduinoJson::serialized(c.meta);
  }
  out.clear();
  serializeJson(doc, out);
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

EMSCRIPTEN_KEEPALIVE const char *fw_render(int id, const char *comp, const char *props, int light) {
  Stage *s = get(id);
  if (!s) return "no such stage";
  const fw::Component *c = nullptr;
  for (auto &r : fw::reg())
    if (!strcmp(r.id, comp)) c = &r;
  if (!c) return "no such component";
  ArduinoJson::JsonDocument doc;
  if (deserializeJson(doc, props && *props ? props : "{}")) return "props: bad JSON";
  fw::T = light ? &fw::LIGHT : &fw::DARK;
  lv_display_set_default(s->d);
  lv_obj_t *scr = lv_display_get_screen_active(s->d);
  lv_obj_clean(scr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, fw::T->bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  c->build(scr, doc.as<ArduinoJson::JsonObjectConst>());
  lv_obj_invalidate(scr);
  return "";
}

EMSCRIPTEN_KEEPALIVE void fw_touch(int id, int x, int y, int down) {
  if (Stage *s = get(id)) { s->pos = {x, y}; s->down = down; }
}
EMSCRIPTEN_KEEPALIVE void fw_tick(uint32_t ms) {
  now_ms = ms;
  lv_timer_handler();
}
EMSCRIPTEN_KEEPALIVE int fw_dirty(int id) {
  Stage *s = get(id);
  if (!s || !s->dirty) return 0;
  s->dirty = false;
  return 1;
}
EMSCRIPTEN_KEEPALIVE const uint32_t *fw_frame(int id) {
  Stage *s = get(id);
  return s ? s->rgba.data() : nullptr;
}
}
