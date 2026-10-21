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
//   fw_events() -> JSON               events + sound requests since the last call (fw::emit / fw::sound): [{stage,kind,name,detail}] or ""
#include <emscripten.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "fw.h"


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

// the colour tokens file (JSON) the build was generated from
EMSCRIPTEN_KEEPALIVE const char *fw_tokens() { return fw::tokens::JSON; }

// height in device px the last build of stage `id` needs (bottom of its lowest child + the screen's bottom padding):
// lets the page size an "auto height" stage (meta "frame": [w, 0]) to its content
EMSCRIPTEN_KEEPALIVE int fw_content_h(int id) {
  Stage *s = get(id);
  if (!s) return 0;
  lv_obj_t *scr = lv_display_get_screen_active(s->d);
  int32_t y = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_area_t a;
    lv_obj_get_coords(lv_obj_get_child(scr, i), &a);
    if (a.y2 + 1 > y) y = a.y2 + 1;
  }
  return y + lv_obj_get_style_pad_bottom(scr, LV_PART_MAIN);
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
