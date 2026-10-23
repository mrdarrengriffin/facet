// F1 race status: the at-a-glance answer to "what is happening in the race right now": the flag the race is under
// (the F1 flag, real component) + one word for the state + where the race is (lap n / N). Colour never alone: every
// state has its word (Racing, Yellow S5, Double yellow S9, Safety car, SC in this lap, Virtual safety car, VSC
// ending, Red flag, Last lap, Chequered, Formation lap, Finished).
// Three forms (a design round, sports-ux.md):
//   pill    (favourite, the F1 screen header) a chip-height pill tinted in the state's tone (chip-tint, like the
//           Status chip): small flag, the word in label caps (text), the lap in label text-dim
//   card    a surface card: the hero flag, the word in screen title type, the lap and what it means in c-meta
//           ("Lap 34 of 56 · 22 to go", "No overtaking", "Hold the delta")
//   banner  full-width tinted box for the moment it changes: hero flag, the word in headline type, the detail
// Tone: green / racing = ok, yellow / SC / VSC = warn, red = error, chequered / finished = idle, last lap = info.
// Motion on a change (silent, like any update): the flag flutters once, the word lifts in, the tint fades (Motion
// fade); reduced motion = the new state at once.
// Data: prop "feed" = an f1_race_feed, else the race simulator. Pairs: "sport" (text on bg / surface; the tint is
// chip-tint, the same as the Status chip's).
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
struct View { lv_obj_t *box = nullptr, *flag = nullptr, *word = nullptr, *detail = nullptr; std::string form, key; lv_color_t tint; };
View *view_of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (View *) lv_obj_get_user_data(h) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }
lv_obj_t *plain(lv_obj_t *p) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
struct State { std::string flag, word, detail, tone; };
State state_of_feed(ArduinoJson::JsonVariantConst f, bool card) {
  ArduinoJson::JsonObjectConst s = f["session"];
  const std::string fl = s["flag"] | "green", st = s["state"] | "live";
  const int lap = s["lap"] | 0, laps = s["laps"] | 0;
  int sec = 0;
  for (ArduinoJson::JsonObjectConst x : f["sectors"].as<ArduinoJson::JsonArrayConst>()) sec = x["n"] | 0;
  char b[64];
  State r;
  r.flag = fl, r.tone = "ok";
  if (lap > 0 && laps > 0) snprintf(b, sizeof b, card ? "Lap %d of %d · %d to go" : "Lap %d/%d", lap, laps, std::max(0, laps - lap));
  else if (laps > 0) snprintf(b, sizeof b, "%d laps", laps);
  else b[0] = 0;
  if (!card && lap > 0 && laps > 0) snprintf(b, sizeof b, "L%d/%d", lap, laps);
  r.detail = b;
  if (st == "formation" || st == "start") r.word = "Formation lap", r.flag = "green";
  else if (fl == "red") r.word = "Red flag", r.tone = "error", r.detail = card ? "Session stopped" : r.detail;
  else if (fl == "sc") r.word = "Safety car", r.tone = "warn", r.detail = card ? "No overtaking · " + r.detail : r.detail;
  else if (fl == "sc_ending") r.word = "SC in this lap", r.tone = "warn", r.flag = "sc";
  else if (fl == "vsc") r.word = card ? "Virtual safety car" : "VSC", r.tone = "warn", r.detail = card ? "Hold the delta · " + r.detail : r.detail;
  else if (fl == "vsc_ending") r.word = "VSC ending", r.tone = "warn", r.flag = "vsc";
  else if (fl == "yellow" || fl == "double_yellow") {
    snprintf(b, sizeof b, "%s S%d", fl == "yellow" ? "Yellow" : "Double yellow", sec);
    r.word = b, r.tone = "warn";
  } else if (fl == "chequered") r.word = st == "final" ? "Finished" : "Chequered", r.tone = "idle";
  else if (s["last_lap"] | false) r.word = "Last lap", r.tone = "info", r.flag = "green";
  else r.word = "Racing", r.flag = "green";
  return r;
}
lv_color_t tone_ink(const std::string &t) {
  const fw::Theme &T = *fw::T;
  return t == "ok" ? T.ok : t == "warn" ? T.warn : t == "error" ? T.error : t == "info" ? T.blue : T.text_dim;
}
void tint_cb(lv_anim_t *a, int32_t x) {
  View *v = (View *) lv_anim_get_user_data(a);
  lv_obj_set_style_bg_color(v->box, lv_color_mix(v->tint, lv_obj_get_style_bg_color(v->box, LV_PART_MAIN), (uint8_t) x), 0);
}
void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst f, bool glide) {
  const State s = state_of_feed(f, v.form != "pill");
  const std::string key = s.flag + "|" + s.word + "|" + s.detail;
  if (key == v.key) return;
  const bool state_changed = v.key.substr(0, v.key.find('|', v.key.find('|') + 1)) != key.substr(0, key.find('|', key.find('|') + 1));
  v.key = key;
  ArduinoJson::JsonDocument d;
  d["flag"] = s.flag, d["size"] = v.form == "pill" ? "sm" : "lg";
  fw::update_child(fw::find("f1_flag"), v.flag, d.as<fw::Props>());
  const fw::ty::Id wt = v.form == "pill" ? fw::ty::label : v.form == "card" ? fw::ty::title : fw::ty::headline;
  const fw::ty::Id dt = v.form == "pill" ? fw::ty::label : fw::ty::c_meta;
  fw::set_text(v.word, wt, s.word.c_str());
  fw::set_text(v.detail, dt, s.detail.empty() ? " " : s.detail.c_str());
  if (glide && state_changed) lift_in(v.word);
  if (v.form != "card") {
    v.tint = tone_ink(s.tone);
    lv_anim_delete(v.box, nullptr);
    if (glide && state_changed && !fw::mo::reduced(root)) {
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, v.box);
      lv_anim_set_user_data(&a, &v);
      lv_anim_set_custom_exec_cb(&a, tint_cb);
      lv_anim_set_values(&a, 0, 255);
      lv_anim_set_duration(&a, fw::mo::ms::fade);
      lv_anim_start(&a);
    } else {
      lv_obj_set_style_bg_color(v.box, v.tint, 0);
    }
  }
  if (v.form == "pill") {  // hug the words, on the 4 px grid
    lv_obj_set_width(v.box, LV_SIZE_CONTENT);
    lv_obj_update_layout(v.box);
    const int w = (lv_obj_get_width(v.box) + 3) / 4 * 4;
    lv_obj_set_width(v.box, w);
    lv_obj_set_width(root, w);
  }
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) { return std::string(p["form"] | "pill") + "|" + std::to_string((int) (p["width"] | 360)) + ((p["sim"] | true) ? "s" : ""); }
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  View *v = new View;
  v->form = p["form"] | "pill";
  lv_obj_t *h = plain(root);
  lv_obj_set_size(h, 0, 0);
  lv_obj_add_flag(h, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_user_data(h, v);
  lv_obj_add_event_cb(h, free_view, LV_EVENT_DELETE, v);
  const bool pill = v->form == "pill", card = v->form == "card";
  const int w = fw::px((float) (p["width"] | 360));
  const int flag_h = (pill ? fw::lay::sport_flag_sm : fw::lay::sport_flag_lg) * 2 / 3 / 4 * 4;
  const int pad = pill ? fw::sp::s3 : fw::sp::s5;
  const int bh = pill ? fw::lay::chip_h : flag_h + 2 * pad;
  v->box = plain(root);
  lv_obj_set_size(v->box, pill ? LV_SIZE_CONTENT : w, bh);
  lv_obj_set_style_pad_hor(v->box, pad, 0);
  lv_obj_set_style_radius(v->box, pill ? LV_RADIUS_CIRCLE : card ? fw::rad::card : fw::rad::button, 0);
  lv_obj_set_style_bg_opa(v->box, card ? LV_OPA_COVER : (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255), 0);
  lv_obj_set_style_bg_color(v->box, card ? t.surface : t.text_dim, 0);
  if (card) {
    lv_obj_set_style_border_width(v->box, fw::stroke::hair, 0);
    lv_obj_set_style_border_color(v->box, t.line, 0);
  }
  lv_obj_set_flex_flow(v->box, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(v->box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(v->box, pill ? fw::sp::s3 : fw::sp::s5, 0);
  lv_obj_add_flag(v->box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  ArduinoJson::JsonDocument fd;
  fd["flag"] = "green", fd["size"] = pill ? "sm" : "lg";
  fw::find("f1_flag")->build(v->box, fd.as<fw::Props>());
  v->flag = lv_obj_get_child(v->box, -1);
  lv_obj_t *words = v->box;
  if (!pill) {  // word over detail
    words = plain(v->box);
    lv_obj_set_flex_grow(words, 1);
    lv_obj_set_height(words, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(words, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(words, fw::sp::s1, 0);
    lv_obj_add_flag(words, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  }
  const fw::ty::Id wt = pill ? fw::ty::label : card ? fw::ty::title : fw::ty::headline;
  v->word = fw::text(words, wt, " ", t.text);
  v->detail = fw::text(words, pill ? fw::ty::label : fw::ty::c_meta, " ", t.text_dim);
  if (!pill) for (lv_obj_t *l : {v->word, v->detail}) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_height(l, fw::style(l == v->word ? wt : fw::ty::c_meta).box);
  }
  lv_obj_set_size(root, pill ? fw::px(160) : w, bh);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(f1_race_status, "F1 race status", "components", R"({
  "about": "What is happening in the race right now: the flag, the state in a word, and the lap.",
  "frame": [400, 120],
  "props": [
    {"name": "form", "type": "select", "options": ["pill", "card", "banner"], "default": "pill"},
    {"name": "width", "type": "range", "min": 240, "max": 448, "step": 8, "default": 360},
    {"name": "sim", "type": "bool", "default": true},
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "type": "select", "options": ["start", "yellow", "double yellow", "vsc", "safety car", "red flag", "restart", "final lap", "chequered"], "default": "yellow"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the race simulator (seeded).",
  "variants": [
    {"label": "pill · yellow · favourite", "props": {}},
    {"label": "pill · safety car", "props": {"jump": "safety car"}},
    {"label": "pill · racing", "props": {"jump": "start", "sim": false, "feed": "{\"session\":{\"state\":\"live\",\"flag\":\"green\",\"lap\":34,\"laps\":56}}"}},
    {"label": "card · VSC", "props": {"form": "card", "jump": "vsc"}},
    {"label": "card · red flag", "props": {"form": "card", "jump": "red flag"}},
    {"label": "banner · safety car", "props": {"form": "banner", "jump": "safety car"}},
    {"label": "banner · chequered", "props": {"form": "banner", "jump": "chequered"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(f1_race_status) {
  View *v = view_of(root);
  if (!v || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
