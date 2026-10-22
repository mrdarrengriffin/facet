// Light (first HA slice, 2026-10-22, WIP): one light, live from Home Assistant (core/ha.h). Faithful first port of
// the Astro Light screen (@facet/ui screens/light.ts): the Header (back + the light's name) and the Dial centred in the
// content box (lay::dial_scale square; content box = lay::top to the bottom inset; snapped to the 4 px grid).
// One spec serves every light: props entity + name (the Living room opens it with "navigate" light?{entity, name}).
// The Dial is bound: value = brightness 0-100 ({"a": "brightness", "fmt": "pct"}; off = 0), on = the state, unavailable
// when HA says so; a change anywhere moves it through the Dial update path.
// Dial events -> HA (ha::act, closed whitelist): "change" v (on release) = light.turn_on brightness_pct v (0 =
// light.turn_off); "toggle" on / off = light.turn_on / light.turn_off. Instant feedback: the Dial already shows the
// new value; store updates are held off it (3 s after a drag, as the Astro screen, or until HA reports the new state
// for a toggle, 5 s at most), then it shows what HA says (back where it was if HA refused).
// Live brightness (Darren 2026-10-22): while the finger drags, the Dial's "input" values go out as a throttle with a
// trailing send: at most one call every LIGHT_THROTTLE_MS (light-throttle-ms, 250), only the latest value kept while
// waiting, sent when the interval is up; the release always sends its value at once; a value equal to the last one
// sent is never sent. HA's updates are held off the Dial from the first move until HA reports the released value (or
// TOGGLE_HOLD_MS pass: HA's state again). On / off = one send each.
#include "fw.h"
#include "ha.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using ArduinoJson::JsonObject;
constexpr uint32_t LIGHT_THROTTLE_MS = 250;  // light-throttle-ms: live brightness while dragging, at most this often
constexpr uint32_t DRAG_HOLD_MS = 5000, TOGGLE_HOLD_MS = 5000;  // drag: renewed on every move

struct Screen {
  lv_obj_t *root = nullptr, *dial = nullptr;
  std::string e;
  int sent = -1, want = -1;  // the last brightness sent (0 = off), the latest waiting for the interval
  uint32_t sent_at = 0;
  lv_timer_t *trail = nullptr;
  int calls = 0;  // brightness calls this screen made (log)
  bool drag_sent = false;  // something went out during this drag (a drag starting at 0 sends no 0 = off)
};
constexpr int ON_ONLY = 1000;  // want: switch on (the Dial's "toggle on"), unless a brightness from the drag replaces it
// one brightness call: 0 = off; never the value sent last
void send(Screen *s, int v, lv_obj_t *from) {
  if (v == s->sent) return;
  char data[40];
  snprintf(data, sizeof data, "{\"brightness_pct\":%d}", v);
  if (*(v > 0 ? fw::ha::act(from, "light.turn_on", s->e.c_str(), data) : fw::ha::act(from, "light.turn_off", s->e.c_str()))) return;
  s->sent = v;
  s->sent_at = lv_tick_get();
  s->calls++;
}
void trail_tick(lv_timer_t *t) {
  auto *s = (Screen *) lv_timer_get_user_data(t);
  s->trail = nullptr;  // one-shot
  if (s->want == ON_ONLY) {
    s->want = -1;
    if (!*fw::ha::act(s->dial, "light.turn_on", s->e.c_str())) { s->sent = -1; s->sent_at = lv_tick_get(); s->calls++; }
    fw::ha::hold(s->dial, TOGGLE_HOLD_MS, s->e, "", "on");
    return;
  }
  if (s->want >= 0) { send(s, s->want, s->dial); s->want = -1; }
}
// a live value while dragging: now if the interval is up, else the latest waits for it
void live(Screen *s, int v) {
  fw::ha::hold(s->dial, DRAG_HOLD_MS);  // the finger owns the Dial
  if (v == 0 && !s->drag_sent) return;  // the drag starts at 0 (off): nothing to send yet
  s->drag_sent = true;
  const uint32_t el = lv_tick_elaps(s->sent_at);
  if (s->want == ON_ONLY || s->sent < 0 || el >= LIGHT_THROTTLE_MS) { s->want = -1; send(s, v, s->dial); return; }
  s->want = v;
  if (!s->trail) {
    s->trail = lv_timer_create(trail_tick, LIGHT_THROTTLE_MS - el, s);
    lv_timer_set_repeat_count(s->trail, 1);
  }
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
bool inside(lv_obj_t *o, lv_obj_t *in) {
  for (; o; o = lv_obj_get_parent(o))
    if (o == in) return true;
  return false;
}
void on_event(lv_obj_t *from, const char *name, const char *detail, void *user) {
  auto *s = (Screen *) user;
  const bool change = !strcmp(name, "change"), flip = !strcmp(name, fw::st::SWITCH_EVENT), in = !strcmp(name, "input");
  if (!s->dial || !inside(from, s->dial) || !(change || flip || in)) {
    if (lv_obj_t *up = lv_obj_get_parent(s->root)) fw::notify(up, name, detail);
    return;
  }
  if (in) { live(s, (int) std::lround(atof(detail))); return; }
  if (flip) {  // on / off: one send; 0 released = the "change" 0 already turned it off
    const bool on = !strcmp(detail, "on");
    if (on) {  // a moment later: a drag that switched it on sends its brightness instead (one call)
      s->want = ON_ONLY;
      if (!s->trail) {
        s->trail = lv_timer_create(trail_tick, 300, s);  // a ring tap or a drag replaces it before then
        lv_timer_set_repeat_count(s->trail, 1);
      }
      return;
    }
    if (s->sent == 0) return;  // the released 0 already turned it off
    if (!*fw::ha::act(from, "light.turn_off", s->e.c_str())) {
      s->sent = 0;
      s->calls++;
      fw::ha::hold(s->dial, TOGGLE_HOLD_MS, s->e, "", "off");
    }
    return;
  }
  // released: the final value at once (the trailing one is dropped), then settle on HA's confirmation
  if (s->trail) { lv_timer_delete(s->trail); s->trail = nullptr; }
  s->want = -1;
  s->drag_sent = false;
  const int v = (int) std::lround(atof(detail));
  send(s, v, from);
  if (v > 0) fw::ha::hold(s->dial, TOGGLE_HOLD_MS, s->e, "brightness", std::to_string(v), true);
  else fw::ha::hold(s->dial, TOGGLE_HOLD_MS, s->e, "", "off");
  fw::emit(from, "light-calls", std::to_string(s->calls).c_str());  // calls this screen made so far (bench / log)
}

const char *META = R"({
  "about": "One light, live from Home Assistant: the Dial sets its brightness, the button switches it on and off.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "entity", "type": "select", "options": ["light.living_room_dimmer_light", "light.sofa_table_light", "light.chair_light"], "default": "light.living_room_dimmer_light"},
    {"name": "name", "type": "text", "default": "Ceiling"}
  ],
  "variants": [{"label": "ceiling", "props": {}}]
})";

