// Bottom sheet: the Facet bottom sheet (docs: Components > Pickers > Bottom sheet, @facet/ui pickers.ts bottomSheet(),
// CSS .f-bsheet): a choose-one list over the screen (the Media speaker picker, Settings pickers, the Covers kinds).
//   overlay  fills the screen it is built on (floating, ignores the screen's layout); hidden while closed
//   scrim    the whole screen, colour scrim at Motion opacity scrim / scrim-light (dark / light theme); a tap on it closes the sheet (event "close" detail
//            "scrim" + sound back: docs data-snd back)
//   panel    as tall as its content (padding + head + rows), at most from lay::sheet_top (96 / 64) to the bottom
//            (Darren 2026-10-22: a short list opens lower, no empty space; the list scrolls only at the cap), anchored
//            to the bottom, the full screen width (docs .f-bsheet-panel: left 0 right 0, content inset), bg fill, 1 px line-hi edge on top, rad::ring top corners (the panel runs rad::ring past
//            the screen bottom, which clips it, so only the top corners round); padding inset - hair on top (the
//            edge makes it inset), inset left / right / bottom; flex column, gap lay::gutter:
//     head   lay::row tall: the title (screen type label, caps, text-dim, grows, ellipsis) + Close = the Icon button
//            component, square, icon close (fw::find), in a hit box that owns the tap and trickles PRESSED (as the Dial
//            hosts Button): event "close" detail "close" + sound back (docs data-snd back). Docs: head at y 128 / 84.
//     list   grows, scrolls vertically; Picker rows (fw::find("picker_row")), lay::sheet_row_h each; picker rows that
//            share a parent are one group, so a tap moves the radio by itself; the sheet adds "close after a pick"
//            (the pick's own sound is the List row's tap). Docs: list from y 192 / 136.
// Motion (motion.json): open = pattern sheet-open (160 ms ease-out: scrim fades in to its opacity, the panel fades in
// and rises by the offset 16 / 12), close = sheet-close (120 ms: scrim out, panel fades and drops); reduced motion =
// fade only (the patterns' mode). Both run from wherever they are, so a close during the opening turns it round.
// No fades (fw::mo::fades, the Guition by default): the scrim snaps on / off (off at the end of the close), the panel is
// opaque and slides up from / down by its own height, translate only; with reduced motion as well: instant.
// Performance (Guition: the first sheet ran at 12 fps, README "Motion tokens"): every frame of the open / close
// redraws the whole screen, because the scrim fades over all of it and the panel (most of the screen) fades and
// moves. Kept small where it can be: the scrim is ONE full-screen rect whose bg_opa animates (no layer, no per-child
// opacity), the panel moves as ONE object (translate_y + opa on the panel only, its rows never re-layout), nothing
// else is invalidated. Browser bench 2026-10-22 (workbench device page, 8x CPU throttle, open + close looping; README
// "Bottom sheet frame cost"): the cost is the blending area, not the objects: flattening the screen under the sheet
// into one opaque lv_snapshot image (LVGL's cover check then skips everything under it) measured no gain, a panel
// snapshot image moving instead of the panel was worse (ARGB blend), an opaque panel that only rises was ~15-18 %
// cheaper. Kept as internal bench props for the panel flash session: `flatten` (screen image) and `panel: solid`.
// Workbench page: the sheet over a real-component screen at device size (the Header + List rows: "Settings"); the
// Speaker and Theme rows open their sheets (their chevron tap plays open), a pick updates the row's value. Props:
// open (open = it opens on build, so the 1.8 s demo reset replays it; closed = tap a row), list, title, selected,
// screen. Internal: `items` (array of {icon, name, status}: the caller's own list instead of the built-in ones; a pick
// sends LV_EVENT_VALUE_CHANGED to the overlay, param = the index; with `selected` = the index ringed, `stay` = a pick
// keeps it open; the Settings screen uses this), `rows` + `row_h` (host rows: the caller's own rows, `rows` of `row_h`
// px for the panel's height; every fill sends LV_EVENT_REFRESH to the overlay with the list as the param, the caller
// builds into it; the Calendar's calendars and day sheets), `loop` (opens and closes forever: the frame-rate bench), `overlay` (only the overlay, built into
// the caller's screen; the Layout foundation's overlay sample) with `still` (shown open, no animation).
// Contrast pairs: tokens/colour.json section "bottom_sheet" (+ picker_row, radio, list_row, icon_button, header).
#include "fw.h"

