// Covers view: the Blinds and curtains screen's body (docs screens/covers.ts coversScreen; the factory spec
// renderer/specs/covers.json adds the header): the window on the left is the hero, the real Cover visual drawn for
// the kind of cover and dragged to a position; on the right the state, the position, a slats slider for Venetian
// blinds and Open / Stop / Close.
//   left     the Cover visual (fw::find; lay::card_w x lay::content_h, its own size)
//   right    the rest of the width, sp::s9 after the window: the status (type label, text-dim: "42% open",
//            "opening", "offline"), the value (hero, text: "42%", "Open", "Closed"), the kind (body, text-dim:
//            "roller blind"), for a Venetian blind "slats" + its tilt (label) over a real Slider the column wide, and
//            at the foot three real Buttons (secondary, the column wide) Open, Stop, Close, sp::s3 apart
// Feed (docs CoverData): props kind, position (0 closed .. 100 open), tilt, state (open / closed / opening / closing /
// unavailable), heading (where it is going while it moves): an HA cover.* entity, or the sample. Taps: a drag on the
// window ("change" on release), the slats, a button: each emits "cover" (position 40 | tilt 60 | open | stop | close)
// for the host and shows it at once (held 3 s against updates, the docs rule); open / close set the heading line.
//   windows  on a spec screen with more than one window (the docs story: Study blind, Lounge curtains, Kitchen
//            blind, on the component page; a spec screen bound to no cover shows no story but its empty state:
//            "no window chosen", controls blocked, BF-048): the screen's Header label = the window's name, and before
//            the time "3 windows" (label type, text-dim, on a surface pill lay::row tall, rad::button, padding
//            lay::cal_btn_pad; pressed surface-press; tap = sound open) opens the windows sheet (Bottom sheet, picker
//            rows: the kind's icon, the name, its state); a pick shows that window (event "window" <name>). The sheet
//            covers the whole screen (the view bleeds up to it). Docs header right [pick].
// Real covers (2026-10-23): prop covers = a comma list of cover ids (the host's, spec $covers) lists those in the
// windows sheet (name, state / position from the HA store), prop cover = the bound one (its friendly name in the
// Header); a pick emits "window" <entity id> and the spec's screen.param re-binds the screen; prop device_class (HA's)
// sets the kind: curtain = curtains, a tilt position = Venetian, else roller. The spec's cover.control turns "cover"
// into cover.set_cover_position / set_cover_tilt_position / open_cover / stop_cover / close_cover.
// Not ported yet: the live "set to" text while dragging (the visual emits on release).
// Update path: the visual and the slider through their own updates (the cover glides there), texts in place.
// Contrast: "cover_visual", "button", "slider", "text".
// audit deps: button, cover_visual, slider, bottom_sheet
#include "fw.h"
#include "spec.h"

