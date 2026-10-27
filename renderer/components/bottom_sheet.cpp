// Bottom sheet: the Facet bottom sheet (docs: Components > Pickers > Bottom sheet, @facet/ui pickers.ts bottomSheet(),
// CSS .f-bsheet), on the contract (renderer/CONTRACT.md, "contract": 2): a choose-one list over the screen (the Media
// speaker picker, Settings pickers, the Covers kinds). The overlay layer (scrim, panel, its height, open / close
// motion, scrim tap and the panel key) is the core's: core/sheet.h. This file fills the panel with real components:
//   head   a row as tall as Close (ibtn-square = layout row): the title (type label, caps, text-dim, the free width, "...") + Close = the Icon button
//          (fw::find, square, role close: event "close" detail "close" + sound back), which closes the sheet.
//          Docs: head at y 128 / 84
//   list   the free height, scrolls vertically; Picker rows (fw::find("picker_row")), sheet-row-h each; picker rows
//          that share a parent are one group, so a tap moves the radio by itself; the sheet adds "close after a pick"
//          (the pick's own sound is the List row's tap). Docs: list from y 192 / 136
// States: open / closed (the core's motion). n/a: pressed, disabled, unavailable, loading, unknown, empty, error (its
// rows carry their own). Themed through the live palette (bg, line-hi, scrim) and its rows' parts.
// Workbench page (meta "stage": "screen": the whole stage, not a page's content box): the sheet over a real-component
// screen (Page: the Header + List rows, "Settings"); the Speaker and
// Theme rows open their sheets (their chevron tap plays open), a pick updates the row's value. Props: open (open = it
// opens on build, so the 1.8 s demo reset replays it; closed = tap a row), list, title, selected, screen. Internal:
// `items` (array of {icon, name, status}: the caller's own list instead of the built-in ones; a pick sends
// LV_EVENT_VALUE_CHANGED to the overlay, param = the index; with `selected` = the index ringed, `stay` = a pick keeps
// it open; the Settings screen uses this), `rows` + `row_h` (host rows: the caller's own rows, `rows` of `row_h` px
// for the panel's height; every fill sends LV_EVENT_REFRESH to the overlay with the list as the param, the caller
// builds into it; the Calendar's calendars and day sheets), `loop` (opens and closes forever: the frame-rate bench),
// `overlay` (only the overlay, built into the caller's screen; the Layout foundation's overlay sample) with `still`
// (shown open, no animation), `flatten` / `panel: solid` (bench options, core/sheet.cpp).
// Contrast pairs: tokens/colour.json section "bottom_sheet" (+ picker_row, radio, list_row, icon_button, header).
#include "fw.h"
#include "fx.h"
#include "sheet.h"

namespace {
using Doc = ArduinoJson::JsonDocument;

lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}

// the lists the workbench can open (the docs explorer uses the house's players; the Settings screen has a theme picker)
struct Option { const char *icon, *name, *status, *tone; };
const Option SPEAKERS[] = {
    {"auto", "Auto", "follow what's playing", "dim"},
    {"speaker", "Kitchen speaker", "playing · Northbound", "working"},
    {"tv", "Living room TV", "idle", "dim"},
    {"speaker", "Study", "paused", "dim"},
    {"speakers", "Downstairs", "group · 3 speakers", "dim"},
};
const Option THEMES[] = {
    {"none", "Dark", "always", "dim"},
    {"none", "Light", "always", "dim"},
    {"none", "Auto", "light by day, dark at night", "dim"},
};
struct List { const char *id, *title; const Option *opts; int n; };
const List LISTS[] = {{"speakers", "speaker", SPEAKERS, 5}, {"theme", "theme", THEMES, 3}};

struct Sheet {
  lv_obj_t *ov = nullptr, *title = nullptr, *list = nullptr;
  lv_timer_t *loop = nullptr;
  int which = 0, sel[2] = {0, 0};
  std::string custom_title;
  lv_obj_t *trigger[2] = {nullptr, nullptr};  // the screen rows that open each list (their value shows the pick)
  // items (a caller's own list, e.g. the Settings screen): replaces the built-in lists; a pick sends
  // LV_EVENT_VALUE_CHANGED to the overlay with the index as the param; stay = a pick does not close (sound packs)
  std::vector<std::string> items;  // icon, name, status per item
  int item_sel = 0;
  bool custom = false, stay = false;
  // host rows (the Calendar's calendars and day sheets): the caller fills the list itself with `rows` rows of `row_h`
  int host_rows = -1, host_row_h = 0;
};
int n_opts(const Sheet *s) { return s->host_rows >= 0 ? s->host_rows : s->custom ? (int) s->items.size() / 3 : LISTS[s->which].n; }
Option opt(const Sheet *s, int i) {
  if (!s->custom) return LISTS[s->which].opts[i];
  return {s->items[3 * i].c_str(), s->items[3 * i + 1].c_str(), s->items[3 * i + 2].c_str(), "dim"};
}

