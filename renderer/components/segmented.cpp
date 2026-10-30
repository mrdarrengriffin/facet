// Segmented: the Facet segmented control (docs: @facet/ui settings.ts segmented(), CSS .f-seg / .f-seg-b), on the
// contract (renderer/CONTRACT.md, "contract": 2). One segment per option in a hairline box; exactly one is chosen.
// Docs uses: a Settings row with up to 3 options inline (Theme auto / dark / light), and the view tabs of Weather,
// Calendar and Overhead.
//   size     touch:min tall ("seg", core/fx_core.cpp); width hug = its segments (each its label on the 4 px grid +
//            space 7 each side, so every separator lands on the grid), or fill = its parent's width, equal segments
//   minimum  touch:min x touch:min (one option). Steps: one
//   box      the hair line ring (border:line, radius:button) is the last child, over the segments, so every segment
//            box starts on the box edge; segments after the first have a hair separator (bg:line) at their left
//   chosen   LVGL CHECKED: plate pressed-surface + an on-line ring (2 hairlines on the box's outer sides: the box line
//            covers the outer one, so it shows just inside it; 1 on the separator sides), label text (rest text-dim).
//            End plates round the outer corners and stay square inside (a rounded plate + a square half over it)
//   pressed  plate pressed-surface, label unchanged, Motion press / release fades (fade-press-fill)
//   disabled fw::st::OFF on every segment: labels disabled-ink, the chosen plate keeps its fill with a disabled-line
//            ring and its label goes disabled-on-fill; no press look; finger down = blocked + error
//   unavailable  the word Unavailable (body, unavailable-note) in place of the control, touch:min tall (= Toggle)
// Every look is a shared colour style per state (C17), so a live theme switch repaints it (the legacy one kept its
// colours). Tap: a real CLICKED on a segment chooses it: "change" (detail = the option id), or "tap" on the one
// already chosen; sound tap either way. Bleed: none. Contrast pairs: tokens/colour.json section "segmented".
// Props: options = comma list of labels (id = the label lower-cased), selected = id or label (default the first),
// width hug | fill, state.
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;

struct Seg {
  std::vector<std::string> ids;
  int chosen = 0;
};
Seg *of(lv_obj_t *o) { return (Seg *) lv_obj_get_user_data(o); }

std::string trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string lower(std::string s) {
  for (char &c : s) c = (char) tolower((unsigned char) c);
  return s;
}

// a plate of one segment: its look lives in state styles (the segment trickles its state down). fill = the chosen /
// pressed surface; ring = its on-line ring style (shown when chosen); place = "seg-plate" or a half
lv_obj_t *plate(lv_obj_t *seg, const char *place, bool round, const char *ring, bool fill) {
  lv_obj_t *o = fx::box(seg, "free");
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  fx::use(o, place);
  fx::use(o, "seg-layer");  // gone when the theme draws the segment part (C46)
  if (round) fx::use(o, "radius:button");
  if (fill) {
    for (const char *t : {"bg:pressed-surface", "bg-none", "fade-release-fill"}) fx::use(o, t);
    fx::use(o, "bg:pressed-surface", LV_STATE_PRESSED);
    fx::use(o, "fade-press-fill", LV_STATE_PRESSED);  // part: a segment is the Segmented's own part, not a component of its own
    fx::use(o, "bg:pressed-surface", ST_ON);
    fx::use(o, "bg-none", ST_OFF);                         // disabled: no press look
    fx::use(o, "bg-none", ST_OFF | LV_STATE_PRESSED);
    fx::use(o, "bg:pressed-surface", ST_ON | ST_OFF);      // ...but the choice still shows
  }
  fx::use(o, "border:text-dim", ST_ON);  // 3:1 on the box (WCAG 1.4.11): the chosen segment's outline
  fx::use(o, ring, ST_ON);
  fx::use(o, "border:disabled-ink", ST_ON | ST_OFF);
  fx::use(o, ring, ST_ON | ST_OFF);
  return o;
}

void choose(lv_obj_t *box, int i) {
  const int n = (int) of(box)->ids.size();  // the segments (the ring child comes after)
  for (int k = 0; k < n; k++) fx::state(lv_obj_get_child(box, k), ST_ON, k == i);
  of(box)->chosen = i;
}

