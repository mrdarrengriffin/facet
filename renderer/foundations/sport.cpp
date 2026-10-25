// Sport foundation page (core/sport.h): the shared language of the sport screens, for people. Shared first (team
// colours, marks, live states, score type, moments), then each sport's own parts (F1: circuits, car marks, gaps;
// tennis: court + serve, the scoring rules as a table, set history; football: pitch + formations, events + the minute
// line, the scoreline). Every visual is a real component, most running their simulator (sample data, made-up
// players and clubs; the F1 grid is the 2026 one). Status = this file + core/sport*.cpp (WIP until approved).
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
lv_obj_t *column(lv_obj_t *parent, int gap) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  return c;
}
lv_obj_t *wrap(lv_obj_t *parent, int gap) {
  lv_obj_t *c = column(parent, gap);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(c, gap, 0);
  return c;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child(parent, -1);
}
lv_obj_t *caption(lv_obj_t *parent, const char *txt) { return fw::text(parent, fw::ty::c_meta, txt, fw::T->text_dim); }
// the page's content width in the components' dp (they take dp, px = fw::px(dp))
int page_dp(int max) { return std::min(max, (fw::lay::screen_w - 2 * fw::sp::s7) * 480 / fw::lay::screen_w / 8 * 8); }

// ---------------------------------------------------------------- shared
struct Team { const char *name, *code, *colour; int num; };
const Team TEAMS[] = {
  {"McLaren", "NOR", "#F47600", 1}, {"Ferrari", "LEC", "#ED1131", 16}, {"Red Bull Racing", "VER", "#4781D7", 3},
  {"Mercedes", "RUS", "#00D7B6", 63}, {"Aston Martin", "ALO", "#229971", 14}, {"Alpine", "GAS", "#00A1E8", 10},
  {"Williams", "ALB", "#1868DB", 23}, {"Racing Bulls", "LAW", "#6C98FF", 30}, {"Haas F1 Team", "OCO", "#9C9FA2", 31},
  {"Audi", "HUL", "#F50537", 27}, {"Cadillac", "PER", "#909090", 11},
  {"Harbour City (football)", "HBC", "#1B3A6B", 9}, {"Ashford Rovers (football)", "ASH", "#F2F2F2", 7},
};
void colours(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  for (const Team &tm : TEAMS) {
    lv_obj_t *row = wrap(page, fw::sp::s4);
    for (int on = 0; on < 2; on++) {
      ArduinoJson::JsonDocument d;
      d["marker"] = "code", d["code"] = tm.code, d["num"] = tm.num, d["colour"] = tm.colour, d["on"] = on ? "surface" : "bg";
      build("f1_car_marker", row, d);
    }
    const lv_color_t raw = hex(tm.colour, t.text);
    char b[112];
    snprintf(b, sizeof b, "%s  %s  %.1f:1 as sent, %.1f:1 shown", tm.name, tm.colour, contrast(raw, t.bg), contrast(team_ink(raw, t.bg, t.text), t.bg));
    caption(row, b);
  }
}
void marks(lv_obj_t *page) {
  // one mark language: a disc in the team colour, cut out of what is behind it, the code / number in token text
  lv_obj_t *row = wrap(page, fw::sp::s5);
  struct K { const char *marker, *code, *colour, *flag, *what; int num; };
  const K ks[] = {{"dot", "VER", "#4781D7", "", "a car", 3}, {"code", "VER", "#4781D7", "", "a car with its code", 3},
                  {"number", "VER", "#4781D7", "", "a car by number", 3}, {"dot", "NOR", "#F47600", "lead", "the leader", 1},
                  {"number", "HBC", "#1B3A6B", "", "a player (shirt 9)", 9}, {"number", "ASH", "#F2F2F2", "", "a player in white", 7}};
  for (const K &k : ks) {
    lv_obj_t *cell = column(row, fw::sp::s2);
    lv_obj_set_width(cell, LV_SIZE_CONTENT);
    ArduinoJson::JsonDocument d;
    d["marker"] = k.marker, d["code"] = k.code, d["num"] = k.num, d["colour"] = k.colour;
    if (*k.flag) d[k.flag] = true;
    build("f1_car_marker", cell, d);
    caption(cell, k.what);
  }
}
void states(lv_obj_t *page) {
  lv_obj_t *row = wrap(page, fw::sp::s4);
  const State all[] = {State::upcoming, State::live, State::brk, State::final, State::delayed, State::stale};
  for (State s : all) {
    ArduinoJson::JsonDocument d;
    d["tone"] = state_tone(s), d["text"] = state_word(s), d["pulse"] = s == State::live;
    build("status_chip", row, d);
  }
  caption(page, "Break is a changeover in tennis and half time in football. Stale: the source stopped answering.");
}
void type(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  struct S { fw::ty::Id id; const char *sample, *what; bool dim; };
  const S ss[] = {{fw::ty::value, "40  AD", "the point in play, a football score: the one thing to read first", false},
                  {fw::ty::title, "6  4", "games in the set in play (bright), finished sets (dim, the winner's in text)", false},
                  {fw::ty::body, "VER  NOR", "a driver in the order", false},
                  {fw::ty::c_meta, "+1.234   0.4", "gaps and intervals, tabular", true},
                  {fw::ty::label, "67'  45+2'", "the match minute, added time", true}};
  for (const S &s : ss) {
    lv_obj_t *row = wrap(page, fw::sp::s5);
    fw::text(row, s.id, s.sample, s.dim ? t.text_dim : t.text);
    caption(row, s.what);
  }
}
void moments(lv_obj_t *page) {
  // the same rule everywhere: the changed digits lift in, the moment word glows accent for a few seconds; silent
  ArduinoJson::JsonDocument d;
  d["layout"] = "board", d["width"] = page_dp(336), d["speed"] = "60x", d["jump"] = "break point";
  build("tennis_score", page, d);
  caption(page, "A point won: the new digits lift in. A break point: the words light up, then settle.");
  ArduinoJson::JsonDocument s;
  s["rows"] = 6, s["width"] = page_dp(240), s["speed"] = "60x", s["jump"] = "overtake", s["stat"] = "interval";
  build("f1_standings", page, s);
  caption(page, "A place gained: the rows slide, the new position glows for a moment.");
}

