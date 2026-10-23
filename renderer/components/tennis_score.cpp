// Tennis score: a tennis feed with tennis's own logic (Sport foundation rules: 0 15 30 40 AD, tiebreak at 6-6 with
// the final-set rule, break / set / match points). Three layouts (a design round, sports-ux.md):
//   board    the broadcast board: one row per player = serve dot, name, finished sets (text-dim; the winner's digit
//            in text), the set in play (text), the point (accent-text). Dense, every fact on two lines.
//   hero     the point is the hero (value type, the largest style with letters for AD: "40 - 30", what changes most
//            often, readable across the room),
//            the names over it, sets and games in one line under it.
//   compact  for a card: name + games of the set in play + the point, sets as a dim prefix.
// The server: the Icons foundation tennis ball (small) beside the name.
// Under all: the moment line (accent-text caps: Break point / Set point / Match point / Tiebreak; upcoming: Starts
// 14:00 from the feed's "starts") and the state
// (Status chip: Live pulses; Changeover; Final; Upcoming). A point won lifts the new digits in (sport-flash), silent.
// Sources without points (ESPN / Team Tracker) hide the point; without a server the dot hides (sports-data.md).
// Data: prop "feed" = a tennis feed, or the tennis simulator (seed, speed, jump, best_of, final_set). Pairs: "sport".
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
constexpr int MAX_SETS = 5;
struct PRow { lv_obj_t *serve, *name, *sets[MAX_SETS], *games, *point; };
struct View { std::string layout; PRow r[2]; lv_obj_t *hero = nullptr, *line = nullptr, *moment = nullptr, *chip = nullptr; std::string last[2][3]; };
View *view_of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (View *) lv_obj_get_user_data(h) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }

// text that changed lifts in from below (sport-flash, ease-out; reduced = instant); silent
void set_lift(lv_obj_t *l, fw::ty::Id s, const char *txt, bool lift) {
  if (!strcmp(lv_label_get_text(l), fw::cased(s, txt).c_str())) return;
  fw::set_text(l, s, txt);
  if (lift) lift_in(l);
}
lv_obj_t *box(lv_obj_t *parent, int w, int h) {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b, w, h);
  return b;
}
lv_obj_t *cell(lv_obj_t *row, fw::ty::Id s, lv_color_t c, int w) {
  lv_obj_t *l = fw::text(row, s, "", c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_width(l, w);
  lv_obj_set_height(l, fw::style(s).box);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}
lv_obj_t *flex_row(lv_obj_t *parent, int w, int h, int gap) {
  lv_obj_t *r = box(parent, w, h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  lv_obj_add_flag(r, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // descenders reach below the text boxes (type rule)
  return r;
}

void build(lv_obj_t *root, View &v, int w) {
  const fw::Theme &t = *fw::T;
  const bool hero = v.layout == "hero", compact = v.layout == "compact";
  const fw::ty::Id num = compact ? fw::ty::body : fw::ty::title;
  const int cw = fw::sp::s10 + (compact ? 0 : fw::sp::s3);  // one score column
  const int rh = compact ? fw::lay::sport_row : fw::lay::sport_row + fw::sp::s3;
  if (hero) {
    v.hero = flex_row(root, w, fw::style(fw::ty::value).box, fw::sp::s5);
    lv_obj_set_flex_align(v.hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  }
  for (int i = 0; i < 2; i++) {
    PRow &r = v.r[i];
    lv_obj_t *row = flex_row(root, w, rh, fw::sp::s3);
    r.serve = fw::icon(row, "sport-tennis-ball", fw::ic::small, t.text);  // who serves (Icons foundation)
    r.name = fw::text(row, num, "", t.text);
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_flex_grow(r.name, 1);
    lv_obj_set_height(r.name, fw::style(num).box);
    fw::bleed(r.name, fw::sp::s3);  // descenders (Lindqvist) hang below the text box: redraw them with it
    for (int s = 0; s < MAX_SETS; s++) r.sets[s] = cell(row, num, t.text_dim, cw);
    r.games = cell(row, num, t.text, cw);
    if (hero) r.point = cell(v.hero, fw::ty::value, t.accent_text, fw::style(fw::ty::value).box * 3 / 2);
    else r.point = cell(row, num, t.accent_text, cw + fw::sp::s3);
    if (hero && i == 0) {  // the dash between the two points
      lv_obj_t *dash = fw::text(v.hero, fw::ty::value, "-", t.text_dim);
      (void) dash;
    }
  }
  if (hero) lv_obj_move_to_index(v.hero, 3);  // the names (with sets), then the points, then the line
  v.line = flex_row(root, w, fw::lay::chip_h, fw::sp::s3);
  v.moment = fw::text(v.line, fw::ty::c_caps_sm, "", t.accent_text);
  lv_obj_set_flex_grow(v.moment, 1);
  const fw::Component *chip = fw::find("status_chip");
  if (chip) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = "idle", cp["text"] = "Upcoming";
    chip->build(v.line, cp.as<fw::Props>());
    v.chip = lv_obj_get_child(v.line, -1);
  }
}

void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst f, bool lift) {
  const fw::Theme &t = *fw::T;
  const TennisNow n = tennis_now(f);
  const TennisFormat fm = tennis_format(f);
  const bool hero = v.layout == "hero", compact = v.layout == "compact";
  const fw::ty::Id num = compact ? fw::ty::body : fw::ty::title;
  ArduinoJson::JsonArrayConst sets = f["sets"];
  const int ns = (int) sets.size();
  const bool done = n.winner >= 0;
  for (int i = 0; i < 2; i++) {
    PRow &r = v.r[i];
    ArduinoJson::JsonObjectConst pl = f["players"][i];
    char b[64];
    const char *name = pl["name"] | (i ? "Player 2" : "Player 1");
    if (compact) name = pl["short"] | name;
    if ((pl["seed"] | 0) > 0 && !compact) snprintf(b, sizeof b, "%s  %d", name, (int) (pl["seed"] | 0));
    else snprintf(b, sizeof b, "%s", name);
    fw::set_text(r.name, num, b);
    lv_obj_set_style_text_color(r.name, done && n.winner != i ? t.text_dim : t.text, 0);
    // serve dot: shown while live and the source says who serves
    const bool srv = !done && n.server == i;
    lv_obj_set_style_opa(r.serve, srv ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    // finished sets (all of them once final), then the set in play
    const int fin = done ? ns : std::max(0, ns - 1);
    for (int s = 0; s < MAX_SETS; s++) {
      if (s < fin && s < fm.best_of) {
        const int a = sets[s][i] | 0, o = sets[s][1 - i] | 0;
        snprintf(b, sizeof b, "%d", a);
        set_lift(r.sets[s], num, b, false);
        lv_obj_set_style_text_color(r.sets[s], a > o ? t.text : t.text_dim, 0);
        lv_obj_remove_flag(r.sets[s], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(r.sets[s], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (!done && ns) {
      snprintf(b, sizeof b, "%d", n.games[i]);
      set_lift(r.games, num, b, lift);
      lv_obj_remove_flag(r.games, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(r.games, LV_OBJ_FLAG_HIDDEN);
    }
    if (*n.label[i]) {
      set_lift(r.point, hero ? fw::ty::value : num, n.label[i], lift);
      lv_obj_remove_flag(r.point, LV_OBJ_FLAG_HIDDEN);
    } else if (!hero) {
      lv_obj_add_flag(r.point, LV_OBJ_FLAG_HIDDEN);
    } else {
      set_lift(r.point, fw::ty::value, " ", false);
    }
  }
  // the moment (in order of weight) and the state
  const State st = state_of(f["state"] | "upcoming", f["stale"] | false);
  const char *m = done ? "" : n.match_point ? "Match point" : n.set_point ? "Set point" : n.break_point ? "Break point"
                : n.tiebreak ? "Tiebreak" : "";
  char starts[32] = "";  // an upcoming match: its start time from the feed ("starts": "14:00", the house's time)
  if (st == State::upcoming && *(f["starts"] | "")) snprintf(starts, sizeof starts, "Starts %s", f["starts"] | ""), m = starts;
  set_lift(v.moment, fw::ty::c_caps_sm, m, lift);
  if (v.chip) {
    ArduinoJson::JsonDocument cp;
    const char *word = st == State::brk ? "Changeover" : st == State::final ? (done ? "Final" : "Final") : state_word(st);
    cp["tone"] = state_tone(st), cp["text"] = word, cp["pulse"] = st == State::live;
    fw::update_child(fw::find("status_chip"), v.chip, cp.as<fw::Props>());
  }
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) {
  char b[96];
  snprintf(b, sizeof b, "%s|%d|%d", p["layout"] | "board", (int) (p["width"] | 336), (int) (p["sim"] | true));
  return b;
}
void fill(lv_obj_t *root, fw::Props p) {
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int w = fw::px((float) (p["width"] | 336));
  lv_obj_set_size(root, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s2, 0);
  lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the rows' descenders and lifts
  // holder of the view (first child, zero size)
  lv_obj_t *h = box(root, 0, 0);
  lv_obj_add_flag(h, LV_OBJ_FLAG_IGNORE_LAYOUT);
  View *v = new View;
  v->layout = p["layout"] | "board";
  lv_obj_set_user_data(h, v);
  lv_obj_add_event_cb(h, free_view, LV_EVENT_DELETE, v);
  build(root, *v, w);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(tennis_score, "Tennis score", "components", R"({
  "about": "A tennis score with tennis's own rules: sets, games, the point, who serves and the big moments.",
  "motion": "the simulated match: points change",
  "motion_off": "sim",
  "frame": [360, 0],
  "icons": ["sport-tennis-ball@small"],
  "props": [
    {"name": "layout", "type": "select", "options": ["board", "hero", "compact"], "default": "board"},
    {"name": "width", "type": "range", "min": 240, "max": 448, "step": 8, "default": 336},
    {"name": "sim", "type": "bool", "default": true},
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "type": "select", "options": ["start", "break point", "break", "tiebreak", "set point", "match point"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "best_of", "type": "select", "options": ["3", "5"], "default": "3"},
    {"name": "final_set", "type": "select", "options": ["tb10", "tb7", "adv"], "default": "tb10"},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the tennis simulator (seeded): made-up players, real scoring.",
  "variants": [
    {"label": "board", "props": {}},
    {"label": "hero", "props": {"layout": "hero"}},
    {"label": "compact", "props": {"layout": "compact", "width": 240}},
    {"label": "break point", "props": {"jump": "break point"}},
    {"label": "hero · break point", "props": {"layout": "hero", "jump": "break point"}},
    {"label": "tiebreak", "props": {"jump": "tiebreak", "seed": 3}},
    {"label": "match point · best of 5", "props": {"jump": "match point", "best_of": "5"}},
    {"label": "no points · ESPN-like feed", "props": {"sim": false, "feed": "{\"state\":\"live\",\"players\":[{\"name\":\"Mara Lindqvist\"},{\"name\":\"Ayla Moreno\"}],\"sets\":[[6,4],[3,2]]}"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(tennis_score) {
  View *v = view_of(root);
  if (!v || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