// the screen's rows (List rows): Speaker / Theme show the pick as their value
struct R { const char *title, *icon, *trailing; bool on; };
const R ROWS[] = {
    {"Speaker", "speaker", "value-chevron", false},
    {"Theme", "bulb", "value-chevron", false},
    {"Chime", "volume", "toggle", true},
    {"Television", "tv", "chevron", false},
};
void row_props(int i, int sel, Doc &d) {
  d["title"] = ROWS[i].title;
  d["icon"] = ROWS[i].icon;
  d["trailing"] = ROWS[i].trailing;
  d["value"] = i == 0 ? SPEAKERS[std::min(std::max(sel, 0), 4)].name : i == 1 ? THEMES[std::min(std::max(sel, 0), 2)].name : "";
  d["on"] = ROWS[i].on;
}
void option_props(const Sheet *s, int i, int sel, Doc &d) {
  const Option o = opt(s, i);
  d["icon"] = o.icon;
  d["name"] = o.name;
  d["status"] = o.status;
  d["tone"] = o.tone;
  d["selected"] = i == sel;
}

void fill_list(Sheet *s) {
  fx::set_text(s->title, s->custom_title.empty() ? LISTS[s->which].title : s->custom_title.c_str(), "type:label");
  fw::sheet::fit(s->ov, n_opts(s), s->host_rows >= 0 ? s->host_row_h : 0);
  lv_obj_clean(s->list);
  lv_obj_scroll_to_y(s->list, 0, LV_ANIM_OFF);
  if (s->host_rows >= 0) {  // host rows: the caller builds them (LV_EVENT_REFRESH on the overlay, param = the list)
    lv_obj_send_event(s->ov, LV_EVENT_REFRESH, s->list);
    return;
  }
  for (int i = 0; i < n_opts(s); i++) {
    Doc d;
    option_props(s, i, s->custom ? s->item_sel : s->sel[s->which], d);
    lv_obj_t *r = build("picker_row", s->list, d);
    if (!r) continue;
    // after the Picker row's own handlers (List row tap + sound, the radio group): remember the pick, show it on the
    // screen row that opened the list, close
    lv_obj_add_event_cb(r, [](lv_event_t *e) {
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      auto *s = (Sheet *) lv_event_get_user_data(e);
      if (lv_obj_has_state(o, fw::st::OFF) || !fw::sheet::is_open(s->ov)) return;
      const int i = (int) lv_obj_get_index(o);
      if (s->custom) {
        s->item_sel = i;
        lv_obj_send_event(s->ov, LV_EVENT_VALUE_CHANGED, (void *) (intptr_t) i);
        if (!s->stay) fw::sheet::close(s->ov);
        return;
      }
      if (i < 0 || i >= LISTS[s->which].n) return;
      s->sel[s->which] = i;
      if (lv_obj_t *row = s->trigger[s->which]) {  // the screen row's value = the pick (the List row's update)
        Doc d;
        row_props(s->which, i, d);
        fw::update_child(fw::find("list_row"), row, d.as<fw::Props>());
      }
      fw::sheet::close(s->ov);
    }, LV_EVENT_CLICKED, s);
  }
}
void open(Sheet *s, int which, bool still = false) {
  s->which = which;
  if (!fw::sheet::is_open(s->ov)) fill_list(s);
  fw::sheet::open(s->ov, still);
}

// the props that pick what the sheet lists: title, selected, items (+ stay)
void choices(Sheet *s, fw::Props p) {
  s->custom_title = p["title"] | "";
  const int sel = p["selected"] | 0;
  s->sel[0] = std::min(std::max(sel, 0), LISTS[0].n - 1);
  s->sel[1] = std::min(std::max(sel, 0), LISTS[1].n - 1);
  s->items.clear();
  s->custom = false;
  s->host_rows = p["rows"].is<int>() ? std::max(0, p["rows"].as<int>()) : -1;
  s->host_row_h = p["row_h"] | 0;
  if (p["items"].is<ArduinoJson::JsonArrayConst>()) {
    s->custom = true;
    for (ArduinoJson::JsonObjectConst it : p["items"].as<ArduinoJson::JsonArrayConst>()) {
      s->items.push_back(it["icon"] | "none");
      s->items.push_back(it["name"] | "");
      s->items.push_back(it["status"] | "");
    }
    s->item_sel = std::min(std::max(sel, 0), std::max(0, n_opts(s) - 1));
    s->stay = p["stay"] | false;
  }
}