// ---------------------------------------------------------------- F1
void circuits(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *g = wrap(page, fw::sp::s5);
  const int box = fw::px(96);
  for (int i = 0; i < N_CIRCUITS; i++) {
    const Circuit &c = CIRCUITS[i];
    lv_obj_t *cell = column(g, fw::sp::s2);
    lv_obj_set_width(cell, box);
    Track tr = fit(c, box, box, fw::stroke::edge + fw::sp::s1, true);  // turned like the TV map
    fw::stroke_path(cell, box, box, tr.pts.data(), (int) tr.pts.size(), fw::stroke::edge, t.steel);
    char b[48];
    fw::text(cell, fw::ty::c_caps_sm, c.place, t.text);
    snprintf(b, sizeof b, "%d · %.1f km", i + 1, c.length_m / 1000.0f);
    caption(cell, b);
  }
  char b[96];
  snprintf(b, sizeof b, "%d circuits, %.1f KB of points on the panel", N_CIRCUITS, CIRCUIT_BYTES / 1024.0f);
  caption(page, b);
}
void car_marks(lv_obj_t *page) {
  lv_obj_t *row = wrap(page, fw::sp::s5);
  const char *kinds[][3] = {{"dot", "", "racing"}, {"dot", "lead", "leading"}, {"dot", "label", "tagged"}, {"code", "pit", "in the pits"}};
  for (auto &k : kinds) {
    lv_obj_t *cell = column(row, fw::sp::s2);
    lv_obj_set_width(cell, LV_SIZE_CONTENT);
    ArduinoJson::JsonDocument d;
    d["marker"] = k[0], d["code"] = "VER", d["num"] = 3, d["colour"] = "#4781D7";
    if (*k[1]) d[k[1]] = true;
    build("f1_car_marker", cell, d);
    caption(cell, k[2]);
  }
  ArduinoJson::JsonDocument c;
  c["track"] = "mc-1929", c["width"] = page_dp(400), c["height"] = 240, c["race_control"] = false, c["speed"] = "10x";
  build("f1_circuit", page, c);
}
void gaps(lv_obj_t *page) {
  ArduinoJson::JsonDocument s;
  s["rows"] = 8, s["width"] = page_dp(240), s["speed"] = "10x", s["stat"] = "interval";
  build("f1_standings", page, s);
  caption(page, "Gap to the leader, then the interval to the car ahead; a lapped car shows +1 lap.");
}