namespace {
using Doc = ArduinoJson::JsonDocument;

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
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
  lv_obj_t *host = nullptr, *ov = nullptr, *scrim = nullptr, *panel = nullptr, *title = nullptr, *list = nullptr;
  lv_obj_t *snap = nullptr;  // flatten: the screen under the sheet as one image while it animates
  lv_draw_buf_t *buf = nullptr;
  lv_timer_t *loop = nullptr;
  bool open = false, flatten = false, solid = false;
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
int row_h(const Sheet *s) { return s->host_rows >= 0 && s->host_row_h > 0 ? s->host_row_h : fw::lay::sheet_row_h; }
Option opt(const Sheet *s, int i) {
  if (!s->custom) return LISTS[s->which].opts[i];
  return {s->items[3 * i].c_str(), s->items[3 * i + 1].c_str(), s->items[3 * i + 2].c_str(), "dim"};
}

void set_bg_opa(void *o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *) o, v, 0); }
// scrim: colour.json scrim (near-black in both themes) at motion.json opacity scrim (dark) / scrim-light (Darren 2026-10-22:
// the backdrop must read clearly darker, not just faded towards the bg)
bool g_scrim_light = false;
lv_opa_t scrim_opa() { return (lv_opa_t) lroundf((g_scrim_light ? fw::mo::OPA_SCRIM_LIGHT : fw::mo::OPA_SCRIM) * 255); }