namespace {
struct CV {
  lv_obj_t *vis = nullptr, *cap = nullptr, *val = nullptr, *kind_l = nullptr, *tilt_box = nullptr, *tilt_v = nullptr, *slider = nullptr;
  std::string kind = "roller", state = "open";
  lv_obj_t *hlabel = nullptr, *sheet = nullptr;
  lv_obj_t *btn[3] = {nullptr, nullptr, nullptr};  // Open / Stop / Close (disabled while offline: blocked + error)
  int btn_w = 0;                                   // their width (dp)
  bool none = false;  // a screen with no window chosen yet (spec entity ""): blocked like offline, says so (BF-048)
  int win = -1;  // the window shown (the spec screen's windows list), -1 = the props
  std::string cover;             // the bound cover (prop cover)
  std::vector<std::string> ids;  // the host's covers (prop covers), empty = the docs sample windows
  int position = 40, tilt = 50, heading = -1;
  int held = -1;
  uint32_t hold_until = 0;
};
CV *of(lv_obj_t *o) { return (CV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
int col_w() { return fw::lay::content_w - fw::lay::card_w - fw::sp::s9; }
// the slats slider's track (dp): the column less a knob (the Slider's box = its track + half a knob each side)
int track_dp() { return (int) std::lround((col_w() - fw::touch::min / 2) / fw::D); }
int pos_of(const CV *c) { return c->held >= 0 && (int32_t) (c->hold_until - lv_tick_get()) > 0 ? c->held : c->position; }
void hold(CV *c, int p) { c->held = p; c->hold_until = lv_tick_get() + 3000; }
const char *kind_text(const std::string &k) { return k == "curtain" ? "curtains" : k == "venetian" ? "Venetian blind" : "roller blind"; }
void render(lv_obj_t *root) {
  CV *c = of(root);
  const bool off = c->state == "unavailable", moving = c->state == "opening" || c->state == "closing";
  const int p = pos_of(c);
  char b[24];
  if (off) snprintf(b, sizeof b, c->none ? "no window chosen" : "offline");
  else if (moving) snprintf(b, sizeof b, "%s", c->state.c_str());
  else if (p <= 0) snprintf(b, sizeof b, "closed");
  else if (p >= 100) snprintf(b, sizeof b, "open");
  else snprintf(b, sizeof b, "%d%% open", p);
  fw::set_text(c->cap, fw::ty::label, b);
  if (off) snprintf(b, sizeof b, "--");
  else if (p <= 0) snprintf(b, sizeof b, "Closed");
  else if (p >= 100) snprintf(b, sizeof b, "Open");
  else snprintf(b, sizeof b, "%d%%", p);
  fw::set_text(c->val, fw::ty::hero, b);
  fw::set_text(c->kind_l, fw::ty::body, kind_text(c->kind));
  snprintf(b, sizeof b, "%d%%", c->tilt);
  fw::set_text(c->tilt_v, fw::ty::label, b);
  const bool venetian = c->kind == "venetian";
  if (venetian) lv_obj_remove_flag(c->tilt_box, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(c->tilt_box, LV_OBJ_FLAG_HIDDEN);
  ArduinoJson::JsonDocument vp;
  vp["kind"] = c->kind, vp["position"] = p, vp["tilt"] = c->tilt, vp["heading"] = moving ? c->heading : -1;
  vp["state"] = off ? "unavailable" : "default";
  fw::update_child(fw::find("cover_visual"), c->vis, vp.as<fw::Props>());
  ArduinoJson::JsonDocument sp;
  sp["value"] = c->tilt, sp["width"] = track_dp(), sp["state"] = off ? "unavailable" : "default";
  fw::update_child(fw::find("slider"), c->slider, sp.as<fw::Props>());
  static const char *const LABEL[3] = {"Open", "Stop", "Close"};
  for (int i = 0; i < 3; i++) {
    if (!c->btn[i] || off == lv_obj_has_state(c->btn[i], fw::st::OFF)) continue;
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = LABEL[i], bp["width"] = c->btn_w, bp["state"] = off ? "disabled" : "default";
    fw::update_child(fw::find("button"), c->btn[i], bp.as<fw::Props>());
  }
}
std::string friendly(const std::string &e) {
  const std::string *n = fw::ha::get(e, "friendly_name");
  return n && !n->empty() ? *n : e;
}
void take(lv_obj_t *root, fw::Props p) {
  CV *c = of(root);
  c->cover = p["cover"] | "";
  c->ids.clear();
  const std::string l = p["covers"] | "";
  for (size_t i = 0; i < l.size();) {
    size_t j = l.find(',', i);
    if (j == std::string::npos) j = l.size();
    std::string id = l.substr(i, j - i);
    while (!id.empty() && id[0] == ' ') id.erase(0, 1);
    if (id.rfind("cover.", 0) == 0 && c->ids.size() < 12) { c->ids.push_back(id); for (const char *a : {"", "friendly_name", "current_position"}) fw::ha::want(id, a); }
    i = j + 1;
  }
  const std::string dc = p["device_class"] | "";  // HA's device class: curtain = curtains, a tilt = Venetian, else roller
  c->kind = !dc.empty() || !c->cover.empty() ? (dc == "curtain" ? "curtain" : p["tilt"].is<double>() ? "venetian" : "roller") : (p["kind"] | "roller");
  if (c->hlabel && !c->ids.empty() && !c->cover.empty()) fw::set_text(c->hlabel, fw::ty::label, friendly(c->cover).c_str());
  c->position = p["position"] | 40;
  c->tilt = p["tilt"] | 50;
  c->state = p["state"] | "open";
  if (c->state == "unknown") c->state = "unavailable";
  if (c->state.empty()) c->state = "open";  // nothing bound: the sample
  // bound to no cover: the prop is there (the sample has none) but holds no cover id ("" or an unset "$entity")
  c->none = p["cover"].is<const char *>() && c->cover.rfind("cover.", 0) != 0;
  if (c->none) c->state = "unavailable";
  c->heading = p["heading"] | -1;
  if (c->held == c->position) c->held = -1;  // HA caught up
  render(root);
}
// the windows (docs demo-data WINDOW_COVERS): name, kind, position, tilt
struct Win { const char *name, *kind; int position, tilt; };
constexpr Win WINDOWS[] = {{"Study blind", "roller", 60, 50}, {"Lounge curtains", "curtain", 100, 50}, {"Kitchen blind", "venetian", 100, 60}};
constexpr int N_WIN = sizeof WINDOWS / sizeof WINDOWS[0];
void show_window(lv_obj_t *root, int i) {
  CV *c = of(root);
  c->win = i;
  ArduinoJson::JsonDocument d;
  d["kind"] = WINDOWS[i].kind, d["position"] = WINDOWS[i].position, d["tilt"] = WINDOWS[i].tilt, d["state"] = WINDOWS[i].position > 0 ? "open" : "closed";
  c->held = -1;
  take(root, d.as<fw::Props>());
  if (c->hlabel) fw::set_text(c->hlabel, fw::ty::label, WINDOWS[i].name);
}
int n_win(const CV *c) { return c->none && c->ids.empty() ? 0 : c->ids.empty() ? N_WIN : (int) c->ids.size(); }  // none: no sample story
void win_sheet_props(const CV *c, bool open, ArduinoJson::JsonDocument &d) {
  d["overlay"] = true, d["title"] = "windows", d["open"] = open ? "open" : "closed", d["selected"] = c->win < 0 ? 0 : c->win;
  ArduinoJson::JsonArray items = d["items"].to<ArduinoJson::JsonArray>();
  if (!c->ids.empty()) {  // the host's covers: name, state / position from the store
    for (size_t i = 0; i < c->ids.size(); i++) {
      const std::string &e = c->ids[i];
      if (e == c->cover) d["selected"] = (int) i;
      ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
      const std::string *st = fw::ha::get(e), *pos = fw::ha::get(e, "current_position");
      std::string status = st ? *st : "unknown";
      if (pos && status == "open" && atoi(pos->c_str()) < 100) status = *pos + "% open";
      it["icon"] = "roller", it["name"] = friendly(e), it["status"] = status;
    }
    return;
  }
  for (const Win &w : WINDOWS) {
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    it["icon"] = w.kind, it["name"] = w.name;
    it["status"] = w.position <= 0 ? "closed" : w.position >= 100 ? "open" : std::to_string(w.position) + "% open";
  }
}
const lv_style_prop_t PICK_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *pick_tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, PICK_PROPS); ready = true; }
  return &d[pressed];
}
// the windows picker in the screen's header + its sheet (spec screens only)
void windows_header(lv_obj_t *root) {
  lv_obj_t *hdr = fw::spec::building_header();
  if (!hdr || n_win(of(root)) < 2) return;
  CV *c = of(root);
  const fw::Theme &t = *fw::T;
  c->hlabel = lv_obj_get_child_count(hdr) > 1 ? lv_obj_get_child(hdr, 1) : nullptr;  // back, label, ..., chrome
  if (c->hlabel && !lv_obj_check_type(c->hlabel, &lv_label_class)) c->hlabel = nullptr;
  lv_obj_t *b = lv_obj_create(hdr);
  lv_obj_remove_style_all(b);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, fw::rad::button, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, t.surface, 0);
  lv_obj_set_style_bg_color(b, fw::st::pressed_surface(t), LV_STATE_PRESSED);
  lv_obj_set_style_transition(b, pick_tr(false), 0);
  lv_obj_set_style_transition(b, pick_tr(true), LV_STATE_PRESSED);
  const std::string txt = std::to_string(n_win(c)) + " windows";
  lv_obj_t *l = fw::text(b, fw::ty::label, txt.c_str(), t.text_dim);
  fw::spec::header_fit(l);
  lv_obj_set_size(b, lv_obj_get_style_width(l, LV_PART_MAIN) + 2 * fw::lay::cal_btn_pad, fw::lay::row);
  lv_obj_center(l);
  fw::spec::header_place(b);
  ArduinoJson::JsonDocument sd;
  win_sheet_props(c, false, sd);
  c->sheet = build("bottom_sheet", root, sd);
  if (c->sheet) {
    lv_obj_set_pos(c->sheet, -fw::lay::inset, -fw::lay::top);
    lv_obj_set_size(c->sheet, fw::lay::screen_w, fw::lay::screen_h);
    fw::qa::free(root, "overlay: hosts the windows sheet, which covers the whole screen");
    fw::bleed(root, fw::lay::top);
    lv_obj_add_event_cb(c->sheet, [](lv_event_t *e) {  // a window picked
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      const int i = (int) (intptr_t) lv_event_get_param(e);
      CV *c = of(root);
      if (!c->ids.empty()) {  // a real cover: the spec's screen.param re-binds the screen to it
        if (i >= 0 && i < (int) c->ids.size()) { c->held = -1; fw::emit(root, "window", c->ids[i].c_str()); }
        return;
      }
      if (i < 0 || i >= N_WIN) return;
      show_window(root, i);
      fw::emit(root, "window", WINDOWS[i].name);
    }, LV_EVENT_VALUE_CHANGED, root);
  }
  lv_obj_add_event_cb(b, [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    CV *c = of(root);
    fw::emit((lv_obj_t *) lv_event_get_current_target(e), "open", "windows");
    fw::sound((lv_obj_t *) lv_event_get_current_target(e), "open");
    if (!c->sheet) return;
    ArduinoJson::JsonDocument sd;
    win_sheet_props(c, true, sd);
    fw::update_child(fw::find("bottom_sheet"), c->sheet, sd.as<fw::Props>());
  }, LV_EVENT_CLICKED, root);
  if (c->ids.empty()) show_window(root, 0);
  else if (c->hlabel && !c->cover.empty()) fw::set_text(c->hlabel, fw::ty::label, friendly(c->cover).c_str());
}
}  // namespace

