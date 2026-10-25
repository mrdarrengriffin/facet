// Segmented: the Facet segmented control (docs: @facet/ui settings.ts segmented(), CSS .f-seg / .f-seg-b). One
// segment per option in a hairline box; exactly one is chosen. Docs uses: a Settings row with up to 3 options inline
// (Theme auto / dark / light), and the view tabs of Weather (Weather / Sun / Moon / Lightning), Calendar
// (Agenda / Month) and Overhead (Now / Radar).
//   box      height touch::min (48 / 32; docs 48), stroke::hair border in line, radius rad::button (12 / 8); the
//            line is a ring child drawn over the segments, so every segment box starts on the box edge (on the grid)
//   segment  label (type label: the docs .fst-label, caps) + sp::s7 each side (24 / 16; docs padding 0 24), width
//            snapped up to the 4 px grid so every separator lands on the grid; segments after the first have a
//            stroke::hair separator in line on their left (docs border-left)
//   width    hug (docs: the box hugs its segments) or fill = lay::content_w (a full-width tab bar), segments equal
//   chosen   st::ON (LVGL CHECKED): plate surface-press + 1 px line-hi outline just inside the box line and over its
//            separator (docs: outline-offset -hair), label text (rest labels text-dim). Plates are the segment's grid
//            box; on the box's outer sides their ring is 2 hairlines and the box line covers the outer one. End
//            plates round the outer corners (radius button) and stay square inside (docs border-radius on
//            :first-child / :last-child): LVGL has one radius per box, so an end plate is a rounded box under a
//            square box with no border on the rounded side
//   pressed  plate surface-press (st::pressed_surface; docs :active), label unchanged, Motion press / release fades
//   disabled st::OFF on every segment (Button rule): labels text-mute, chosen plate keeps surface-press with a line
//            outline (st::disabled_line) and its label goes text-dim (st::disabled_on_fill: text-mute is 2.7:1 on
//            surface-press), no press look; finger down = blocked + error
//   unavailable  (a segmented bound to a Home Assistant select, states.json): the word Unavailable (body, text-dim)
//            in place of the control, touch::min tall, finger down = blocked + error (= Toggle)
// Tap: a real CLICKED on a segment chooses it: event "change" (detail = the option id), or "tap" (same detail) on the
// one already chosen; sound tap either way (= Radio / Picker row: choosing one of a set is a plain action in place). The control owns the
// choice (no owner needed); the workbench demo reset puts it back to `selected`.
// Props: options = comma list of labels (id = the label lower-cased), selected = id or label (default the first),
// width hug | fill, state. Contrast pairs: tokens/colour.json section "segmented".
#include "fw.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool press) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[press];
}

struct Seg {
  std::vector<std::string> ids;
  int chosen = 0;
};

std::string trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string lower(std::string s) {
  for (char &c : s) c = (char) tolower((unsigned char) c);
  return s;
}

// a plate of one segment: the look lives in state styles, the segment trickles its state down. fill = the chosen /
// pressed surface; bw / sides = its line-hi ring (shown when chosen)
lv_obj_t *plate(lv_obj_t *seg, int x, int w, int h, int radius, lv_border_side_t sides, int bw, bool fill) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *o = lv_obj_create(seg);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_pos(o, x, 0);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, radius, 0);
  if (fill) {
    lv_obj_set_style_bg_color(o, fw::st::pressed_surface(t), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, ST_ON);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, ST_OFF);       // disabled: no press look
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, ST_ON | ST_OFF);  // ...but the choice still shows
  }
  lv_obj_set_style_border_width(o, bw, 0);
  lv_obj_set_style_border_side(o, sides, 0);
  lv_obj_set_style_border_color(o, fw::st::on_line(t), 0);
  lv_obj_set_style_border_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(o, LV_OPA_COVER, ST_ON);
  lv_obj_set_style_border_color(o, fw::st::disabled_line(t), ST_ON | ST_OFF);
  lv_obj_set_style_transition(o, tr(false), 0);
  lv_obj_set_style_transition(o, tr(true), LV_STATE_PRESSED);
  return o;
}

void choose(lv_obj_t *box, int i) {
  const int n = (int) ((Seg *) lv_obj_get_user_data(box))->ids.size();  // the segments (the ring child comes after)
  for (int k = 0; k < n; k++) {
    lv_obj_t *s = lv_obj_get_child(box, k);
    if (k == i) lv_obj_add_state(s, ST_ON);
    else lv_obj_remove_state(s, ST_ON);
  }
  ((Seg *) lv_obj_get_user_data(box))->chosen = i;
}
}  // namespace