// ---- flatten (bench option, see the top): the screen under the sheet as one opaque image while the sheet animates
void unflatten(Sheet *s) {
  if (s->snap) { lv_obj_delete(s->snap); s->snap = nullptr; }
  if (s->buf) { lv_draw_buf_destroy(s->buf); s->buf = nullptr; }
}
void flatten(Sheet *s) {
  if (!s->flatten || s->snap) return;
  const bool was_hidden = lv_obj_has_flag(s->ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s->ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(s->host);
  s->buf = lv_snapshot_take(s->host, LV_COLOR_FORMAT_RGB565);  // the panels' format; no alpha = it covers
  if (!was_hidden) lv_obj_remove_flag(s->ov, LV_OBJ_FLAG_HIDDEN);
  if (!s->buf) return;
  s->snap = lv_image_create(s->ov);
  lv_obj_remove_flag(s->snap, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(s->snap, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_move_to_index(s->snap, 0);  // under the scrim and the panel
  lv_image_set_src(s->snap, s->buf);
  const int32_t ext = ((int32_t) s->buf->header.w - lv_obj_get_width(s->host)) / 2;  // snapshot = box + ext draw
  lv_obj_set_pos(s->snap, -ext, -ext);
}

void fill_list(Sheet *s);
void close(Sheet *s);

void opened(lv_anim_t *a) { unflatten((Sheet *) lv_anim_get_user_data(a)); }
void closed(lv_anim_t *a) {
  auto *s = (Sheet *) lv_anim_get_user_data(a);
  unflatten(s);
  if (!s->open) {
    set_bg_opa(s->scrim, LV_OPA_TRANSP);  // no fades: the scrim stayed while the panel dropped, then snaps off
    lv_obj_add_flag(s->ov, LV_OBJ_FLAG_HIDDEN);
  }
}

void open(Sheet *s, int which, bool still = false) {
  s->which = which;
  fill_list(s);
  if (s->open) return;
  s->open = true;
  lv_obj_remove_flag(s->ov, LV_OBJ_FLAG_HIDDEN);
  if (still) {
    set_bg_opa(s->scrim, scrim_opa());
    lv_obj_set_style_opa(s->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_y(s->panel, 0, 0);
    return;
  }
  if (!fw::mo::fades(s->panel)) {  // no fades (core/fw.h, Guition): the scrim snaps on, the opaque panel slides up
    lv_anim_delete(s->scrim, set_bg_opa);
    lv_anim_delete(s->panel, nullptr);
    set_bg_opa(s->scrim, scrim_opa());
    lv_obj_set_style_opa(s->panel, LV_OPA_COVER, 0);
    lv_obj_update_layout(s->panel);
    const int32_t ty = lv_obj_get_style_translate_y(s->panel, LV_PART_MAIN);
    fw::Anim m{set_ty, ty ? ty : lv_obj_get_height(s->panel), 0, fw::mo::MOVE};
    m.done = opened; m.user = s;
    fw::animate(s->panel, fw::mo::sheet_open, m);
    return;
  }
  if (lv_obj_get_style_opa(s->panel, LV_PART_MAIN) == LV_OPA_TRANSP) set_bg_opa(s->scrim, LV_OPA_TRANSP);
  flatten(s);
  fw::animate(s->scrim, fw::mo::sheet_open, {set_bg_opa, lv_obj_get_style_bg_opa(s->scrim, LV_PART_MAIN), scrim_opa(), fw::mo::OPA});
  if (s->solid) {  // bench option: the panel opaque, rising only (it covers, so LVGL skips what is under it)
    lv_obj_set_style_opa(s->panel, LV_OPA_COVER, 0);
    fw::Anim m{set_ty, fw::mo::pat(fw::mo::sheet_open).offset, 0, fw::mo::MOVE};
    m.done = opened; m.user = s;
    fw::animate(s->panel, fw::mo::sheet_open, m);
  } else fw::appear(s->panel, fw::mo::sheet_open, true, 1, 0, opened, s);
}

void close(Sheet *s) {
  if (!s->open) return;
  s->open = false;
  if (!fw::mo::fades(s->panel)) {  // no fades: the panel slides down (translate only), the scrim snaps off at the end
    lv_anim_delete(s->panel, nullptr);
    lv_obj_set_style_opa(s->panel, LV_OPA_COVER, 0);
    fw::Anim m{set_ty, lv_obj_get_style_translate_y(s->panel, LV_PART_MAIN), lv_obj_get_height(s->panel), fw::mo::MOVE};
    m.done = closed; m.user = s;
    fw::animate(s->panel, fw::mo::sheet_close, m);
    return;
  }
  flatten(s);
  fw::animate(s->scrim, fw::mo::sheet_close, {set_bg_opa, lv_obj_get_style_bg_opa(s->scrim, LV_PART_MAIN), LV_OPA_TRANSP, fw::mo::OPA});
  if (s->solid) {
    fw::Anim m{set_ty, lv_obj_get_style_translate_y(s->panel, LV_PART_MAIN), lv_obj_get_height(s->panel), fw::mo::MOVE};
    m.done = closed; m.user = s;
    fw::animate(s->panel, fw::mo::sheet_close, m);
  } else fw::appear(s->panel, fw::mo::sheet_close, false, 1, 0, closed, s);
}

// the label in `o`'s subtree showing `txt` (a List row's value), or nullptr
lv_obj_t *label_with(lv_obj_t *o, const char *txt) {
  if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), txt)) return o;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
    if (lv_obj_t *l = label_with(lv_obj_get_child(o, i), txt)) return l;
  return nullptr;
}

// the panel's height = its content (padding + head + gutter + the rows + the part past the bottom), capped at the
// tallest sheet (from lay::sheet_top to the bottom; the list scrolls only then), its top on the grid: every term is a
// layout token on the 4 px grid. Bottom-anchored, so a short list opens lower (Darren 2026-10-22: no empty space).
// The overlay's own box, not the host's: a caller may stretch the overlay over the whole screen from inside a smaller
// box (the Thermostat and Calendar views); the panel follows it (width too).
void size_panel(Sheet *s) {
  lv_obj_update_layout(s->host);
  const int H = lv_obj_get_height(s->ov), max_h = H - fw::lay::sheet_top + fw::rad::ring;
  lv_obj_set_width(s->panel, lv_obj_get_width(s->ov) + 2 * fw::stroke::hair);
  const int need = 2 * fw::lay::inset + fw::lay::row + fw::lay::gutter + n_opts(s) * row_h(s) + fw::rad::ring;
  const int h = std::min(max_h, need);
  lv_obj_set_height(s->panel, h);
  lv_obj_set_y(s->panel, H + fw::rad::ring - h);
}

void fill_list(Sheet *s) {
  fw::set_text(s->title, fw::ty::label, s->custom_title.empty() ? LISTS[s->which].title : s->custom_title.c_str());
  size_panel(s);
  lv_obj_clean(s->list);
  lv_obj_scroll_to_y(s->list, 0, LV_ANIM_OFF);
  if (s->host_rows >= 0) {  // host rows: the caller builds them (LV_EVENT_REFRESH on the overlay, param = the list)
    lv_obj_send_event(s->ov, LV_EVENT_REFRESH, s->list);
    return;
  }
  for (int i = 0; i < n_opts(s); i++) {
    const Option o = opt(s, i);
    Doc d;
    d["icon"] = o.icon;
    d["name"] = o.name;
    d["status"] = o.status;
    d["tone"] = o.tone;
    d["selected"] = i == (s->custom ? s->item_sel : s->sel[s->which]);
    lv_obj_t *r = build("picker_row", s->list, d);
    if (!r) continue;
    // after the Picker row's own handlers (List row tap + sound, the radio group): remember the pick, show it on the
    // screen row that opened the list, close
    lv_obj_add_event_cb(r, [](lv_event_t *e) {
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      auto *s = (Sheet *) lv_event_get_user_data(e);
      if (lv_obj_has_state(o, fw::st::OFF) || !s->open) return;
      if (s->custom) {
        const int i = (int) lv_obj_get_index(o);
        s->item_sel = i;
        lv_obj_send_event(s->ov, LV_EVENT_VALUE_CHANGED, (void *) (intptr_t) i);
        if (!s->stay) close(s);
        return;
      }
      const List &L = LISTS[s->which];
      const int i = (int) lv_obj_get_index(o), was = s->sel[s->which];
      if (i < 0 || i >= L.n) return;
      if (lv_obj_t *row = s->trigger[s->which])
        if (lv_obj_t *v = label_with(row, L.opts[was].name)) fw::set_text(v, fw::ty::body, L.opts[i].name);
      s->sel[s->which] = i;
      close(s);
    }, LV_EVENT_CLICKED, s);
  }
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

// the overlay (scrim + panel) on `host`, closed
Sheet *sheet(lv_obj_t *host, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *s = new Sheet;
  s->host = host;
  s->flatten = p["flatten"] | false;
  s->solid = !strcmp(p["panel"] | "fade", "solid");
  choices(s, p);

  s->ov = box(host);
  lv_obj_add_flag(s->ov, (lv_obj_flag_t) (LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_USER_4));
  lv_obj_set_user_data(s->ov, s);
  lv_obj_set_size(s->ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(s->ov, 0, 0);
  fw::qa::free(s->ov, "overlay: bottom sheet");  // sits over the screen on purpose (tools/audit_screens.mjs skips it)

  s->scrim = box(s->ov);
  lv_obj_add_flag(s->scrim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(s->scrim, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s->scrim, t.scrim, 0);
  g_scrim_light = t.light;
  lv_obj_set_style_bg_opa(s->scrim, LV_OPA_TRANSP, 0);
  lv_obj_add_event_cb(s->scrim, [](lv_event_t *e) {
    auto *s = (Sheet *) lv_event_get_user_data(e);
    if (!s->open) return;
    fw::emit(s->scrim, "close", "scrim");
    fw::sound(s->scrim, "back");
    close(s);
  }, LV_EVENT_CLICKED, s);

  lv_obj_update_layout(host);
  const int W = lv_obj_get_content_width(host), H = lv_obj_get_content_height(host);
  s->panel = box(s->ov);
  lv_obj_add_flag(s->panel, LV_OBJ_FLAG_CLICKABLE);  // taps on the panel never reach the scrim
  // one hair past the left / right edges and rad::ring past the bottom: the full 1 px edge with its rounded top corners
  // shows, its sides and bottom fall off the screen (Darren 2026-10-22: a top-only border drew square corners)
  lv_obj_set_pos(s->panel, -fw::stroke::hair, fw::lay::sheet_top);
  lv_obj_set_size(s->panel, W + 2 * fw::stroke::hair, H - fw::lay::sheet_top + fw::rad::ring);
  // (the height follows the list: size_panel() on every fill_list)
  lv_obj_set_style_bg_color(s->panel, t.bg, 0);
  lv_obj_set_style_bg_opa(s->panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s->panel, fw::rad::ring, 0);
  lv_obj_set_style_border_side(s->panel, LV_BORDER_SIDE_FULL, 0);
  lv_obj_set_style_border_width(s->panel, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(s->panel, t.line_hi, 0);
  lv_obj_set_style_pad_top(s->panel, fw::lay::inset - fw::stroke::hair, 0);
  lv_obj_set_style_pad_hor(s->panel, fw::lay::inset, 0);
  lv_obj_set_style_pad_bottom(s->panel, fw::lay::inset + fw::rad::ring - fw::stroke::hair, 0);  // + the (off-screen) bottom edge
  lv_obj_set_flex_flow(s->panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s->panel, fw::lay::gutter, 0);
  lv_obj_set_style_opa(s->panel, LV_OPA_TRANSP, 0);

  lv_obj_t *head = box(s->panel);
  lv_obj_set_size(head, LV_PCT(100), fw::lay::row);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(head, fw::lay::gutter, 0);
  s->title = fw::text(head, fw::ty::label, "", t.text_dim);
  lv_obj_remove_flag(s->title, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(s->title, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(s->title, 0);
  lv_obj_set_flex_grow(s->title, 1);
  lv_obj_set_height(s->title, fw::style(fw::ty::label).box);
  lv_obj_t *hit = box(head);  // owns the tap (sound back), trickles PRESSED to the Icon button for its press look
  lv_obj_add_flag(hit, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_set_size(hit, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  Doc cp;
  cp["style"] = "square";
  cp["icon"] = "close";
  if (lv_obj_t *cb = build("icon_button", hit, cp)) lv_obj_remove_flag(cb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(hit, [](lv_event_t *e) {
    auto *s = (Sheet *) lv_event_get_user_data(e);
    if (!s->open) return;
    fw::emit((lv_obj_t *) lv_event_get_current_target(e), "close", "close");
    fw::sound((lv_obj_t *) lv_event_get_current_target(e), "back");
    close(s);
  }, LV_EVENT_CLICKED, s);

  s->list = box(s->panel);
  lv_obj_add_flag(s->list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(s->list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s->list, LV_SCROLLBAR_MODE_OFF);  // the Facet scroll track is its own part (later)
  lv_obj_set_width(s->list, LV_PCT(100));
  lv_obj_set_flex_grow(s->list, 1);
  lv_obj_set_flex_flow(s->list, LV_FLEX_FLOW_COLUMN);

  lv_obj_add_event_cb(s->ov, [](lv_event_t *e) {
    auto *s = (Sheet *) lv_event_get_user_data(e);
    if (s->loop) lv_timer_delete(s->loop);
    lv_anim_delete(s->scrim, nullptr);
    lv_anim_delete(s->panel, nullptr);
    unflatten(s);  // the image first, then its buffer (the overlay's children go after this event)
    delete s;
  }, LV_EVENT_DELETE, s);
  return s;
}
}  // namespace

namespace {
// what needs the screen again (fw::refill); the rest (open, list, title, selected, items) moves in place
std::string sig(fw::Props p) {
  return std::string((p["screen"] | true) ? "s" : "") + ((p["loop"] | false) ? "l" : "") + ((p["flatten"] | false) ? "f" : "") +
         (p["panel"] | "fade") + ((p["still"] | false) ? "t" : "");
}
// the screen (root = its box; nullptr = made on parent) + the sheet over it; returns the sheet
Sheet *fill(lv_obj_t *parent, lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool overlay = p["overlay"] | false;
  // the screen: the caller's (overlay), else a device-size screen of real components (Header + List rows)
  lv_obj_t *host = parent;
  lv_obj_t *speaker_row = nullptr, *theme_row = nullptr;
  const int which = !strcmp(p["list"] | "speakers", "theme") ? 1 : 0;
  if (!overlay) {
    host = root ? root : box(parent);
    lv_obj_remove_style_all(host);
    lv_obj_remove_flag(host, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    fw::shape(host, sig(p));
    lv_obj_set_size(host, fw::lay::screen_w, fw::lay::screen_h);
    lv_obj_set_style_bg_color(host, t.bg, 0);
    lv_obj_set_style_bg_opa(host, LV_OPA_COVER, 0);
    if (p["screen"] | true) {
      Doc hd;
      hd["label"] = "Settings";
      if (lv_obj_t *h = build("header", host, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);
      lv_obj_t *c = box(host);
      lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
      lv_obj_set_size(c, fw::lay::content_w, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
      const int sel = p["selected"] | 0;
      for (int i = 0; i < 4; i++) {
        Doc d;
        row_props(i, sel, d);
        lv_obj_t *r = build("list_row", c, d);
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
    lv_obj_set_user_data(s->trigger[i], (void *) (intptr_t) i);
    lv_obj_add_event_cb(s->trigger[i], [](lv_event_t *e) {
      auto *s = (Sheet *) lv_event_get_user_data(e);
      open(s, (int) (intptr_t) lv_obj_get_user_data((lv_obj_t *) lv_event_get_current_target(e)));
    }, LV_EVENT_CLICKED, s);
  }
  if (p["loop"] | false) {  // frame-rate bench: open, hold, close, hold, ... (sheet-in / sheet-out apart + a pause)
    s->loop = lv_timer_create([](lv_timer_t *tm) {
      auto *s = (Sheet *) lv_timer_get_user_data(tm);
      if (s->open) close(s); else open(s, s->which);
    }, fw::mo::ms::sheet_in + fw::mo::ms::sheet_out, s);
    s->which = which;
  }
  if (strcmp(p["open"] | "open", "closed")) open(s, which, p["still"] | false);
  else s->which = which;
  return s;
}
}  // namespace

FW_COMPONENT(bottom_sheet, "Bottom sheet", "components", R"({
  "about": "A choose-one list over the screen: a dimming scrim, a title with Close and picker rows; a tap on the scrim, Close or a pick closes it.",
  "frame": [480, 480],
  "icons": ["close@glyph", "speaker@icon", "speakers@icon", "tv@icon", "auto@icon", "bulb@icon", "volume@icon", "back@glyph"],
  "props": [
    {"name": "open", "type": "select", "options": ["open", "closed"], "default": "open"},
    {"name": "list", "type": "select", "options": ["speakers", "theme"], "default": "speakers"},
    {"name": "title", "type": "text", "default": ""},
    {"name": "selected", "type": "range", "min": 0, "max": 4, "step": 1, "default": 1},
    {"name": "screen", "type": "bool", "default": true}
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
      const Option o = opt(s, i);
      Doc d;
      d["icon"] = o.icon;
      d["name"] = o.name;
      d["status"] = o.status;
      d["tone"] = o.tone;
      d["selected"] = i == sel;
      fw::update_child(fw::find("picker_row"), lv_obj_get_child(s->list, i), d.as<fw::Props>());
    }
  } else fill_list(s);
  const bool want = strcmp(p["open"] | "open", "closed") != 0;
  if (want && !s->open) open(s, which, p["still"] | false);  // (open() lists the rows again: only when it opens)
  else if (!want) close(s);
  return true;
}
