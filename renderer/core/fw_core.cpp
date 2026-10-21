// Facet core shared by every host (README.md): the WASM workbench (core/main.cpp) and the device firmware
// (device/fw_device.h). Themes from the generated token tables, the component registry, and render() = build one
// component on a screen exactly the way the workbench preview does.
#include <cmath>
#include <string>
#include <vector>
#include "fw.h"

namespace fw {
// themes from the generated token tables (tokens/colour.json): [accent set][dark, light], built once
const Theme *theme(bool light, int set) {
  static Theme th[tokens::N_ACCENTS][2];
  static bool ready = false;
  if (!ready) {
    for (int a = 0; a < tokens::N_ACCENTS; a++)
      for (int l = 0; l < 2; l++) {
        Theme &t = th[a][l];
        t.light = l;
        t.accent_set = a;
        for (auto &c : tokens::COLOURS)
          *reinterpret_cast<lv_color_t *>(reinterpret_cast<char *>(&t) + c.off) = lv_color_hex(l ? c.light : c.dark);
        for (int r = 0; r < tokens::N_ROLES; r++)
          *reinterpret_cast<lv_color_t *>(reinterpret_cast<char *>(&t) + tokens::ROLE_OFF[r]) =
              lv_color_hex(l ? tokens::ACCENTS[a].light[r] : tokens::ACCENTS[a].dark[r]);
      }
    ready = true;
  }
  if (set < 0 || set >= tokens::N_ACCENTS) set = tokens::ACCENT_DEFAULT;
  return &th[set][light ? 1 : 0];
}
const Theme *T = nullptr;
std::vector<Component> &reg() {
  static std::vector<Component> r;
  return r;
}
int add(const Component &c) {
  reg().push_back(c);
  return (int) reg().size();
}

const Component *find(const char *id) {
  for (auto &r : reg())
    if (!strcmp(r.id, id)) return &r;
  return nullptr;
}

void meta_json(std::string &out) {
  ArduinoJson::JsonDocument doc;
  auto arr = doc.to<ArduinoJson::JsonArray>();
  for (auto &c : reg()) {
    auto o = arr.add<ArduinoJson::JsonObject>();
    o["id"] = c.id;
    o["name"] = c.name;
    o["section"] = c.section;
    o["meta"] = ArduinoJson::serialized(c.meta);
  }
  serializeJson(doc, out);
}

namespace {
Sink sink = nullptr;
}
void set_sink(Sink s) { sink = s; }
void emit(lv_obj_t *o, const char *name, const char *detail) { if (sink) sink(o, "event", name, detail ? detail : ""); }
void sound(lv_obj_t *o, const char *id) { if (sink) sink(o, "sound", id, ""); }
void sound(lv_obj_t *o, const char *id, float pos) {
  char b[12];
  snprintf(b, sizeof b, "%.3f", LV_CLAMP(0.0f, pos, 1.0f));
  if (sink) sink(o, "sound", id, b);
}

// step ticks (fw.h): a tick limiter shared by every step control
namespace {
constexpr float LANDMARKS[] = {0.0f, 1.0f};  // fractions of the range that are landmark steps (e.g. add 0.5f)
uint32_t last_tick = 0;
bool ticked = false;
lv_timer_t *held = nullptr;  // a landmark waiting out the interval
lv_obj_t *held_obj = nullptr;
float held_pos = 0;
// pos = the step's place in the range (0..1): the host pitches the tick by it (sound.json `track`, Darren 2026-10-21)
void tick_now(lv_obj_t *o, bool landmark, float pos) {
  sound(o, landmark ? "slider-landmark" : "slider-tick", pos);
  // ticks only fire on LVGL frames (16 ms): advance by tick-min while dragging so the average rate is 1 / tick-min,
  // not snapped to every 2nd frame; a pause (> 2 intervals) starts afresh
  if (ticked && lv_tick_elaps(last_tick) < 2 * mo::ms::tick_min) last_tick += mo::ms::tick_min;
  else last_tick = lv_tick_get();
  ticked = true;
}
}  // namespace
bool tick_landmark(int step, int n) {
  for (float f : LANDMARKS) if (step == (int) std::lround(f * n)) return true;
  return false;
}
void step_ticks(lv_obj_t *o, int from, int to, int n) {
  const int dir = to > from ? 1 : -1;
  for (int s = from; s != to;) {
    s += dir;
    const bool lm = tick_landmark(s, n);
    const uint32_t since = lv_tick_elaps(last_tick);
    if (!ticked || since >= mo::ms::tick_min) { if (!held) { tick_now(o, lm, n ? (float) s / n : 0.0f); continue; } }
    if (!lm) continue;  // too soon: this step counts, no sound
    if (held) continue;  // a landmark is already waiting (one per interval)
    held_obj = o;
    held_pos = n ? (float) s / n : 0.0f;
    held = lv_timer_create([](lv_timer_t *t) {
      lv_obj_t *ho = held_obj;
      held = nullptr;
      lv_timer_delete(t);
      if (ho && lv_obj_is_valid(ho)) tick_now(ho, true, held_pos);
    }, mo::ms::tick_min - since, nullptr);
    lv_timer_set_repeat_count(held, 1);
  }
}

// bleed (fw.h): overflow visible + ext draw size on o and every ancestor (each object keeps the largest it was asked)
void bleed(lv_obj_t *o, int px) {
  for (lv_obj_t *x = o; x; x = lv_obj_get_parent(x)) {
    lv_obj_add_flag(x, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(x, [](lv_event_t *e) { lv_event_set_ext_draw_size(e, (int32_t) (intptr_t) lv_event_get_user_data(e)); },
                        LV_EVENT_REFR_EXT_DRAW_SIZE, (void *) (intptr_t) px);
    lv_obj_refresh_ext_draw_size(x);
  }
}

// clean `scr`, make it the Facet screen (bg token, flex column, centred) and build `comp` with `props` on it
const char *render(lv_obj_t *scr, const char *comp, const char *props, bool light, int accent) {
  const Component *c = find(comp);
  if (!c) return "no such component";
  ArduinoJson::JsonDocument doc;
  if (deserializeJson(doc, props && *props ? props : "{}")) return "props: bad JSON";
  T = theme(light, accent);
  // reduced motion for this display: props "reduced_motion" (workbench playground / Show on device), else the panel's
  mo::set_reduced(lv_obj_get_display(scr), doc["reduced_motion"] | mo::reduced_default());
  lv_obj_clean(scr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, T->bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  c->build(scr, doc.as<ArduinoJson::JsonObjectConst>());
  lv_obj_update_layout(scr);
  lv_obj_invalidate(scr);
  return "";
}

// ---- type: the text-box rule (caps centred in the style's line box) ----
namespace {
int cap_px(const lv_font_t *f) {  // cap height above the baseline, measured from the font ('H', else '0')
  lv_font_glyph_dsc_t g;
  for (uint32_t c : {(uint32_t) 'H', (uint32_t) '0'})
    if (lv_font_get_glyph_dsc(f, &g, c, 0) && g.box_h > 0) return g.box_h + g.ofs_y;
  return (int) lroundf(lv_font_get_line_height(f) * 0.6f);
}
uint32_t utf8_next(const char *&p) {
  auto b = (unsigned char) *p++;
  if (b < 0x80) return b;
  int n = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : 1;
  uint32_t c = b & (0x3F >> n);
  while (n-- && (*p & 0xC0) == 0x80) c = c << 6 | (*p++ & 0x3F);
  return c;
}
void utf8_put(std::string &o, uint32_t c) {
  if (c < 0x80) o += (char) c;
  else if (c < 0x800) { o += (char) (0xC0 | c >> 6); o += (char) (0x80 | (c & 0x3F)); }
  else if (c < 0x10000) { o += (char) (0xE0 | c >> 12); o += (char) (0x80 | (c >> 6 & 0x3F)); o += (char) (0x80 | (c & 0x3F)); }
  else { o += (char) (0xF0 | c >> 18); o += (char) (0x80 | (c >> 12 & 0x3F)); o += (char) (0x80 | (c >> 6 & 0x3F)); o += (char) (0x80 | (c & 0x3F)); }
}
}  // namespace

std::string cased(ty::Id s, const char *txt) {
  if (!txt) return "";
  if (!ty::STYLES[s].upper) return txt;
  std::string o;
  for (const char *p = txt; *p;) {
    uint32_t c = utf8_next(p);
    if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) c -= 32;  // ASCII + Latin-1 letters
    else if (c == 0xFF) c = 0x178;
    utf8_put(o, c);
  }
  return o;
}

void text_style(lv_obj_t *l, ty::Id s) {
  const ty::Style &st = ty::STYLES[s];
  const lv_font_t *f = font(s);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_letter_space(l, st.letter_space, 0);
  const int lh = lv_font_get_line_height(f), asc = lh - f->base_line;
  lv_obj_set_style_text_line_space(l, st.box - lh, 0);  // wrapped lines on the box pitch
  const int off = (int) lroundf((st.box + cap_px(f)) / 2.0f) - asc;  // top of the line box -> caps centred in the box
  lv_obj_set_style_pad_top(l, off > 0 ? off : 0, 0);
  lv_obj_set_style_translate_y(l, off < 0 ? off : 0, 0);
  if (lv_label_get_long_mode(l) != LV_LABEL_LONG_WRAP) lv_obj_set_height(l, st.box);
}

lv_obj_t *text(lv_obj_t *parent, ty::Id s, const char *txt, lv_color_t c, int wrap_w) {
  lv_obj_t *l = lv_label_create(parent);
  if (wrap_w > 0) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, wrap_w);
  }
  text_style(l, s);
  lv_obj_set_style_text_color(l, c, 0);
  lv_label_set_text(l, cased(s, txt).c_str());
  return l;
}

void set_text(lv_obj_t *l, ty::Id s, const char *txt) { lv_label_set_text(l, cased(s, txt).c_str()); }

}  // namespace fw
