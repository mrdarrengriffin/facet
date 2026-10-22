// Living room (first HA slice, 2026-10-22, WIP): a room's lights, live from Home Assistant (core/ha.h). Composed of
// real components only:
//   header   the Header (back + the room name + the screen chrome) at (inset, inset)
//   content  from lay::top, one scrolling column of List rows, one per light (rows touch: the hairlines divide them)
//   row      bound to the light (ha::bound): title = the light's name, icon bulb, sub = "Off" / "On" / "On · 64%"
//            ({"e", "fmt": "light"}), trailing toggle = its state ({"fmt": "on"}), unavailable when HA says so
// Tap a row = toggle the light (the List row hands the tap to its Toggle): the toggle slides at once, the sub says
// "Turning on" / "Turning off" while HA hasn't confirmed (ha::hold 5 s), then the row shows what HA reports (back
// where it was if HA refused or never answered). Long-press a row = open the Light screen for it ("navigate"
// light?{entity, name}: the device runtime builds the one Light screen with those props).
// The lights are the room's spec below (entity + name); HA data comes from the store, so the rows repaint through the
// List row update path when a light changes anywhere (the wall switch, HA, the other panel), never a rebuild.
#include "fw.h"
#include "ha.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using ArduinoJson::JsonObject;

struct Light { const char *e, *name; };
constexpr Light LIGHTS[] = {
    {"light.living_room_dimmer_light", "Ceiling"},
    {"light.sofa_table_light", "Sofa table"},
    {"light.chair_light", "Chair"},
};
constexpr int N = sizeof LIGHTS / sizeof LIGHTS[0];
constexpr uint32_t PENDING_MS = 5000;  // how long a toggle waits for HA before it shows HA's state again

// the panel subscribes to these at boot (ha::want), so the room paints with real values when it opens
const int pre = [] {
  for (const Light &l : LIGHTS) { fw::ha::want(l.e, ""); fw::ha::want(l.e, "brightness"); }
  return 0;
}();

struct Room {
  lv_obj_t *root = nullptr;
  lv_obj_t *rows[N] = {};
  int8_t pending[N] = {};      // per light: 1 turning on, -1 turning off, 0 none (shown after the toggle's slide)
  lv_timer_t *timer = nullptr;  // that repaint (a row refill inside the toggle's own tap would delete it mid-event)
};

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void row_spec(const Light &l, Doc &d) {
  d["title"] = l.name;
  d["icon"] = "bulb";
  d["trailing"] = "toggle";
  JsonObject sub = d["sub"].to<JsonObject>();
  sub["e"] = l.e;
  sub["fmt"] = "light";
  JsonObject on = d["on"].to<JsonObject>();
  on["e"] = l.e;
  on["fmt"] = "on";
  JsonObject st = d["state"].to<JsonObject>();
  st["e"] = l.e;
  st["fmt"] = "avail";
}
int row_index(Room *r, lv_obj_t *o) {
  for (; o; o = lv_obj_get_parent(o))
    for (int i = 0; i < N; i++)
      if (r->rows[i] == o) return i;
  return -1;
}

void show_pending(lv_timer_t *t) {
  auto *r = (Room *) lv_timer_get_user_data(t);
  r->timer = nullptr;  // one-shot: LVGL deletes it after this call
  for (int i = 0; i < N; i++) {
    if (!r->pending[i]) continue;
    const bool on = r->pending[i] > 0;
    r->pending[i] = 0;
    if (!r->rows[i] || !fw::ha::held(r->rows[i])) continue;  // HA already answered
    Doc spec, p;
    row_spec(LIGHTS[i], spec);
    fw::ha::resolve(spec.as<fw::Props>(), p, nullptr);
    p["on"] = on;
    p["sub"] = on ? "Turning on" : "Turning off";
    fw::Quiet q;
    fw::update_child(fw::find("list_row"), r->rows[i], p.as<fw::Props>());
  }
}
void on_event(lv_obj_t *from, const char *name, const char *detail, void *user) {
  auto *r = (Room *) user;
  const int i = !strcmp(name, fw::st::SWITCH_EVENT) ? row_index(r, from) : -1;
  if (i < 0) {  // not a light's toggle: on up to the device runtime (the Header's back)
    if (lv_obj_t *up = lv_obj_get_parent(r->root)) fw::notify(up, name, detail);
    return;
  }
  const Light &l = LIGHTS[i];
  const bool on = !strcmp(detail, "on");
  if (*fw::ha::act(from, on ? "light.turn_on" : "light.turn_off", l.e)) return;
  // pending: the toggle already moved (its slide); the sub says so once the slide is done, until HA confirms (or the
  // hold runs out: HA's state again)
  r->pending[i] = on ? 1 : -1;
  if (!r->timer) {
    r->timer = lv_timer_create(show_pending, fw::mo::duration(r->rows[i], fw::st::SWITCH, fw::mo::MOVE) + 20, r);
    lv_timer_set_repeat_count(r->timer, 1);
  }
  fw::ha::hold(r->rows[i], PENDING_MS, l.e, "", on ? "on" : "off");
}

const char *META = R"({
  "about": "A room's lights, live from Home Assistant: tap a row to switch the light, long-press it to open the Light screen.",
  "frame": [480, 480],
  "icons": ["back@glyph", "bulb@icon"],
  "props": [],
  "variants": [{"label": "living room", "props": {}}]
})";

void build_room(lv_obj_t *parent, fw::Props) {
  const fw::Theme &t = *fw::T;
  auto *r = new Room;
  r->root = box(parent);
  lv_obj_set_size(r->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(r->root, t.bg, 0);
  lv_obj_set_style_bg_opa(r->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(r->root, [](lv_event_t *e) {
    auto *r = (Room *) lv_event_get_user_data(e);
    if (r->timer) lv_timer_delete(r->timer);
    delete r;
  }, LV_EVENT_DELETE, r);
  fw::listen(r->root, on_event, r);

  Doc hd;
  hd["label"] = "Living room";
  if (const fw::Component *h = fw::find("header")) {
    h->build(r->root, hd.as<fw::Props>());
    lv_obj_set_pos(lv_obj_get_child(r->root, -1), fw::lay::inset, fw::lay::inset);
  }
  lv_obj_t *c = box(r->root);
  lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);

  for (int i = 0; i < N; i++) {
    Doc spec;
    row_spec(LIGHTS[i], spec);
    lv_obj_t *row = r->rows[i] = fw::ha::bound(c, "list_row", spec.as<fw::Props>());
    if (!row) continue;
    lv_obj_add_event_cb(row, [](lv_event_t *e) {  // long-press: the Light screen for this light (no toggle after it)
      auto *r = (Room *) lv_event_get_user_data(e);
      auto *row = (lv_obj_t *) lv_event_get_current_target(e);
      const int i = row_index(r, row);
      if (i < 0 || lv_obj_has_state(row, fw::st::OFF)) return;
      if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
      lv_obj_remove_state(row, LV_STATE_PRESSED);
      Doc to;
      to["entity"] = LIGHTS[i].e;
      to["name"] = LIGHTS[i].name;
      std::string props;
      serializeJson(to, props);
      const std::string id = "light?" + props;
      fw::sound(row, "open");
      fw::emit(row, "navigate", id.c_str());
    }, LV_EVENT_LONG_PRESSED, r);
  }
}
const int reg = fw::add({"living_room", "Living room", "screens", META, build_room});
}  // namespace
