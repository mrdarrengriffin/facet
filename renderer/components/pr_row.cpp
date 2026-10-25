// Pull request row: the Facet pull request row (docs: Components > Lists > Pull request row, @facet/ui lists.ts prRow()
// on listRow()): lay::pr_row_h (144 / 96), full width (content_w), surface + 1 px line, rad::card, padding lay::row_pad
// - hair, a column: meta (c-meta, text-dim: repo · #number, grow, age; docs number / age text-mute, which is
// disabled-only here), sp::s2, the title (c-tile, text, up to two lines in 2 x its box, ellipsis), sp::s2, the chips
// (Status chip components, sp::s2 apart, tone + words from fw::gh::state): review, then draft, then the CI chip.
// Pressed (states.json, the Card's press: the press-tint fill, line-hi edge, Motion press / release); tap = "tap"
// (detail "repo#number") + sound open. Update path: pressed <-> default fades, anything else refills (fw::st::update).
// Contrast pairs: colour.json sections "card" (press), "chart" (meta text-dim), "status_chip".
// audit deps: status_chip
#include "fw.h"
#include "github.h"

namespace {
const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }
  return &d[pressed];
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *meta(lv_obj_t *parent, const char *s, bool grow = false) {
  lv_obj_t *l = fw::text(parent, fw::ty::c_meta, s, fw::T->text_dim);
  // (its height = the style box: fw::text sizes it; an explicit height here cut the g of "seedling")
  if (grow) { lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS); lv_obj_set_flex_grow(l, 1); }
  else { lv_obj_update_layout(l); lv_obj_set_width(l, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID); }
  return l;
}
void fill(lv_obj_t *c, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(c);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::pr_row_h);
  lv_obj_set_style_radius(c, fw::rad::card, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, fw::stroke::hair, 0);
  const lv_style_selector_t SEL[2] = {0, LV_STATE_PRESSED};
  const lv_color_t BG[2] = {t.surface, fw::st::pressed_tint(t)}, BD[2] = {t.line, fw::st::pressed_line(t)};
  for (int i = 0; i < 2; i++) {
    lv_obj_set_style_bg_color(c, BG[i], SEL[i]);
    lv_obj_set_style_border_color(c, BD[i], SEL[i]);
    lv_obj_set_style_transition(c, tr(i == 1), SEL[i]);
  }
  lv_obj_set_style_pad_all(c, fw::lay::row_pad - fw::stroke::hair, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, fw::sp::s2, 0);
  lv_obj_t *m = box(c);
  // the line's box reaches a step below the text (descenders: the g of "seedling" was cut nested in a list, 2026-10-23);
  // the title pulls up by the same step, so the layout is the docs'
  lv_obj_set_size(m, LV_PCT(100), fw::style(fw::ty::c_meta).box + fw::sp::s1);
  lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(m, fw::sp::s2, 0);
  meta(m, p["repo"] | "hearth-ui");
  char num[16];
  snprintf(num, sizeof num, "#%d", (int) (p["number"] | 88));
  meta(m, num, true);
  meta(m, p["age"] | "5h");
  lv_obj_t *ti = fw::text(c, fw::ty::c_tile, p["title"] | "Card grid: snap scrolling one row at a time", t.text, 1);
  lv_obj_set_style_margin_top(ti, -fw::sp::s1, 0);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(ti, LV_PCT(100), 2 * fw::style(fw::ty::c_tile).box);
  lv_obj_t *chips = box(c);
  lv_obj_set_size(chips, LV_PCT(100), fw::lay::chip_h);
  lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(chips, fw::sp::s2, 0);
  if (const fw::Component *sc = fw::find("status_chip")) {
    std::vector<std::string> kinds = {"review"};
    if (p["draft"] | false) kinds.push_back("draft");
    const char *ci = p["ci"] | "pass";
    if (strcmp(ci, "none")) kinds.push_back(std::string("ci-") + ci);
    for (const auto &k : kinds) {
      const fw::gh::Status s = fw::gh::state(k);
      ArduinoJson::JsonDocument cp;
      cp["tone"] = s.tone, cp["text"] = s.text;
      sc->build(chips, cp.as<fw::Props>());
    }
  }
  fw::force_state(c, p);
}
}  // namespace

FW_COMPONENT(pr_row, "Pull request row", "components", R"({
  "about": "A pull request row: repo, number and age, a two-line title, and chips.",
  "frame": [464, 128],
  "props": [
    {"name": "repo", "about": "The repository the pull request is in.", "type": "text", "default": "hearth-ui"},
    {"name": "number", "about": "The pull request's number.", "type": "range", "min": 1, "max": 9999, "step": 1, "default": 88},
    {"name": "title", "about": "The pull request's title, over up to two lines.", "type": "text", "default": "Card grid: snap scrolling one row at a time"},
    {"name": "age", "about": "How long ago it was opened or last changed, such as 5h.", "type": "text", "default": "5h"},
    {"name": "draft", "about": "Marks it as a draft that is not ready for review yet.", "type": "bool", "default": false},
    {"name": "ci", "about": "How its automatic checks went: pass, fail, pending, or none when it has no checks.", "type": "select", "options": ["pass", "fail", "pending", "none"], "default": "pass"},
    {"name": "state", "about": "default ready to tap; pressed while held.", "type": "select", "options": ["default", "pressed"], "default": "default"}
  ],
  "variants": [
    {"label": "review · ci pass", "props": {}},
    {"label": "draft · ci running", "props": {"draft": true, "ci": "pending", "number": 412, "title": "Lantern: dim the hallway after midnight"}},
    {"label": "long title", "props": {"title": "Sensors: debounce the door contact so a slow close does not send two events, and log the bounce count for tuning", "ci": "fail"}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "descenders · seedling", "props": {"repo": "seedling", "number": 1290, "title": "Fix the sunrise time drifting by a minute each week", "age": "3d"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);
  fw::st::built(c, p);
  fw::st::on_tap(c, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    lv_obj_t *m = lv_obj_get_child(o, 0);
    const std::string d = std::string(lv_label_get_text(lv_obj_get_child(m, 0))) + lv_label_get_text(lv_obj_get_child(m, 1));
    fw::emit(o, "tap", d.c_str());
    fw::sound(o, "open");
  }, nullptr);
}

FW_UPDATE(pr_row) { return fw::st::update(root, p, fill); }