// the overlay (the core's layer) on `host`, closed, its panel filled: head + list
Sheet *sheet(lv_obj_t *host, fw::Props p) {
  auto *s = new Sheet;
  choices(s, p);
  s->ov = fw::sheet::make(host, p["flatten"] | false, !strcmp(p["panel"] | "fade", "solid"));
  lv_obj_set_user_data(s->ov, s);
  lv_obj_t *panel = fw::sheet::panel(s->ov);

  lv_obj_t *head = fx::box(panel, "row");  // as tall as the Close button (ibtn-square = layout row, 48 / 32)
  s->title = fx::text(head, "", "type:label", "fg:text-dim");
  fx::use(s->title, "grow-w");
  Doc cp;
  cp["style"] = "square";
  cp["role"] = "close";
  if (lv_obj_t *cb = build("icon_button", head, cp))
    lv_obj_add_event_cb(cb, [](lv_event_t *e) {  // after the Icon button's own tap (event close + sound back)
      auto *b = (lv_obj_t *) lv_event_get_current_target(e);
      if (!lv_obj_has_state(b, fw::st::OFF)) fw::sheet::close(((Sheet *) lv_event_get_user_data(e))->ov);
    }, LV_EVENT_CLICKED, s);

  s->list = fx::box(panel, "col");
  fx::use(s->list, "grow");
  lv_obj_add_flag(s->list, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // clickable: a drag in a gap scrolls too
  lv_obj_set_scroll_dir(s->list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s->list, LV_SCROLLBAR_MODE_OFF);  // the Facet scroll track is its own part (later)

  lv_obj_add_event_cb(s->ov, [](lv_event_t *e) {
    auto *s = (Sheet *) lv_event_get_user_data(e);
    if (s->loop) lv_timer_delete(s->loop);
    delete s;
  }, LV_EVENT_DELETE, s);
  return s;
}

// what needs the screen again (fw::refill); the rest (open, list, title, selected, items) moves in place
std::string sig(fw::Props p) {
  return std::string((p["screen"] | true) ? "s" : "") + ((p["loop"] | false) ? "l" : "") + ((p["flatten"] | false) ? "f" : "") +
         (p["panel"] | "fade") + ((p["still"] | false) ? "t" : "");
}
// the screen (root = its box; nullptr = made on parent) + the sheet over it; returns the sheet
Sheet *fill(lv_obj_t *parent, lv_obj_t *root, fw::Props p) {
  const bool overlay = p["overlay"] | false;
  // the screen: the caller's (overlay), else a Page of real components (Header + List rows) filling the frame
  lv_obj_t *host = parent;
  lv_obj_t *speaker_row = nullptr, *theme_row = nullptr;
  const int which = !strcmp(p["list"] | "speakers", "theme") ? 1 : 0;
  if (!overlay) {
    host = root ? root : fx::box(parent, "free");  // the screen: the Page, then the overlay over all of it
    for (const char *t : {"fill", "bg:bg"}) fx::use(host, t);
    fx::name(host, "bottom_sheet");
    fw::shape(host, sig(p));
    if (p["screen"] | true) {
      lv_obj_t *page = fx::box(host, "free");
      fx::use(page, "page");
      lv_obj_t *ph = fx::box(page, "free");
      fx::use(ph, "page-head");
      Doc hd;
      hd["label"] = "Settings";
      build("header", ph, hd);
      lv_obj_t *body = fx::box(page, "free");
      for (const char *t : {"body", "body:list"}) fx::use(body, t);
      const int sel = p["selected"] | 0;
      for (int i = 0; i < 4; i++) {
        Doc d;
        row_props(i, sel, d);
        lv_obj_t *r = build("list_row", body, d);
        if (i == 0) speaker_row = r;
        if (i == 1) theme_row = r;
      }
    }
  }
  Sheet *s = sheet(host, p);
  s->trigger[0] = speaker_row;
  s->trigger[1] = theme_row;
  // the screen rows open their list (after the List row's own tap: event + sound open)
  for (int i = 0; i < 2; i++) {
    if (!s->trigger[i]) continue;
    lv_obj_add_event_cb(s->trigger[i], [](lv_event_t *e) {
      auto *s = (Sheet *) lv_event_get_user_data(e);
      open(s, lv_event_get_current_target(e) == s->trigger[1] ? 1 : 0);
    }, LV_EVENT_CLICKED, s);
  }
  if (p["loop"] | false) {  // frame-rate bench: open, hold, close, hold, ... (sheet-in / sheet-out apart + a pause)
    s->loop = lv_timer_create([](lv_timer_t *tm) {
      auto *s = (Sheet *) lv_timer_get_user_data(tm);
      if (fw::sheet::is_open(s->ov)) fw::sheet::close(s->ov); else open(s, s->which);
    }, fw::sheet::open_close_ms(), s);
    s->which = which;
  }
  if (strcmp(p["open"] | "open", "closed")) open(s, which, p["still"] | false);
  else s->which = which;
  return s;
}
}  // namespace