// ---------------------------------------------------------------- tennis
void court(lv_obj_t *page) {
  ArduinoJson::JsonDocument d;
  d["width"] = page_dp(360), d["speed"] = "10x";
  build("tennis_court", page, d);
  caption(page, "The server's dot sits behind their baseline on the half the score says; the box they serve into is shaded.");
}
void scoring(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  struct Ex { const char *feed, *what; };
  const Ex ex[] = {
    {R"j({"sets":[[0,0]],"points":[0,0],"server":0})j", "a new game"},
    {R"j({"sets":[[0,0]],"points":[1,0],"server":0})j", "one point"},
    {R"j({"sets":[[0,0]],"points":[2,1],"server":0})j", "two points to one"},
    {R"j({"sets":[[1,1]],"points":[3,2],"server":0})j", "a point from the game"},
    {R"j({"sets":[[1,1]],"points":[3,3],"server":0})j", "deuce"},
    {R"j({"sets":[[1,1]],"points":[4,3],"server":0})j", "advantage server"},
    {R"j({"sets":[[1,1]],"points":[2,3],"server":0})j", "the receiver is a point from breaking"},
    {R"j({"sets":[[5,4]],"points":[3,1],"server":0})j", "a point from the set"},
    {R"j({"sets":[[6,4],[5,3]],"points":[3,0],"server":0})j", "a point from the match"},
    {R"j({"sets":[[6,6]],"points":[3,2],"server":1})j", "tiebreak at 6-6: points count 1, 2, 3"},
    {R"j({"sets":[[6,4],[4,6],[6,6]],"points":[9,8],"server":0,"format":{"best_of":3,"final_set":"tb10"}})j", "final set: first to 10"},
    {R"j({"sets":[[6,4],[4,6],[7,7]],"points":[0,0],"server":0,"format":{"best_of":3,"final_set":"adv"}})j", "final set by advantage: no tiebreak"},
  };
  for (const Ex &e : ex) {
    ArduinoJson::JsonDocument d;
    deserializeJson(d, e.feed);
    const TennisNow n = tennis_now(d.as<ArduinoJson::JsonVariantConst>());
    lv_obj_t *row = wrap(page, fw::sp::s5);
    char b[32];
    snprintf(b, sizeof b, "%s - %s", n.label[0], n.label[1]);
    lv_obj_t *l = fw::text(row, fw::ty::title, b, t.text);
    lv_obj_set_width(l, fw::px(88));
    const char *m = n.match_point ? "Match point" : n.set_point ? "Set point" : n.break_point ? "Break point" : n.tiebreak ? "Tiebreak" : "";
    lv_obj_t *w = fw::text(row, fw::ty::c_caps_sm, *m ? m : " ", t.accent_text);
    lv_obj_set_width(w, fw::px(88));
    caption(row, e.what);
  }
}
void sets(lv_obj_t *page) {
  ArduinoJson::JsonDocument d;
  d["layout"] = "board", d["width"] = page_dp(336), d["sim"] = false;
  d["feed"] = R"({"state":"live","format":{"best_of":5},"players":[{"name":"Mara Lindqvist","seed":3},{"name":"Ayla Moreno"}],"sets":[[6,4],[3,6],[7,6],[2,1]],"points":[2,3],"server":0})";
  build("tennis_score", page, d);
  caption(page, "Finished sets in dim digits, the winner's in text; the set in play bright; the point in accent.");
}

// ---------------------------------------------------------------- football
void pitch(lv_obj_t *page, const char *jump, const char *what) {
  ArduinoJson::JsonDocument d;
  d["width"] = page_dp(400), d["speed"] = "60x", d["jump"] = jump;
  build("football_pitch", page, d);
  caption(page, what);
}
}  // namespace