namespace {
// structural props (a change = the segments again, fw::refill, snap); the choice and disabled move in place
std::string sig(fw::Props p) {
  return std::string(p["options"] | "Auto, Dark, Light") + "|" + (p["width"] | "hug") + "|" +
         (strcmp(p["state"] | "default", "unavailable") ? "" : "u");
}
void fill(lv_obj_t *box, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  auto *d = (Seg *) lv_obj_get_user_data(box);
  d->ids.clear();
  d->chosen = 0;
  fw::shape(box, sig(p));
  if (!strcmp(state, "unavailable")) {  // states.json unavailable: the word in place of the control (= Toggle)
    lv_obj_t *u = box;
    lv_obj_remove_style_all(u);
    lv_obj_add_flag(u, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(u, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(u, LV_SIZE_CONTENT, fw::touch::min);
    lv_obj_t *l = fw::text(u, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t));
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);  // one line at the type box (a new label wraps: font height)
    lv_obj_set_height(l, fw::style(fw::ty::body).box);
    lv_obj_center(l);
    lv_obj_update_layout(l);
    lv_obj_set_width(u, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);  // on the grid
    lv_obj_add_state(u, ST_OFF);  // the tap = blocked (the root's handler, made once in build)
    return;
  }
  const bool off = !strcmp(state, "disabled"), fill_w = !strcmp(p["width"] | "hug", "fill");

  // options: comma list of labels, id = label lower-cased
  std::vector<std::string> labels;
  {
    const std::string all = p["options"] | "Auto, Dark, Light";
    size_t a = 0;
    while (a <= all.size()) {
      size_t b = all.find(',', a);
      if (b == std::string::npos) b = all.size();
      const std::string s = trim(all.substr(a, b - a));
      if (!s.empty()) labels.push_back(s);
      a = b + 1;
    }
    if (labels.empty()) labels.push_back("Option");
  }
  const int n = (int) labels.size();
  for (auto &l : labels) d->ids.push_back(lower(l));
  const std::string sel = lower(trim(p["selected"] | ""));
  for (int i = 0; i < n; i++)
    if (d->ids[i] == sel) d->chosen = i;

  const int G = fw::sp::GRID, hair = fw::stroke::hair, pad = fw::sp::s7, h = fw::touch::min;
  const int R = fw::rad::button, ring = 2 * hair;  // plate ring: the box line over its outer px + line-hi inside
  static_assert(fw::rad::button > 2 * fw::stroke::hair, "the end plates follow the box's corner");

  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

  // segments: labels first (hug needs their widths), each segment width on the grid
  std::vector<lv_obj_t *> segs, labs;
  std::vector<int> ws;
  for (int i = 0; i < n; i++) {
    lv_obj_t *s = lv_obj_create(box);
    lv_obj_remove_style_all(s);
    lv_obj_add_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    if (i > 0) {  // the separator (docs border-left)
      lv_obj_set_style_border_width(s, hair, 0);
      lv_obj_set_style_border_side(s, LV_BORDER_SIDE_LEFT, 0);
      lv_obj_set_style_border_color(s, t.line, 0);
      lv_obj_set_style_border_opa(s, LV_OPA_COVER, 0);
    }
    lv_obj_t *l = fw::text(s, fw::ty::label, labels[i].c_str(), t.text_dim);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);  // one line at the type box (a new label wraps: font height)
    lv_obj_set_height(l, fw::style(fw::ty::label).box);
    lv_obj_set_style_text_color(l, t.text, ST_ON);  // chosen = full text colour (docs)
    lv_obj_set_style_text_color(l, fw::st::disabled_ink(t), ST_OFF);
    lv_obj_set_style_text_color(l, fw::st::disabled_on_fill(t), ST_ON | ST_OFF);  // text-mute fails 3:1 on the plate
    lv_obj_update_layout(l);
    ws.push_back((lv_obj_get_width(l) + 2 * pad + G - 1) / G * G);
    segs.push_back(s);
    labs.push_back(l);
  }
  if (fill_w) {  // equal segments over the content width, the last takes the remainder (all on the grid)
    const int each = fw::lay::content_w / n / G * G;
    for (int i = 0; i < n; i++) ws[i] = i < n - 1 ? each : fw::lay::content_w - each * (n - 1);
  }
  int W = 0;
  for (int w : ws) W += w;
  lv_obj_set_size(box, W, h);

