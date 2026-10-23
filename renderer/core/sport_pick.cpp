// Match picker of the live sport screens: core/sport_pick.h (the button, the two-level sheet, data in, events out).
#include "sport_pick.h"
#include "sport.h"
#include <algorithm>
#include <string>
#include <strings.h>
#include <vector>

namespace fw::sport {
namespace {
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
const char *TABS[3] = {"live", "upcoming", "results"};
int tab_of(const char *state) {
  if (!strcmp(state, "upcoming")) return 1;
  if (!strcmp(state, "final")) return 2;
  return 0;  // live, half_time, break, delayed, suspended: in play (or about to resume)
}
bool live_state(const char *s) { return !strcmp(s, "live") || !strcmp(s, "half_time") || !strcmp(s, "break"); }
}  // namespace

struct Picker {
  lv_obj_t *root = nullptr, *btn = nullptr, *sheet = nullptr, *list = nullptr;
  char sport = 't';
  int level = 0, tab = 0;
  bool tab_set = false, open = false, live = false;
  std::string browse;              // the tournament whose matches the sheet shows
  std::string browse_name;
  std::string match, pick, event;  // props: the picked match, the button text, its tournament
  Doc data;                        // prop picker
  std::vector<lv_obj_t *> rows;    // the list's rows, in order (the sample / tournaments, or the matches of the tab)
  std::vector<std::string> ids;    // what each row stands for
  std::string sig;                 // the rows shown (level, tab, ids): same = update in place
  lv_timer_t *later = nullptr;     // list changes asked for from inside a row's own tap: next turn
  bool want_rows = false, want_close = false;
  int delay = 0;                   // prop delay: how far the TV runs behind the data, s (the host sends the feed that late)
  bool paused = false;             // TV sync pause: the screen holds its feed
  uint32_t paused_at = 0;
  bool syncing = false;            // sync mode: the bar is up
  lv_obj_t *bar = nullptr, *pause_btn = nullptr, *readout = nullptr, *sync_row = nullptr;
  lv_timer_t *idle = nullptr;      // sync mode hides itself after 30 s untouched
  lv_obj_t *chip = nullptr;        // the header's "paused +12 s" (Status chip), hidden unless paused
  lv_timer_t *tick = nullptr;      // the paused counter, 1 s
};

namespace {
std::string surname(const char *n) {  // "I. Jovic" -> Jovic; "Cash / Glasspool" -> Cash/Glasspool
  std::string s = n ? n : "", out;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find('/', a);
    if (b == std::string::npos) b = s.size();
    std::string part = s.substr(a, b - a);
    while (!part.empty() && part.back() == ' ') part.pop_back();
    const size_t sp = part.rfind(' ');
    if (!out.empty()) out += "/";
    out += sp == std::string::npos ? part : part.substr(sp + 1);
    a = b + 1;
  }
  return out;
}
std::string sets_text(ArduinoJson::JsonArrayConst sets) {
  std::string t;
  for (ArduinoJson::JsonArrayConst s : sets) {
    char b[16];
    snprintf(b, sizeof b, "%d-%d", (int) (s[0] | 0), (int) (s[1] | 0));
    if (!t.empty()) t += " ";
    t += b;
  }
  return t;
}
// a match row's words: name "Jovic v Shnaider", status by state, tone working while live
void match_words(const Picker *k, ArduinoJson::JsonObjectConst m, std::string &name, std::string &status, bool &working) {
  ArduinoJson::JsonArrayConst pl = m["players"];
  name = (k->sport == 't' ? surname(pl[0] | "") : std::string(pl[0] | "")) + " v " + (k->sport == 't' ? surname(pl[1] | "") : std::string(pl[1] | ""));
  const char *st = m["state"] | "upcoming";
  working = live_state(st);
  char b[96];
  if (!strcmp(st, "upcoming")) {
    if (*(m["starts"] | "")) snprintf(b, sizeof b, "starts %s", m["starts"] | "");
    else snprintf(b, sizeof b, "upcoming");
    status = b;
    return;
  }
  const char *word = !strcmp(st, "final") ? "final" : !strcmp(st, "half_time") ? "half time" : !strcmp(st, "break") ? "changeover"
                   : !strcmp(st, "delayed") ? "delayed" : !strcmp(st, "suspended") ? "suspended" : "live";
  status = word;
  if (k->sport == 't') {
    const std::string s = sets_text(m["sets"]);
    if (!s.empty()) status += " \xC2\xB7 " + s;
    if (working && m["points"].is<ArduinoJson::JsonArrayConst>()) {  // the point, labelled by the tennis rules
      Doc f;
      f["sets"] = m["sets"], f["points"] = m["points"], f["state"] = "live";
      const TennisNow n = tennis_now(f.as<ArduinoJson::JsonVariantConst>());
      if (*n.label[0]) { snprintf(b, sizeof b, " \xC2\xB7 %s-%s", n.label[0], n.label[1]); status += b; }
    }
  } else {
    snprintf(b, sizeof b, " \xC2\xB7 %d-%d", (int) (m["score"][0] | 0), (int) (m["score"][1] | 0));
    status += b;
    if (!strcmp(st, "live") && (m["clock"]["min"] | 0) > 0) {
      if ((m["clock"]["added"] | 0) > 0) snprintf(b, sizeof b, " \xC2\xB7 %d+%d'", (int) (m["clock"]["min"] | 0), (int) (m["clock"]["added"] | 0));
      else snprintf(b, sizeof b, " \xC2\xB7 %d'", (int) (m["clock"]["min"] | 0));
      status += b;
    }
  }
}
bool have_matches(const Picker *k) { return k->data["event"]["id"].is<const char *>() && k->browse == (k->data["event"]["id"] | ""); }
// the matches of the tab, in the host's order
std::vector<ArduinoJson::JsonObjectConst> tab_matches(const Picker *k, int tab) {
  std::vector<ArduinoJson::JsonObjectConst> v;
  if (!have_matches(k)) return v;
  for (ArduinoJson::JsonObjectConst m : k->data["matches"].as<ArduinoJson::JsonArrayConst>())
    if (tab_of(m["state"] | "upcoming") == tab) v.push_back(m);
  return v;
}
void default_tab(Picker *k) {
  if (k->tab_set || !have_matches(k)) return;
  k->tab = !tab_matches(k, 0).empty() ? 0 : !tab_matches(k, 1).empty() ? 1 : 2;
}
std::string rows_sig(const Picker *k) {
  std::string s = k->level ? "m|" + k->browse + "|" + TABS[k->tab] + "|" : "t|";
  if (k->level) {
    if (!have_matches(k)) return s + "loading";
    for (ArduinoJson::JsonObjectConst m : tab_matches(k, k->tab)) s += std::string(m["id"] | "") + ",";
  } else {
    if (!k->data["tournaments"].is<ArduinoJson::JsonArrayConst>()) return s + "loading";
    for (ArduinoJson::JsonObjectConst t : k->data["tournaments"].as<ArduinoJson::JsonArrayConst>()) s += std::string(t["id"] | "") + ",";
  }
  return s;
}
int n_rows(const Picker *k) {  // + the Sync with TV row on top of both levels
  if (k->level) {  // the tallest tab: the tabs stay put while switching (a bottom-anchored panel would move them)
    int n = 1;
    for (int t = 0; t < 3; t++) n = std::max(n, (int) tab_matches(k, t).size());
    return 2 + n;
  }
  return 2 + (k->data["tournaments"].is<ArduinoJson::JsonArrayConst>() ? (int) k->data["tournaments"].size() : 1);
}
void tournament_props(ArduinoJson::JsonObjectConst t, Doc &d) {
  char b[96];
  const char *tour = t["tour"] | "";
  snprintf(b, sizeof b, *tour ? "%s \xC2\xB7 %s" : "%s%s", t["name"] | "", tour);
  d["title"] = b, d["icon"] = "none", d["trailing"] = "chevron";
  const int live = t["live"] | 0, count = t["count"] | 0;
  if (live) snprintf(b, sizeof b, "%d live \xC2\xB7 %d today", live, count);
  else snprintf(b, sizeof b, "%d today", count);
  d["sub"] = b;
}
void match_props(const Picker *k, ArduinoJson::JsonObjectConst m, Doc &d) {
  std::string name, status;
  bool working = false;
  match_words(k, m, name, status, working);
  d["icon"] = "none", d["name"] = name, d["status"] = status, d["tone"] = working ? "working" : "dim";
  d["selected"] = k->match == (m["id"] | "");
}
void sample_props(const Picker *k, Doc &d) {
  d["icon"] = "none", d["name"] = "Sample match", d["status"] = "the simulator \xC2\xB7 made-up players", d["tone"] = "dim";
  d["selected"] = k->match.empty();
}
const char *sheet_title(const Picker *k) {
  if (k->level) return k->browse_name.empty() ? "matches" : k->browse_name.c_str();
  return k->sport == 't' ? "tournaments" : "competitions";
}
void sheet_props(const Picker *k, bool open, Doc &d) {
  d["overlay"] = true, d["title"] = sheet_title(k), d["open"] = open ? "open" : "closed";
  d["rows"] = n_rows(k), d["row_h"] = fw::lay::sheet_row_h;
}
void loading_row(lv_obj_t *list, const char *txt) {
  lv_obj_t *l = fw::text(list, fw::ty::body, txt, fw::T->text_dim);
  lv_obj_set_style_margin_top(l, fw::lay::section_gap, 0);
}
int paused_s(const Picker *k) { return k->paused ? (int) (lv_tick_elaps(k->paused_at) / 1000) : 0; }
std::string delay_text(const Picker *k) {
  char b[48];
  snprintf(b, sizeof b, "TV +%d s", k->delay);
  return b;
}
void sync_mode(Picker *k, bool on);
void show_delay(Picker *k) {
  const std::string d = delay_text(k);
  if (k->sync_row) {
    Doc r;
    r["title"] = "Sync with TV", r["sub"] = d, r["icon"] = "tv", r["trailing"] = "chevron";
    fw::update_child(fw::find("list_row"), k->sync_row, r.as<fw::Props>());
  }
  if (k->readout) fw::set_text(k->readout, fw::ty::label, d.c_str());
  if (k->chip) {
    char b[32];
    snprintf(b, sizeof b, "paused +%d s", paused_s(k));
    Doc c;
    c["tone"] = "warn", c["text"] = b;
    fw::update_child(fw::find("status_chip"), k->chip, c.as<fw::Props>());
  }
}
void set_delay(Picker *k, int d) {
  k->delay = d < 0 ? 0 : d > 300 ? 300 : d;
  show_delay(k);
}
void pause_props(const Picker *k, Doc &b) { b["kind"] = "primary", b["label"] = k->paused ? "Play" : "Pause", b["width"] = 88; }
void idle_restart(Picker *k) {  // the bar hides itself after sync.idle with nothing touched (never while paused)
  if (k->idle) lv_timer_reset(k->idle);
}
void set_paused(Picker *k, bool on) {
  if (on == k->paused) return;
  if (!on) k->delay = std::min(300, k->delay + paused_s(k));  // Play: the delay grows by the time spent paused
  k->paused = on;
  k->paused_at = lv_tick_get();
  if (k->chip) {
    if (on) lv_obj_remove_flag(k->chip, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(k->chip, LV_OBJ_FLAG_HIDDEN);
  }
  if (on && !k->tick) k->tick = lv_timer_create([](lv_timer_t *t) { show_delay((Picker *) lv_timer_get_user_data(t)); }, 1000, k);
  if (!on && k->tick) { lv_timer_delete(k->tick); k->tick = nullptr; }
  if (k->pause_btn) {
    Doc b;
    pause_props(k, b);
    fw::update_child(fw::find("button"), k->pause_btn, b.as<fw::Props>());
  }
  show_delay(k);
  fw::emit(k->root, on ? "pause" : "play", "");
  if (!on) fw::emit(k->root, "delay", std::to_string(k->delay).c_str());
  idle_restart(k);
}
void on_bar(lv_obj_t *, const char *name, const char *detail, void *user) {
  auto *k = (Picker *) user;
  if (strcmp(name, "tap")) return;
  idle_restart(k);
  int step = 0;
  if (strchr("+-", detail[0]) && sscanf(detail, "%d", &step) == 1) {  // -5 / -1 / +1 / +5 s
    set_delay(k, k->delay + step);
    if (!k->paused) fw::emit(k->root, "delay", std::to_string(k->delay).c_str());
  } else if (!strcasecmp(detail, "Pause") || !strcasecmp(detail, "Play")) set_paused(k, !strcasecmp(detail, "Pause"));
  else if (!strcasecmp(detail, "Done")) sync_mode(k, false);
}
// sync mode (Darren 2026-10-23): a slim bar along the bottom over the screen (over the court, never the hero score):
// Pause / Play (primary Button, wide), the readout "TV +38 s", -5 / -1 / +1 / +5 s (480: -1 / +1) and Done. The bar
// exists always and is hidden outside sync mode.
void build_bar(Picker *k) {
  const fw::Theme &t = *fw::T;
  const int h = fw::touch::button_h + 2 * fw::sp::s2;  // 80 / 52: the Buttons centred on the grid
  lv_obj_t *bar = lv_obj_create(k->root);
  lv_obj_remove_style_all(bar);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bar, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN));
  lv_obj_set_pos(bar, 0, fw::lay::screen_h - h);
  lv_obj_set_size(bar, fw::lay::screen_w, h);
  lv_obj_set_style_bg_color(bar, t.bg, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(bar, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(bar, t.line_hi, 0);
  lv_obj_set_style_pad_hor(bar, fw::lay::inset, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_top(bar, fw::sp::s2 - fw::stroke::hair, 0);  // the hairline + this = space 2: Buttons on the grid
  lv_obj_set_style_pad_column(bar, fw::sp::s2, 0);
  fw::qa::free(bar, "overlay: the TV sync bar over the bottom of the screen (sync mode)");
  Doc b;
  pause_props(k, b);
  k->pause_btn = build("button", bar, b);
  k->readout = fw::text(bar, fw::ty::label, delay_text(k).c_str(), t.text);
  lv_obj_set_width(k->readout, 0);
  lv_obj_set_flex_grow(k->readout, 1);
  lv_obj_set_style_text_align(k->readout, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(k->readout, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_height(k->readout, fw::style(fw::ty::label).box);
  lv_obj_set_style_margin_top(k->readout, (fw::touch::button_h - fw::style(fw::ty::label).box) / 2 / fw::sp::GRID * fw::sp::GRID, 0);
  const bool wide = fw::lay::screen_w >= 720;
  for (const char *l : {"-5", "-1", "+1", "+5"}) {
    if (!wide && (l[1] == '5')) continue;
    Doc d;
    d["kind"] = "outline", d["label"] = l;
    build("button", bar, d);
  }
  Doc dn;
  dn["kind"] = "secondary", dn["label"] = "Done";
  build("button", bar, dn);
  fw::listen(bar, on_bar, k);
  k->bar = bar;
}
void sync_mode(Picker *k, bool on) {
  if (!k->bar) return;
  if (!on && k->paused) set_paused(k, false);  // Done while paused = Play first: nothing is lost
  k->syncing = on;
  if (on) lv_obj_remove_flag(k->bar, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(k->bar, LV_OBJ_FLAG_HIDDEN);
  if (on && !k->idle)
    k->idle = lv_timer_create([](lv_timer_t *t) {
      auto *k = (Picker *) lv_timer_get_user_data(t);
      if (!k->paused) sync_mode(k, false);
    }, 30000, k);
  if (!on && k->idle) { lv_timer_delete(k->idle); k->idle = nullptr; }
  fw::emit(k->root, "sync", on ? "on" : "off");
}
// the list (bottom_sheet host rows: LV_EVENT_REFRESH, the list is the param)
void fill_rows(Picker *k, lv_obj_t *list) {
  k->list = list;
  k->rows.clear(), k->ids.clear();
  k->sig = rows_sig(k);
  k->sync_row = nullptr;
  {  // the first row of both levels: Sync with TV (the current delay under it): closes the sheet, sync mode
    Doc r;
    r["title"] = "Sync with TV", r["sub"] = delay_text(k), r["icon"] = "tv", r["trailing"] = "chevron";
    k->sync_row = build("list_row", list, r);
  }
  if (!k->level) {
    Doc d;
    sample_props(k, d);
    if (lv_obj_t *r = build("picker_row", list, d)) k->rows.push_back(r), k->ids.push_back("");
    if (!k->data["tournaments"].is<ArduinoJson::JsonArrayConst>()) {
      loading_row(list, k->data["error"].is<const char *>() ? "No live data right now." : "Loading\xE2\x80\xA6");
      return;
    }
    for (ArduinoJson::JsonObjectConst t : k->data["tournaments"].as<ArduinoJson::JsonArrayConst>()) {
      Doc r;
      tournament_props(t, r);
      if (lv_obj_t *o = build("list_row", list, r)) k->rows.push_back(o), k->ids.push_back(t["id"] | "");
    }
    return;
  }
  // the matches: back + the tabs, then the tab's matches
  lv_obj_t *bar = lv_obj_create(list);
  lv_obj_remove_style_all(bar);
  lv_obj_remove_flag(bar, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(bar, LV_PCT(100), fw::lay::sheet_row_h);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, fw::lay::gutter, 0);
  Doc bp;
  bp["style"] = "square", bp["icon"] = "back";
  build("icon_button", bar, bp);
  Doc sp;
  sp["options"] = "Live, Upcoming, Results", sp["selected"] = TABS[k->tab];
  build("segmented", bar, sp);
  if (!have_matches(k)) { loading_row(list, "Loading\xE2\x80\xA6"); return; }
  const std::vector<ArduinoJson::JsonObjectConst> ms = tab_matches(k, k->tab);
  if (ms.empty()) {
    loading_row(list, k->tab == 0 ? "Nothing live here now." : k->tab == 1 ? "Nothing more today." : "No results yet today.");
    return;
  }
  for (ArduinoJson::JsonObjectConst m : ms) {
    Doc d;
    match_props(k, m, d);
    if (lv_obj_t *r = build("picker_row", list, d)) k->rows.push_back(r), k->ids.push_back(m["id"] | "");
  }
}
// the sheet's rows again (or in place when the same rows), open or closed as asked
void show(Picker *k, bool open) {
  if (!k->sheet) return;
  if (open && k->open && k->list && rows_sig(k) == k->sig) {  // the same rows: each one's own update path
    if (!k->level) {
      Doc d;
      sample_props(k, d);
      if (!k->rows.empty()) fw::update_child(fw::find("picker_row"), k->rows[0], d.as<fw::Props>());
      size_t i = 1;
      for (ArduinoJson::JsonObjectConst t : k->data["tournaments"].as<ArduinoJson::JsonArrayConst>()) {
        if (i >= k->rows.size()) break;
        Doc r;
        tournament_props(t, r);
        fw::update_child(fw::find("list_row"), k->rows[i++], r.as<fw::Props>());
      }
    } else {
      size_t i = 0;
      for (ArduinoJson::JsonObjectConst m : tab_matches(k, k->tab)) {
        if (i >= k->rows.size()) break;
        Doc d;
        match_props(k, m, d);
        fw::update_child(fw::find("picker_row"), k->rows[i++], d.as<fw::Props>());
      }
    }
    return;
  }
  Doc d;
  sheet_props(k, open, d);
  k->open = open;
  fw::update_child(fw::find("bottom_sheet"), k->sheet, d.as<fw::Props>());
}
void run_later(lv_timer_t *t) {
  auto *k = (Picker *) lv_timer_get_user_data(t);
  k->later = nullptr;
  const bool rows = k->want_rows, close = k->want_close;
  k->want_rows = k->want_close = false;
  if (close) show(k, false);
  else if (rows) { k->sig.clear(); show(k, true); }
}
void later(Picker *k, bool close) {
  (close ? k->want_close : k->want_rows) = true;
  if (k->later) return;
  k->later = lv_timer_create(run_later, 0, k);
  lv_timer_set_repeat_count(k->later, 1);
  lv_timer_set_auto_delete(k->later, true);
}
void button(Picker *k) {
  if (!k->btn) return;
  Doc bp;
  bp["name"] = k->pick.empty() ? "sample" : k->pick.c_str();
  bp["playing"] = k->live;
  bp["icon"] = k->sport == 't' ? "sport-tennis-ball" : "sport-goal";
  fw::update_child(fw::find("speaker_button"), k->btn, bp.as<fw::Props>());
}
void take(Picker *k, fw::Props p) {
  k->match = p["match"] | "";
  k->pick = p["pick"] | "";
  k->event = p["event"] | "";
  if (!k->paused) set_delay(k, p["delay"] | 0);  // while paused the screen owns the delay
  k->data.clear();
  ArduinoJson::JsonVariantConst v = p["picker"];
  if (v.is<const char *>()) {
    if (*(v.as<const char *>()) && deserializeJson(k->data, v.as<const char *>())) k->data.clear();
  } else if (v.is<ArduinoJson::JsonObjectConst>()) k->data.set(v);
  if (k->level && have_matches(k) && k->browse_name.empty()) k->browse_name = k->data["event"]["name"] | "";
}
// a row of the sheet that `o` is (or is inside), -1 none
int row_index(Picker *k, lv_obj_t *o) {
  for (lv_obj_t *a = o; a && a != k->sheet; a = lv_obj_get_parent(a))
    for (size_t i = 0; i < k->rows.size(); i++)
      if (k->rows[i] == a) return (int) i;
  return -1;
}
void on_sheet(lv_obj_t *from, const char *name, const char *detail, void *user) {
  auto *k = (Picker *) user;
  if (!strcmp(name, "close")) { k->open = false; return; }
  if (!strcmp(name, "change")) {  // a tab
    for (int i = 0; i < 3; i++)
      if (!strcmp(detail, TABS[i])) k->tab = i;
    k->tab_set = true;
    later(k, false);
    return;
  }
  if (strcmp(name, "tap")) return;
  if (k->sync_row && (from == k->sync_row || lv_obj_get_parent(from) == k->sync_row)) {  // Sync with TV
    later(k, true);
    sync_mode(k, true);
    return;
  }
  if (!strcmp(detail, "back") && k->level) {  // the Icon button left of the tabs: back to the tournaments
    k->level = 0;
    fw::emit(k->root, "browse", "tournaments");
    later(k, false);
    return;
  }
  const int i = row_index(k, from);
  if (i < 0) return;
  if (!k->level && i > 0) {  // a tournament: its matches
    k->level = 1, k->tab = 0, k->tab_set = false;
    k->browse = k->ids[i];
    k->browse_name.clear();
    for (ArduinoJson::JsonObjectConst t : k->data["tournaments"].as<ArduinoJson::JsonArrayConst>())
      if (k->browse == (t["id"] | "")) {
        Doc d;
        tournament_props(t, d);
        k->browse_name = d["title"] | "";
      }
    default_tab(k);
    const std::string ev = "event " + k->browse;
    fw::emit(k->root, "browse", ev.c_str());
    later(k, false);
    return;
  }
  // a match (or the sample): the pick; the button shows it at once, the host sends the feed
  k->match = k->ids[i];
  k->pick = k->match.empty() ? "" : k->browse_name;
  k->event = k->match.empty() ? "" : k->browse;
  if (k->match.empty()) k->live = false;
  button(k);
  fw::emit(k->root, "pick", k->match.c_str());
  later(k, true);
}
}  // namespace

Picker *picker_attach(lv_obj_t *root, lv_obj_t *header, char sport, fw::Props p) {
  auto *k = new Picker;
  k->root = root, k->sport = sport;
  take(k, p);

  // the button in the time's place (docs header clock: false, as Media): the chrome stays the last child, hidden
  lv_obj_t *chrome = lv_obj_get_child_count(header) ? lv_obj_get_child(header, -1) : nullptr;
  Doc bp;
  bp["name"] = k->pick.empty() ? "sample" : k->pick.c_str();
  bp["playing"] = false;
  bp["icon"] = sport == 't' ? "sport-tennis-ball" : "sport-goal";
  k->btn = build("speaker_button", header, bp);
  if (chrome) { lv_obj_add_flag(chrome, LV_OBJ_FLAG_HIDDEN); lv_obj_move_to_index(chrome, -1); }
  // the paused state (TV sync): a Status chip before the button, hidden unless paused
  Doc cp;
  cp["tone"] = "warn", cp["text"] = "paused +0 s";
  if ((k->chip = build("status_chip", header, cp))) {
    lv_obj_add_flag(k->chip, LV_OBJ_FLAG_HIDDEN);
    if (k->btn) lv_obj_move_to_index(k->chip, lv_obj_get_index(k->btn));
  }
  build_bar(k);  // under the sheet
  Doc sd;
  sheet_props(k, false, sd);
  k->sheet = build("bottom_sheet", root, sd);
  if (k->sheet) {
    lv_obj_set_pos(k->sheet, 0, 0);
    lv_obj_set_size(k->sheet, fw::lay::screen_w, fw::lay::screen_h);
    fw::qa::free(k->sheet, "overlay: the match picker sheet covers the whole screen");
    lv_obj_add_event_cb(k->sheet, [](lv_event_t *e) {
      fill_rows((Picker *) lv_event_get_user_data(e), (lv_obj_t *) lv_event_get_param(e));
    }, LV_EVENT_REFRESH, k);
    fw::listen(k->sheet, on_sheet, k);
  }
  if (k->btn)
    lv_obj_add_event_cb(k->btn, [](lv_event_t *e) {  // after the button's own tap (event + open sound): the sheet
      auto *k = (Picker *) lv_event_get_user_data(e);
      if (!k->sheet || lv_obj_has_state(k->btn, fw::st::OFF)) return;
      if (!k->event.empty() && !k->match.empty()) {  // a pick: its tournament's matches
        if (k->level != 1 || k->browse != k->event) k->tab_set = false;
        k->level = 1, k->browse = k->event, k->browse_name = k->pick;
        default_tab(k);
        const std::string ev = "event " + k->browse;
        fw::emit(k->root, "browse", ev.c_str());
      } else {
        k->level = 0;
        fw::emit(k->root, "browse", "tournaments");
      }
      k->sig.clear();
      show(k, true);
    }, LV_EVENT_CLICKED, k);
  return k;
}
void picker_update(Picker *k, fw::Props p) {
  if (!k) return;
  const std::string was_pick = k->pick;
  take(k, p);
  default_tab(k);
  if (k->pick != was_pick) button(k);

  if (k->open) show(k, true);
}
bool picker_paused(const Picker *k) { return k && k->paused; }
void picker_hero(Picker *k, lv_obj_t *hero) {  // in sync mode a tap on the hero toggles Pause / Play
  if (!k || !hero) return;
  lv_obj_add_flag(hero, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(hero, [](lv_event_t *e) {
    auto *k = (Picker *) lv_event_get_user_data(e);
    if (!k->syncing) return;
    fw::sound(k->root, k->paused ? "toggle-on" : "toggle-off");
    set_paused(k, !k->paused);
  }, LV_EVENT_CLICKED, k);
}
void picker_live(Picker *k, bool live) {
  if (!k || live == k->live) return;
  k->live = live;
  button(k);
}
void picker_free(Picker *k) {
  if (!k) return;
  if (k->later) lv_timer_delete(k->later);
  if (k->tick) lv_timer_delete(k->tick);
  if (k->idle) lv_timer_delete(k->idle);
  delete k;
}
}  // namespace fw::sport