FW_COMPONENT(sport, "Sport", "foundations", R"({
  "about": "The shared language of the sport screens, then each sport's own parts: F1, tennis and football.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "about": "Which part of the sport page to show, from the shared colours and marks to each sport's own parts such as F1 circuits, the tennis court or the football pitch.", "type": "select", "options": ["colours", "marks", "states", "type", "moments", "circuits", "car_marks", "gaps", "court", "scoring", "sets", "pitch", "events", "scoreline"], "default": "colours"}
  ],
  "variants": [
    {"label": "Team colours", "about": "Shared. Team colours come with the data, not from Facet. They only ever mark a car, a player or a bar, never carry text, and each is lifted towards the text colour just enough to stand out from what it sits on.", "props": {"section": "colours"}},
    {"label": "Competitor marks", "about": "Shared. A competitor is a disc in its team colour, cut out of what is behind it by a thin ring. It can carry a code or a number; the leader gets a ring of its own.", "props": {"section": "marks"}},
    {"label": "Live states", "about": "Shared. Every sport uses the same few words for where an event is. Only Live moves.", "props": {"section": "states"}},
    {"label": "Score type", "about": "Shared. One size for the thing to read first, everything else steps down. Numbers line up in their columns.", "props": {"section": "type"}},
    {"label": "Moments", "about": "Shared. A change from the data animates like a tap would, without sound: digits lift in, words light up, rows slide.", "props": {"section": "moments"}},
    {"label": "F1: circuits", "about": "Every circuit of the 2026 season, drawn from survey data in the same line as everything else: the true shape, turned the way the TV map shows it (north up is an option), the lap starting on the line and running the way the race does.", "props": {"section": "circuits"}},
    {"label": "F1: car marks", "about": "Cars glide along the track itself between updates. A car in the pits goes hollow; a retired car leaves the track. Tags that would cover another car are hidden.", "props": {"section": "car_marks"}},
    {"label": "F1: gaps", "about": "The order is the hero: position, team bar, driver, then the gap and the interval.", "props": {"section": "gaps"}},
    {"label": "Tennis: court and serve", "about": "The court from above shows what the score already says: who serves, from which side, into which box. There is no ball or player position: no public source has them.", "props": {"section": "court"}},
    {"label": "Tennis: scoring", "about": "Tennis's own rules, worked out from the points: 15, 30, 40, deuce and advantage, the tiebreak at 6-6, the final-set rule, and when a point would win the game, the set or the match.", "props": {"section": "scoring"}},
    {"label": "Tennis: set history", "about": "Finished sets stay on the board, quietly; the set in play and the point are brighter.", "props": {"section": "sets"}},
    {"label": "Football: pitch and formations", "about": "The pitch at its true proportions, both line-ups at their formation places from the team sheet. Players are never drawn at live positions: no public data has them.", "props": {"section": "pitch"}},
    {"label": "Football: events and the minute line", "about": "Goals, cards and substitutions sit on a minute line under the pitch, home above and away below, with the latest one in words.", "props": {"section": "events"}},
    {"label": "Football: scoreline", "about": "The score and the minute are the hero; possession is two thin team bars meeting under it.", "props": {"section": "scoreline"}}
  ]
})") {
  const char *s = p["section"] | "colours";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *page = column(parent, fw::sp::s5);
  lv_obj_set_style_pad_hor(page, fw::sp::s7, 0);
  if (!strcmp(s, "marks")) marks(page);
  else if (!strcmp(s, "states")) states(page);
  else if (!strcmp(s, "type")) type(page);
  else if (!strcmp(s, "moments")) moments(page);
  else if (!strcmp(s, "circuits")) circuits(page);
  else if (!strcmp(s, "car_marks")) car_marks(page);
  else if (!strcmp(s, "gaps")) gaps(page);
  else if (!strcmp(s, "court")) court(page);
  else if (!strcmp(s, "scoring")) scoring(page);
  else if (!strcmp(s, "sets")) sets(page);
  else if (!strcmp(s, "pitch")) pitch(page, "kickoff", "Sample match, made-up clubs: 4-3-3 against 4-2-3-1.");
  else if (!strcmp(s, "events")) pitch(page, "sub", "Later in the match: goals, cards and substitutions on the line.");
  else if (!strcmp(s, "scoreline")) pitch(page, "goal", "The first goal: the digits lift in, the latest event lights up.");
  else colours(page);
}