FW_COMPONENT(covers_view, "Covers view", "components", R"({
  "about": "The Blinds and curtains screen's body: the window drawn for the kind of cover and dragged to a position; the state, the position, the slats and Open / Stop / Close beside it.",
  "frame": [480, 400],
  "audit": {"snaps": {"kind": "a roller, Venetian or curtain is a different drawing: structure, the Cover visual's rule"}},
  "props": [
    {"name": "kind", "type": "select", "options": ["roller", "venetian", "curtain"], "default": "roller"},
    {"name": "position", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "tilt", "type": "range", "min": 0, "max": 100, "step": 1, "default": 50},
    {"name": "device_class", "type": "text", "default": ""},
    {"name": "cover", "type": "text", "default": ""},
    {"name": "covers", "type": "text", "default": ""},
    {"name": "state", "type": "select", "options": ["open", "closed", "opening", "closing", "unavailable"], "default": "open"}
  ],
  "variants": [
    {"label": "roller · 40% open", "props": {}},
    {"label": "Venetian · slats", "props": {"kind": "venetian", "position": 70, "tilt": 30}},
    {"label": "curtains · closing", "props": {"kind": "curtain", "position": 60, "state": "closing", "heading": 0}},
    {"label": "closed", "props": {"position": 0, "state": "closed"}},
    {"label": "offline", "props": {"state": "unavailable"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *c = new CV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (CV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, c);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(root, fw::sp::s9, 0);
  lv_obj_t *left = box(root);
  lv_obj_set_size(left, fw::lay::card_w, fw::lay::content_h);
  ArduinoJson::JsonDocument vp;
  vp["kind"] = p["kind"] | "roller", vp["position"] = p["position"] | 40;
  c->vis = build("cover_visual", left, vp);
  fw::listen(left, [](lv_obj_t *, const char *name, const char *detail, void *user) {  // a drag released
    if (strcmp(name, "change")) return;
    auto *root = (lv_obj_t *) user;
    CV *c = of(root);
    hold(c, atoi(detail));
    fw::emit(root, "cover", (std::string("position ") + detail).c_str());
    render(root);
  }, root);
  const int W = col_w();
  lv_obj_t *col = box(root);
  lv_obj_set_size(col, W, fw::lay::content_h);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, fw::sp::s2, 0);
  c->cap = fw::text(col, fw::ty::label, "", t.text_dim, W);
  c->val = fw::text(col, fw::ty::hero, "--", t.text, W);
  c->kind_l = fw::text(col, fw::ty::body, "", t.text_dim, W);
  c->tilt_box = box(col);  // slats: caption + value, the slider under them
  lv_obj_set_size(c->tilt_box, W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c->tilt_box, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(c->tilt_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(c->tilt_box, fw::sp::s3, 0);
  lv_obj_set_style_margin_top(c->tilt_box, fw::sp::s9, 0);
  fw::text(c->tilt_box, fw::ty::label, "slats", t.text_dim);
  c->tilt_v = fw::text(c->tilt_box, fw::ty::label, "", t.text_dim);
  ArduinoJson::JsonDocument sp;
  sp["value"] = p["tilt"] | 50, sp["width"] = track_dp();
  c->slider = build("slider", c->tilt_box, sp);
  fw::listen(c->tilt_box, [](lv_obj_t *, const char *name, const char *detail, void *user) {
    if (strcmp(name, "change")) return;
    auto *root = (lv_obj_t *) user;
    CV *c = of(root);
    c->tilt = atoi(detail);
    fw::emit(root, "cover", (std::string("tilt ") + detail).c_str());
    render(root);
  }, root);
  lv_obj_t *grow = box(col);
  lv_obj_set_flex_grow(grow, 1);
  lv_obj_t *btns = box(col);
  lv_obj_set_size(btns, W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(btns, fw::sp::s3, 0);
  static const char *const CMD[3][2] = {{"Open", "open"}, {"Stop", "stop"}, {"Close", "close"}};
  for (int i = 0; i < 3; i++) {
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = CMD[i][0], bp["width"] = (int) std::lround(W / fw::D);
    lv_obj_t *b = build("button", btns, bp);
    if (!b) continue;
    c->btn[i] = b, c->btn_w = bp["width"];
    lv_obj_set_user_data(b, (void *) CMD[i][1]);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      CV *c = of(root);
      const char *cmd = (const char *) lv_obj_get_user_data((lv_obj_t *) lv_event_get_current_target(e));
      if (c->state == "unavailable") return;
      c->held = -1;
      if (!strcmp(cmd, "open")) c->heading = 100, c->state = "opening";
      else if (!strcmp(cmd, "close")) c->heading = 0, c->state = "closing";
      else c->heading = -1, c->state = pos_of(c) <= 0 ? "closed" : "open";
      fw::emit(root, "cover", cmd);
      render(root);
    }, LV_EVENT_CLICKED, root);
  }
  take(root, p);
  windows_header(root);
}

FW_UPDATE(covers_view) {
  CV *c = of(root);
  if (!c) return false;
  if (c->win >= 0 && !(p["covers"] | "")[0]) { render(root); return true; }  // a picked sample window keeps showing
  take(root, p);
  return true;
}