FW_COMPONENT(bottom_sheet, "Bottom sheet", "components", R"({
  "about": "A choose-one list over the screen: a dimming scrim, a title with Close and picker rows; a tap on the scrim, Close or a pick closes it.",
  "contract": 2,
  "parts": [],
  "stage": "screen",
  "frame": [480, 480],
  "icons": ["close@glyph", "speaker@icon", "speakers@icon", "tv@icon", "auto@icon", "bulb@icon", "volume@icon", "back@glyph"],
  "props": [
    {"name": "open", "about": "Whether the sheet is showing (open) or hidden off the bottom of the screen (closed). Changing it slides the sheet in or out.", "type": "select", "options": ["open", "closed"], "default": "open"},
    {"name": "list", "about": "For previews only: which made-up list fills the sheet, the speakers to play on or the light and dark theme choices.", "type": "select", "options": ["speakers", "theme"], "default": "speakers"},
    {"name": "title", "about": "The heading at the top of the sheet. Leave empty to use the list's own title.", "type": "text", "default": ""},
    {"name": "selected", "about": "Which row is chosen, counting from 0 at the top; its radio is on.", "type": "range", "min": 0, "max": 4, "step": 1, "default": 1},
    {"name": "screen", "about": "For previews only: draws a sample screen under the sheet so you can see it dim behind the scrim.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "open · speakers", "props": {}},
    {"label": "open · theme", "props": {"list": "theme", "selected": 0}},
    {"label": "closed · tap Speaker or Theme", "props": {"open": "closed"}},
    {"label": "no screen under it", "props": {"screen": false}},
    {"label": "reduced motion", "props": {"reduced_motion": true}}
  ]
})") {
  fill(parent, nullptr, p);
}

// Update path: the root = the screen box (the overlay itself when built as an overlay). open <-> closed = the sheet's
// own open / close (Motion sheet-open / sheet-close, silent); another list, title or items = its rows again (snap) and
// the panel's height follows; selected = the radios move (the Picker rows' update) and the screen rows show the pick
// (the List rows' update); screen / bench props = the screen again (fw::refill, snap) and the sheet opens. Silent.
FW_UPDATE(bottom_sheet) {
  lv_obj_t *ov = lv_obj_has_flag(root, LV_OBJ_FLAG_USER_4) ? root : lv_obj_get_child(root, -1);
  if (!ov || !lv_obj_has_flag(ov, LV_OBJ_FLAG_USER_4)) return false;
  if (ov != root && !fw::shape(root, sig(p))) {
    fw::refill(root);  // the old overlay goes with it (its DELETE frees the sheet)
    fill(nullptr, root, p);
    return true;
  }
  auto *s = (Sheet *) lv_obj_get_user_data(ov);
  const int which = !strcmp(p["list"] | "speakers", "theme") ? 1 : 0;
  const std::string was_title = s->custom_title, was_items = [&] {
    std::string a;
    for (auto &x : s->items) a += x + "\n";
    return a;
  }();
  const bool was_custom = s->custom;
  choices(s, p);
  std::string items;
  for (auto &x : s->items) items += x + "\n";
  // the screen rows show the pick
  for (int i = 0; i < 2; i++)
    if (s->trigger[i]) {
      Doc d;
      row_props(i, p["selected"] | 0, d);
      fw::update_child(fw::find("list_row"), s->trigger[i], d.as<fw::Props>());
    }
  const bool same_rows = s->host_rows < 0 && which == s->which && was_title == s->custom_title && items == was_items && was_custom == s->custom &&
                         (int) lv_obj_get_child_count(s->list) == n_opts(s);
  s->which = which;
  if (same_rows) {  // the radios move to the pick
    const int sel = s->custom ? s->item_sel : s->sel[which];
    for (int i = 0; i < n_opts(s); i++) {
      Doc d;
      option_props(s, i, sel, d);
      fw::update_child(fw::find("picker_row"), lv_obj_get_child(s->list, i), d.as<fw::Props>());
    }
  } else fill_list(s);
  const bool want = strcmp(p["open"] | "open", "closed") != 0;
  if (want && !fw::sheet::is_open(ov)) open(s, which, p["still"] | false);
  else if (!want) fw::sheet::close(ov);
  return true;
}