// structural props (a change = the segments again, fw::refill, snap); the choice and disabled move in place
std::string sig(fw::Props p) {
  return std::string(p["options"] | "Auto, Dark, Light") + "|" + (p["width"] | "hug") + "|" +
         (strcmp(p["state"] | "default", "unavailable") ? "" : "u");
}
void fill(lv_obj_t *box, fw::Props p) {
  const char *state = p["state"] | "default";
  auto *d = of(box);
  d->ids.clear();
  d->chosen = 0;
  fw::shape(box, sig(p));
  if (!strcmp(state, "unavailable")) {  // states.json unavailable: the word in place of the control (= Toggle)
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    fx::use(box, "word-slot");
    fx::fit_grid(fx::text(box, fw::st::UNAVAILABLE_TEXT, "type:body", "fg:unavailable-note"));
    lv_obj_add_state(box, ST_OFF);  // the tap = blocked (the root's handler, made once in build)
    return;
  }
  const bool off = !strcmp(state, "disabled"), fill_w = !strcmp(p["width"] | "hug", "fill");

  std::vector<std::string> labels;  // options: comma list of labels, id = label lower-cased
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

  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::use(box, fill_w ? "fill-w" : "hug");
  fx::use(box, "seg");  // after hug: the box's own height on top of hug's content height
  fx::use(box, "part:control");  // the theme's control (C46): the box behind the segments

  for (int i = 0; i < n; i++) {
    lv_obj_t *s = fx::box(box, "free");
    lv_obj_add_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));  // part: a segment is the Segmented's own part
    lv_obj_remove_flag(s, LV_OBJ_FLAG_EVENT_BUBBLE);
    fx::use(s, "seg-part");
    if (i > 0) {  // the separator (docs border-left): a hairline child at the segment's left edge, under the plates
      lv_obj_t *sep = fx::box(s, "free");
      lv_obj_add_flag(sep, LV_OBJ_FLAG_IGNORE_LAYOUT);
      fx::use(sep, "seg-sep");
      fx::use(sep, "bg:line");
      fx::use(sep, "seg-layer");
    }
    // plates cover the whole segment incl. its separator; on the box's outer sides the ring is 2 hairlines
    if (n == 1) plate(s, "seg-plate", true, "ring-full-2", true);
    else if (i == 0) { plate(s, "seg-plate", true, "ring-full-2", true); plate(s, "seg-half-r", false, "ring-tb-2", true); }
    else if (i == n - 1) { plate(s, "seg-plate", true, "ring-full-2", true); plate(s, "seg-half-l", false, "ring-tb-2", true); }
    else plate(s, "seg-plate", false, "ring-tb-2", true);
    if (n > 1) plate(s, "seg-plate", false, i == 0 ? "ring-r" : i == n - 1 ? "ring-l" : "ring-lr", false);  // separator sides, 1 hairline

    fx::use(s, "part:segment");  // the theme's segment (C46): a theme that styles it draws each segment (Win98: raised, the chosen one sunken)
    lv_obj_t *l = fx::text(s, labels[i].c_str(), "type:label", "fg:text-dim");
    fx::use(l, "fg:text", ST_ON);  // chosen = full text colour (docs)
    fx::use(l, "fg:disabled-ink", ST_OFF);
    fx::use(l, "fg:disabled-on-fill", ST_ON | ST_OFF);  // disabled-ink fails 3:1 on the plate
    fx::use(l, "seg-label");  // space 7 each side
    fx::fit_grid(l);
    if (fill_w) lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    if (off) fx::state(s, ST_OFF, true);

    fw::st::on_tap(s, [](lv_event_t *e) {  // part: a segment's tap (blocked on finger down, a real tap on CLICKED)
      if (!fw::st::tap_now(e)) return;
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      lv_obj_t *b = lv_obj_get_parent(o);
      auto *d = of(b);
      const int i = (int) lv_obj_get_index(o);
      if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->ids[d->chosen].c_str()); return; }
      if (i != d->chosen) {
        choose(b, i);
        fw::emit(o, "change", d->ids[i].c_str());
      } else fw::emit(o, fw::st::TAP_EVENT, d->ids[i].c_str());  // the chosen one again: nothing changes
      fw::sound(o, fw::st::TAP_SOUND);
    }, nullptr);
  }
  if (fill_w) fx::equal(box);  // equal segments on the grid, the last takes the remainder
  // the box line: a ring on the box edge over the segments (their press / chosen fill runs under it)
  lv_obj_t *rg = fx::box(box, "free");
  lv_obj_add_flag(rg, LV_OBJ_FLAG_IGNORE_LAYOUT);
  for (const char *t : {"seg-plate", "border:line", "radius:button", "seg-layer"}) fx::use(rg, t);
  choose(box, d->chosen);
}
}  // namespace

FW_COMPONENT(segmented, "Segmented", "components", R"({
  "about": "One choice out of a few, side by side: a setting with up to 3 options, or the view tabs of a screen.",
  "contract": 2,
  "parts": ["control", "segment"],
  "min": ["touch:min", "touch:min"],
  "frame": [480, 96],
  "props": [
    {"name": "options", "about": "The choices, separated by commas, up to three.", "type": "text", "default": "Auto, Dark, Light"},
    {"name": "selected", "about": "Which choice is picked, by its name in lower case.", "type": "text", "default": "auto"},
    {"name": "width", "about": "hug fits the control to its words; fill stretches it across the space.", "type": "select", "options": ["hug", "fill"], "default": "hug"},
    {"name": "state", "debug": true, "about": "default can be tapped; disabled cannot be changed; unavailable shows the setting cannot be read.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
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
  lv_obj_remove_style_all(box);
  fx::name(box, "segmented");
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
  auto *d = of(root);
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
