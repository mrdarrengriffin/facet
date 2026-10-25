// Football pitch: a football match feed as the scoreline over the pitch from above (IFAB markings at 105 x 68 m) with
// both line-ups in their formation (home left, attacking right) and the match's events on a minute line under it.
// Honest by design (sports-data.md): players stand at their FORMATION places from the line-up (API-Football style
// "row:col" grid), never at live positions (no public tracking exists); events have no pitch position in any free
// source, so they go on the minute line, not on the grass.
//   header   home short + score, the minute (45+2'), away score + short; Status chip (Live pulses / Half time /
//            Final / Upcoming); possession as two thin team bars (only when the feed has it)
//   pitch    steel lines (stroke::edge): touch / goal lines, halfway, centre circle + spot, both boxes, goal areas,
//            penalty spots and the arcs (stroke atom, LVGL's arc steps at this size)
//   players  Sport marks, number kind at lay::sport_player: a behind-colour disc, team-colour ring (fitted to 3:1),
//            shirt number in text; a sent-off player leaves (fade), a sub takes the number (lift)
//   events   minute line 0-90 (+ added) with half time; home marks above, away below, as Icons foundation icons at
//            the small size: sport-goal (text), sport-card (warn = yellow, error = red), sport-sub (steel). The latest
//            event as its icon + one line of words under it, accent-text for sport-moment then text-dim
// Motion: score digits lift (sport-flash), new marks appear (fade), silent. Pairs: "sport".
// Data: prop "feed" = a football match feed, or the match simulator (made-up clubs and players).
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
using fw::StrokePt;
constexpr float PL = 105, PW = 68;
struct View {
  int w = 0, ph = 0, top = 0, tl_y = 0;
  lv_obj_t *score[2] = {nullptr, nullptr}, *shrt[2] = {nullptr, nullptr}, *minute = nullptr, *chip = nullptr;
  lv_obj_t *poss[2] = {nullptr, nullptr};
  lv_obj_t *players = nullptr, *marks = nullptr, *latest = nullptr, *latest_row = nullptr, *latest_icon = nullptr;
  int n_events = -1;
  uint32_t latest_until = 0;
  std::string lineup_sig;
};
View *view_of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (View *) lv_obj_get_user_data(h) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }
// an event's icon (Icons foundation, sport group) in its colour: goal = text, yellow / red card = warn / error, sub = steel
lv_obj_t *event_icon(lv_obj_t *parent, const char *type, fw::ic::Size size) {
  const fw::Theme &t = *fw::T;
  if (!strcmp(type, "goal")) return fw::icon(parent, "sport-goal", size, t.text);
  if (!strcmp(type, "yellow")) return fw::icon(parent, "sport-card", size, t.warn);
  if (!strcmp(type, "red")) return fw::icon(parent, "sport-card", size, t.error);
  return fw::icon(parent, "sport-sub", size, t.steel);
}
lv_obj_t *plain(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}
lv_obj_t *line_box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c, int radius = 0) {
  lv_obj_t *o = plain(p, x, y, w, h);
  lv_obj_set_style_border_width(o, fw::stroke::edge, 0);
  lv_obj_set_style_border_color(o, c, 0);
  lv_obj_set_style_radius(o, radius, 0);
  fw::qa::free(o, "pitch markings at IFAB proportions");
  return o;
}
lv_obj_t *fill_box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c, int radius = 0) {
  lv_obj_t *o = plain(p, x, y, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  fw::qa::free(o, "pitch markings at IFAB proportions");
  return o;
}

void draw_pitch(lv_obj_t *root, View &v) {
  const fw::Theme &t = *fw::T;
  const int e = fw::stroke::edge, oy = v.top;
  const float s = v.w / PL;
  auto X = [&](float m) { return (int) std::lround(m * s); };
  auto Y = [&](float m) { return oy + (int) std::lround(m * s); };
  line_box(root, 0, oy, v.w, v.ph, t.steel);
  fill_box(root, X(PL / 2) - e / 2, oy, e, v.ph, t.steel);
  const int cr = X(9.15f);
  line_box(root, X(PL / 2) - cr, Y(PW / 2) - cr, 2 * cr, 2 * cr, t.steel, LV_RADIUS_CIRCLE);
  const int spot = fw::stroke::indicator;
  fill_box(root, X(PL / 2) - spot / 2, Y(PW / 2) - spot / 2, spot, spot, t.steel, LV_RADIUS_CIRCLE);
  for (int side = 0; side < 2; side++) {
    const float bx = side ? PL - 16.5f : 0, gx = side ? PL - 5.5f : 0, px = side ? PL - 11 : 11;
    line_box(root, X(bx), Y((PW - 40.32f) / 2), X(16.5f), X(40.32f), t.steel);
    line_box(root, X(gx), Y((PW - 18.32f) / 2), X(5.5f), X(18.32f), t.steel);
    fill_box(root, X(px) - spot / 2, Y(PW / 2) - spot / 2, spot, spot, t.steel, LV_RADIUS_CIRCLE);
    // the arc: r 9.15 around the spot, the part outside the box (acos(5.5 / 9.15) = 53.1 deg either side)
    const int d = 2 * cr, ax = X(px) - cr, ay = Y(PW / 2) - cr;
    lv_obj_t *arc = side ? fw::stroke_arc(root, ax, ay, d, 126.9f, 233.1f, e, t.steel, fw::Cap::butt)
                         : fw::stroke_arc(root, ax, ay, d, 306.9f, 53.1f, e, t.steel, fw::Cap::butt);
    fw::qa::free(arc, "penalty arc around the spot");
  }
}

// formation place of grid "row:col" among `rows` rows (row 1 = keeper), cols in that row; home attacks right
StrokePt place(const View &v, int team, int row, int col, int cols_in_row, int rows) {
  const float depth = rows <= 1 ? 0.05f : 0.05f + 0.40f * (row - 1) / (rows - 1);  // own goal -> just short of halfway
  const float across = (col - 0.5f) / std::max(1, cols_in_row);
  float x = depth * v.w, y = v.top + across * v.ph;
  if (team == 1) x = v.w - x, y = v.top + v.ph - (y - v.top);  // away mirrored (their left = our right)
  return {x, y};
}

void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst f, bool lift) {
  const fw::Theme &t = *fw::T;
  const uint32_t now = lv_tick_get();
  ArduinoJson::JsonArrayConst teams = f["teams"];
  char b[64];
  for (int k = 0; k < 2; k++) {
    ArduinoJson::JsonObjectConst tm = teams[k];
    snprintf(b, sizeof b, "%d", (int) (tm["score"] | 0));
    if (strcmp(lv_label_get_text(v.score[k]), b)) {
      fw::set_text(v.score[k], fw::ty::value, b);
      if (lift) lift_in(v.score[k]);
    }
    fw::set_text(v.shrt[k], fw::ty::label, tm["short"] | (k ? "AWAY" : "HOME"));
    // possession bars: width by share, team colour fitted
    const int pc = tm["possession"] | -1;
    if (pc >= 0) {
      lv_obj_remove_flag(v.poss[k], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(v.poss[k], std::max(fw::stroke::edge, (int) std::lround((v.w / 2.0f - fw::sp::s1) * pc / 50.0f / 2)));
      lv_obj_set_style_bg_color(v.poss[k], team_ink(hex(tm["colour"] | "", t.text_dim), t.bg, t.text), 0);
    } else {
      lv_obj_add_flag(v.poss[k], LV_OBJ_FLAG_HIDDEN);
    }
  }
  // the minute
  const State st = state_of(f["state"] | "upcoming", f["stale"] | false);
  ArduinoJson::JsonObjectConst clk = f["clock"];
  const int mn = clk["min"] | 0, add = clk["added"] | 0;
  if (st == State::live && add > 0) snprintf(b, sizeof b, "%d+%d'", mn, add);
  else if (st == State::live) snprintf(b, sizeof b, "%d'", mn);
  else if (st == State::brk) snprintf(b, sizeof b, "HT");
  else if (st == State::final) snprintf(b, sizeof b, "FT");
  else if (*(f["starts"] | "")) snprintf(b, sizeof b, "%s", f["starts"] | "");  // a real fixture: its kick-off ("20:00")
  else if ((f["starts_in"] | -1) >= 0) snprintf(b, sizeof b, "%ds", (int) (f["starts_in"] | 0));
  else snprintf(b, sizeof b, " ");
  fw::set_text(v.minute, fw::ty::label, b);
  if (v.chip) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = state_tone(st), cp["text"] = st == State::brk ? "Half time" : state_word(st), cp["pulse"] = st == State::live;
    fw::update_child(fw::find("status_chip"), v.chip, cp.as<fw::Props>());
  }
  // players: rebuilt when the line-ups change (a sub, a red card), numbers lift
  std::string sig;
  for (int k = 0; k < 2; k++)
    for (ArduinoJson::JsonObjectConst pl : teams[k]["lineup"].as<ArduinoJson::JsonArrayConst>()) sig += std::to_string((int) (pl["num"] | 0)) + (pl["grid"] | "") + ",";
  if (sig != v.lineup_sig) {
    const std::string old = v.lineup_sig;
    v.lineup_sig = sig;
    lv_obj_clean(v.players);
    for (int k = 0; k < 2; k++) {
      ArduinoJson::JsonObjectConst tm = teams[k];
      // rows and their sizes from the grid (row:col)
      int rows = 1, cols[8] = {0};
      for (ArduinoJson::JsonObjectConst pl : tm["lineup"].as<ArduinoJson::JsonArrayConst>()) {
        int r = 0, c = 0;
        sscanf(pl["grid"] | "1:1", "%d:%d", &r, &c);
        r = std::max(1, std::min(7, r));
        rows = std::max(rows, r), cols[r] = std::max(cols[r], c);
      }
      for (ArduinoJson::JsonObjectConst pl : tm["lineup"].as<ArduinoJson::JsonArrayConst>()) {
        int r = 0, c = 0;
        sscanf(pl["grid"] | "1:1", "%d:%d", &r, &c);
        r = std::max(1, std::min(7, r));
        MarkLook l;
        l.mark = Mark::number, l.team = hex(tm["colour"] | "", t.text_dim), l.behind = t.bg, l.num = pl["num"] | 0, l.code = "";
        lv_obj_t *mk = marker(v.players, l);
        lv_obj_set_size(mk, fw::lay::sport_player, fw::lay::sport_player);
        const StrokePt pt = place(v, k, r, c, cols[r], rows);
        marker_place(mk, pt.x, pt.y, v.w);
        fw::qa::free(mk, "formation place from the line-up grid");
        char key[16];
        snprintf(key, sizeof key, "%d%s,", l.num, pl["grid"] | "");
        if (lift && !old.empty() && old.find(key) == std::string::npos) lift_in(lv_obj_get_child(mk, 0));
      }
    }
  }
  // events on the minute line (rebuilt when there are new ones; the new ones fade in)
  ArduinoJson::JsonArrayConst evs = f["events"];
  const int ne = (int) evs.size();
  if (ne != v.n_events) {
    const int before = v.n_events;
    v.n_events = ne;
    lv_obj_clean(v.marks);
    const int span = 90 + (int) (clk["period"] == 2 ? (clk["added_total"] | 0) : 0);
    const int lw = lv_obj_get_width(v.marks), mid = lv_obj_get_height(v.marks) / 2, me = fw::ic::PX[fw::ic::small];
    // the line + half time
    fill_box(v.marks, 0, mid - fw::stroke::hair / 2, lw, fw::stroke::hair, t.line);
    fill_box(v.marks, lw * 45 / span, mid - me, fw::stroke::hair, 2 * me, t.line);
    for (int i = 0; i < ne; i++) {
      ArduinoJson::JsonObjectConst e = evs[i];
      const char *ty = e["type"] | "";
      const int m = std::min(span, (int) (e["min"] | 0) + (int) (e["added"] | 0));
      const int x = std::max(0, std::min(lw - me, lw * m / span - me / 2));
      const int team = e["team"] | 0;
      const int y = team == 0 ? mid - fw::sp::s1 - me : mid + fw::sp::s1;
      lv_obj_t *o = event_icon(v.marks, ty, fw::ic::small);
      lv_obj_set_pos(o, x, y);
      fw::qa::free(o, "placed by its minute");
      if (lift && before >= 0 && i >= before) fw::appear(o, fw::mo::fade, true, 0);
    }
    if (ne > 0) {  // the latest, in words
      ArduinoJson::JsonObjectConst e = evs[ne - 1];
      const char *ty = e["type"] | "";
      const char *word = !strcmp(ty, "goal") ? "Goal" : !strcmp(ty, "yellow") ? "Yellow card" : !strcmp(ty, "red") ? "Red card" : "Sub";
      const int team = e["team"] | 0;
      char mm[16];
      if ((e["added"] | 0) > 0) snprintf(mm, sizeof mm, "%d+%d'", (int) (e["min"] | 0), (int) (e["added"] | 0));
      else snprintf(mm, sizeof mm, "%d'", (int) (e["min"] | 0));
      if (!strcmp(ty, "sub")) snprintf(b, sizeof b, "%s %s · %s on, %s off", mm, teams[team]["short"] | "", e["player"] | "", e["off"] | "");
      else snprintf(b, sizeof b, "%s %s · %s, %s", mm, word, e["player"] | "", teams[team]["short"] | "");
      fw::set_text(v.latest, fw::ty::c_meta, b);
      if (v.latest_icon) lv_obj_delete(v.latest_icon);
      v.latest_icon = event_icon(v.latest_row, ty, fw::ic::icon);
      lv_obj_move_to_index(v.latest_icon, 0);
      if (lift && before >= 0 && ne > before) v.latest_until = now + fw::mo::ms::sport_moment, lift_in(v.latest);
    } else {
      fw::set_text(v.latest, fw::ty::c_meta, " ");
      if (v.latest_icon) lv_obj_delete(v.latest_icon), v.latest_icon = nullptr;
    }
  }
  lv_obj_set_style_text_color(v.latest, now < v.latest_until ? t.accent_text : t.text_dim, 0);
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) {
  char b[64];
  snprintf(b, sizeof b, "%d|%d", (int) (p["width"] | 400), (int) (p["sim"] | true));
  return b;
}
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  View *v = new View;
  lv_obj_t *h = plain(root, 0, 0, 0, 0);
  lv_obj_set_user_data(h, v);
  lv_obj_add_event_cb(h, free_view, LV_EVENT_DELETE, v);
  v->w = fw::px((float) (p["width"] | 400));
  // header: [HBC 2] [67'] [2 ASH] ... chip under; value-size scores
  const int vh = fw::style(fw::ty::value).box, lh = fw::style(fw::ty::label).box;
  const int third = v->w / 3;
  for (int k = 0; k < 2; k++) {
    v->shrt[k] = fw::text(root, fw::ty::label, "", t.text);
    lv_obj_set_width(v->shrt[k], third / 2);
    lv_obj_set_style_text_align(v->shrt[k], k ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(v->shrt[k], k ? v->w - third / 2 : 0, (vh - lh) / 2);
    v->score[k] = fw::text(root, fw::ty::value, "", t.text);
    lv_obj_set_width(v->score[k], third / 2);
    lv_obj_set_style_text_align(v->score[k], k ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(v->score[k], k ? v->w / 2 + third / 2 : v->w / 2 - third, 0);
  }
  v->minute = fw::text(root, fw::ty::label, "", t.text_dim);
  lv_obj_set_width(v->minute, third / 2);
  lv_obj_set_style_text_align(v->minute, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(v->minute, v->w / 2 - third / 4, (vh - lh) / 2);
  // possession bars meet in the middle under the score
  const int py = vh + fw::sp::s2;
  for (int k = 0; k < 2; k++) {
    v->poss[k] = plain(root, 0, py, 0, fw::stroke::indicator);
    lv_obj_set_style_bg_opa(v->poss[k], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(v->poss[k], fw::stroke::indicator / 2, 0);
    lv_obj_set_align(v->poss[k], k ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(v->poss[k], k ? v->w / 2 + fw::sp::s1 : -(v->w / 2 + fw::sp::s1));
    fw::qa::free(v->poss[k], "length = possession share");
  }
  v->top = py + fw::stroke::indicator + fw::sp::s5;
  v->ph = (int) std::lround(v->w * PW / PL);
  draw_pitch(root, *v);
  v->players = plain(root, 0, 0, v->w, v->top + v->ph);
  // the minute line + the latest event + the state chip
  const int me = fw::ic::PX[fw::ic::small];
  v->tl_y = v->top + v->ph + fw::sp::s5;
  v->marks = plain(root, 0, v->tl_y, v->w, 2 * me + 2 * fw::sp::s1);
  const int ly = v->tl_y + 2 * me + 2 * fw::sp::s1 + fw::sp::s3;
  lv_obj_t *row = plain(root, 0, ly, v->w, fw::lay::chip_h);
  v->latest_row = row;
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, fw::sp::s3, 0);
  v->latest = fw::text(row, fw::ty::c_meta, " ", t.text_dim);
  lv_label_set_long_mode(v->latest, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_flex_grow(v->latest, 1);
  lv_obj_set_height(v->latest, fw::style(fw::ty::c_meta).box);
  if (const fw::Component *chip = fw::find("status_chip")) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = "idle", cp["text"] = "Upcoming";
    chip->build(row, cp.as<fw::Props>());
    v->chip = lv_obj_get_child(row, -1);
  }
  lv_obj_set_size(root, v->w, (ly + fw::lay::chip_h + 3) / 4 * 4);  // the pitch keeps its true 105 x 68 height; the box snaps
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = football_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(football_pitch, "Football pitch", "components", R"({
  "about": "A football match: the score and minute, both line-ups in formation, and the match's events.",
  "motion": "the simulated match: the clock runs, events arrive",
  "motion_off": "sim",
  "frame": [440, 440],
  "icons": ["sport-goal@small", "sport-goal@icon", "sport-card@small", "sport-card@icon", "sport-sub@small", "sport-sub@icon"],
  "props": [
    {"name": "width", "about": "How wide the pitch is, in pixels.", "type": "range", "min": 280, "max": 432, "step": 8, "default": 400},
    {"name": "sim", "about": "For previews only: runs a made-up match so the score and events change.", "type": "bool", "default": true},
    {"name": "speed", "about": "For previews only: how fast the made-up match runs, real time or 10 or 60 times faster.", "type": "select", "options": ["1x", "10x", "60x"], "default": "60x"},
    {"name": "jump", "about": "For previews only: skips the made-up match to a moment such as a goal, a card or half time.", "type": "select", "options": ["start", "kickoff", "goal", "card", "sub", "half time", "added time", "full time"], "default": "start"},
    {"name": "seed", "about": "For previews only: picks a different made-up match; the same number always gives the same match.", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "feed", "about": "The live match data sent to the panel. Leave empty to use the made-up match.", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the match simulator (seeded): made-up clubs and players.",
  "variants": [
    {"label": "simulator · 60x", "props": {}},
    {"label": "first goal", "props": {"jump": "goal"}},
    {"label": "half time", "props": {"jump": "half time"}},
    {"label": "full time", "props": {"jump": "full time", "seed": 4}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(football_pitch) {
  View *v = view_of(root);
  if (!v || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = football_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