void build_light(lv_obj_t *parent, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *s = new Screen;
  s->e = p["entity"] | "light.living_room_dimmer_light";
  s->root = box(parent);
  lv_obj_set_size(s->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(s->root, t.bg, 0);
  lv_obj_set_style_bg_opa(s->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(s->root, [](lv_event_t *e) {
    auto *s = (Screen *) lv_event_get_user_data(e);
    if (s->trail) lv_timer_delete(s->trail);
    delete s;
  }, LV_EVENT_DELETE, s);
  fw::listen(s->root, on_event, s);

  Doc hd;
  hd["label"] = p["name"] | "Light";
  if (const fw::Component *h = fw::find("header")) {
    h->build(s->root, hd.as<fw::Props>());
    lv_obj_set_pos(lv_obj_get_child(s->root, -1), fw::lay::inset, fw::lay::inset);
  }
  // the Dial in the content box (lay::top .. screen_h - inset), centred, on the 4 px grid
  const int S = fw::lay::dial_scale, ch = fw::lay::screen_h - fw::lay::top - fw::lay::inset;
  lv_obj_t *holder = box(s->root);
  lv_obj_set_size(holder, S, S);
  lv_obj_set_pos(holder, (fw::lay::screen_w - S) / 2 / 4 * 4, (fw::lay::top + (ch - S) / 2) / 4 * 4);
  Doc d;
  d["caption"] = "Brightness";
  d["unit"] = "%";
  JsonObject v = d["value"].to<JsonObject>();
  v["e"] = s->e;
  v["a"] = "brightness";
  v["fmt"] = "pct";
  JsonObject on = d["on"].to<JsonObject>();
  on["e"] = s->e;
  on["fmt"] = "on";
  JsonObject st = d["state"].to<JsonObject>();
  st["e"] = s->e;
  st["fmt"] = "avail";
  s->dial = fw::ha::bound(holder, "dial", d.as<fw::Props>());
}
const int reg = fw::add({"light", "Light", "screens", META, build_light});
}  // namespace