  // place: every segment is a grid box (x = a sum of grid widths, full height); the box line is a ring child drawn
  // over them (below), so it sits on the box edge and nothing inside is pushed off the grid by it
  int x = 0;
  for (int i = 0; i < n; i++) {
    const int w = ws[i];
    lv_obj_t *s = segs[i];
    lv_obj_set_pos(s, x, 0);
    lv_obj_set_size(s, w, h);
    lv_obj_center(labs[i]);
    // plates cover the whole segment incl. its separator (children start after the separator: x = -hair). On the
    // box's outer sides the line-hi ring is 2 hairlines wide: the box line covers the outer one, so the chosen ring
    // shows just inside the box line (docs outline-offset -hair); on the separator sides it is 1 hairline
    const int px0 = i > 0 ? -hair : 0, pw = w;
    const auto TB = (lv_border_side_t) (LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM);
    if (n == 1) plate(s, px0, pw, h, R, LV_BORDER_SIDE_FULL, ring, true);
    else if (i == 0) {
      plate(s, px0, pw, h, R, LV_BORDER_SIDE_FULL, ring, true);
      plate(s, px0 + R, pw - R, h, 0, TB, ring, true);  // squares the inner corners
    } else if (i == n - 1) {
      plate(s, px0, pw, h, R, LV_BORDER_SIDE_FULL, ring, true);
      plate(s, px0, pw - R, h, 0, TB, ring, true);
    } else plate(s, px0, pw, h, 0, TB, ring, true);
    if (n > 1) {  // the separator sides, 1 hairline
      const int sides = (i > 0 ? LV_BORDER_SIDE_LEFT : 0) | (i < n - 1 ? LV_BORDER_SIDE_RIGHT : 0);
      plate(s, px0, pw, h, 0, (lv_border_side_t) sides, hair, false);
    }
    lv_obj_move_foreground(labs[i]);
    if (off) lv_obj_add_state(s, ST_OFF);
    x += w;

    fw::st::on_tap(s, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
      if (!fw::st::tap_now(e)) return;
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      lv_obj_t *b = lv_obj_get_parent(o);
      auto *d = (Seg *) lv_obj_get_user_data(b);
      const int i = (int) lv_obj_get_index(o);
      if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->ids[d->chosen].c_str()); return; }
      if (i != d->chosen) {
        choose(b, i);
        fw::emit(o, "change", d->ids[i].c_str());
      } else fw::emit(o, fw::st::TAP_EVENT, d->ids[i].c_str());  // the chosen one again: nothing changes
      fw::sound(o, fw::st::TAP_SOUND);
    }, nullptr);
  }
  // the box line: a ring on the box edge over the segments (their press / chosen fill runs under it)
  lv_obj_t *rg = lv_obj_create(box);
  lv_obj_remove_style_all(rg);
  lv_obj_remove_flag(rg, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(rg, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(rg, W, h);
  lv_obj_set_style_radius(rg, R, 0);
  lv_obj_set_style_border_width(rg, hair, 0);
  lv_obj_set_style_border_color(rg, t.line, 0);
  lv_obj_set_style_border_opa(rg, LV_OPA_COVER, 0);
  choose(box, d->chosen);
}
}  // namespace

FW_COMPONENT(segmented, "Segmented", "components", R"({
  "about": "One choice out of a few, side by side: a setting with up to 3 options, or the view tabs of a screen.",
  "frame": [480, 96],
  "props": [
    {"name": "options", "about": "The choices, separated by commas, up to three.", "type": "text", "default": "Auto, Dark, Light"},
    {"name": "selected", "about": "Which choice is picked, by its name in lower case.", "type": "text", "default": "auto"},
    {"name": "width", "about": "hug fits the control to its words; fill stretches it across the space.", "type": "select", "options": ["hug", "fill"], "default": "hug"},
    {"name": "state", "about": "default can be tapped; disabled cannot be changed; unavailable shows the setting cannot be read.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "setting · 3 options", "props": {}},
    {"label": "setting · 2 options", "props": {"options": "Mono, Analogue", "selected": "analogue"}},
    {"label": "tabs · Weather", "props": {"options": "Weather, Sun, Moon, Lightning", "selected": "weather"}},
    {"label": "tabs · Calendar", "props": {"options": "Agenda, Month", "selected": "month"}},
    {"label": "tabs · fill", "props": {"options": "Now, Radar", "selected": "now", "width": "fill"}},
    {"label": "disabled", "props": {"selected": "dark", "state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Seg;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_user_data(box, d);
  lv_obj_add_event_cb(box, [](lv_event_t *e) { delete (Seg *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fill(box, p);
  fw::st::on_tap(box, [](lv_event_t *e) {  // unavailable: the root is the word (clickable), a tap = blocked
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) && fw::st::tap_now(e)) fw::st::blocked(o, "unavailable");
  }, nullptr);
}

// Update path: another selected option = the choice moves (the chosen plate + label fade, fw::st::set_state ON),
// default <-> disabled fades in place (fw::st::set_state OFF on each segment); options, width or unavailable in / out
// = the segments again (fw::refill, snap; unavailable fades the new contents in). Silent.
FW_UPDATE(segmented) {
  auto *d = (Seg *) lv_obj_get_user_data(root);
  if (!d) return false;
  const bool was_unavail = !d->ids.size();
  if (!fw::shape(root, sig(p))) {
    fw::refill(root);
    fill(root, p);
    if (was_unavail != !d->ids.size()) fw::st::fade_in_children(root);
    return true;
  }
  if (!d->ids.size()) return true;  // unavailable: nothing else shows
  const std::string sel = lower(trim(p["selected"] | ""));
  int c = 0;
  for (int i = 0; i < (int) d->ids.size(); i++)
    if (d->ids[i] == sel) c = i;
  const bool off = !strcmp(p["state"] | "default", "disabled");
  for (int i = 0; i < (int) d->ids.size(); i++) {
    lv_obj_t *s = lv_obj_get_child(root, i);
    fw::st::set_state(s, ST_ON, i == c);
    fw::st::set_state(s, ST_OFF, off);
  }
  d->chosen = c;
  return true;
}
