// Foundations > Sports surfaces: tokens/surfaces.json (via surfaces_gen.h, runtime core/surface.h) = how playing
// surfaces are drawn: every surface and venue preset as the real Tennis court component, the rules in plain words,
// the same court small and large, where the surface comes from in the data. For people (no token names on the page).
// Status = this file + tokens/surfaces.json (WIP until approved).
#include "fw.h"
#include "surface.h"

namespace {
lv_obj_t *column(lv_obj_t *parent, int gap) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *wrap(lv_obj_t *parent, int gap) {
  lv_obj_t *c = column(parent, gap);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(c, gap, 0);
  return c;
}
// content width in the components' dp (they take dp, px = fw::px(dp)), on the 8 dp step of the court's width prop
int page_dp() { return (fw::lay::screen_w - 2 * fw::sp::s7) * 480 / fw::lay::screen_w / 8 * 8; }
int px_of(int dp) { return fw::px((float) dp); }
void court(lv_obj_t *parent, const char *surface, int dp, const char *feed = nullptr) {
  const fw::Component *c = fw::find("tennis_court");
  if (!c) return;
  ArduinoJson::JsonDocument d;
  d["sim"] = false, d["width"] = dp, d["surface"] = surface;
  if (feed) d["feed"] = feed;
  c->build(parent, d.as<fw::Props>());
}
// one surface: the court, its name, one line about it
void card(lv_obj_t *parent, const fw::sf::Surface &s, int dp) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *c = column(parent, fw::sp::s2);
  lv_obj_set_width(c, px_of(dp));
  court(c, s.id, dp);
  lv_obj_t *gap = lv_obj_get_child(c, -1);
  if (gap) lv_obj_set_style_margin_bottom(gap, fw::sp::s1, 0);
  fw::text(c, fw::ty::c_label, s.label, t.text);
  fw::text(c, fw::ty::c_meta, s.about, t.text_dim, px_of(dp));
}
void grid(lv_obj_t *page, bool venues) {
  const int gap = fw::sp::s5, dp = (page_dp() - 16) / 2 / 8 * 8;
  lv_obj_t *w = wrap(page, gap);
  for (const fw::sf::Surface &s : fw::sf::SURFACES)
    if (s.venue == venues) card(w, s, dp);
}
void rules(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  court(page, "hard", page_dp(), R"({"state":"live","players":[{"short":"LIN"},{"short":"MOR"}],"sets":[[2,1]],"points":[1,0],"server":0})");
  for (const fw::sf::Rule &r : fw::sf::RULES) {
    lv_obj_t *c = column(page, fw::sp::s1);
    fw::text(c, fw::ty::c_label, r.title, t.text);
    fw::text(c, fw::ty::c_body, r.about, t.text_dim, px_of(page_dp()));
  }
}
void sizes(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *w = wrap(page, fw::sp::s5);
  lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  court(w, "australian-open", 160);
  court(w, "australian-open", page_dp() - 160 - 16 >= 160 ? (page_dp() - 176) / 8 * 8 : page_dp());
  fw::text(page, fw::ty::c_meta, "Small or large, the lines keep one weight and the edges land on the grid; only the court grows.", t.text_dim, px_of(page_dp()));
}
void data(lv_obj_t *page) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *w = wrap(page, fw::sp::s5);
  const int dp = (page_dp() - 16) / 2 / 8 * 8;
  for (const char *word : {"Clay", "Grass"}) {
    char f[160];
    snprintf(f, sizeof f, R"({"state":"live","surface":"%s","players":[{"short":"LIN"},{"short":"MOR"}],"sets":[[0,0]],"points":[0,0],"server":1})", word);
    lv_obj_t *c = column(w, fw::sp::s2);
    lv_obj_set_width(c, px_of(dp));
    court(c, "auto", dp, f);
    char b[64];
    snprintf(b, sizeof b, "The data says \"%s\"", word);
    fw::text(c, fw::ty::c_meta, b, t.text_dim, px_of(dp));
  }
}
}  // namespace

FW_COMPONENT(surfaces, "Sports surfaces", "foundations", R"({
  "about": "How playing surfaces are drawn: true to the venue, in Facet's own flat style. Tennis first; football and F1 follow the same rules.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "type": "select", "options": ["surfaces", "venues", "rules", "sizes", "data"], "default": "surfaces"}
  ],
  "variants": [
    {"label": "Surfaces", "about": "The five tennis surfaces in their real colours, toned down to sit on the canvas. Switch the theme: dark goes deeper, light stays mid-tone so the white lines still read.", "props": {"section": "surfaces"}},
    {"label": "Venues", "about": "Presets for the big events: recognisable at a glance, drawn the Facet way, with no logos or sponsor boards.", "props": {"section": "venues"}},
    {"label": "Rules", "about": "What every surface follows, with a live court: the server's dot sits in the run-off and the box they serve into is lifted, under the lines.", "props": {"section": "rules"}},
    {"label": "Sizes", "about": "The same venue small and large.", "props": {"section": "sizes"}},
    {"label": "From the data", "about": "When a source names the surface, the court follows it; when it doesn't, a venue table fills the gap, and the plain hard court is the fallback.", "props": {"section": "data"}}
  ]
})") {
  const char *s = p["section"] | "surfaces";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *page = column(parent, fw::sp::s5);
  lv_obj_set_style_pad_hor(page, fw::sp::s7, 0);
  if (!strcmp(s, "venues")) grid(page, true);
  else if (!strcmp(s, "rules")) rules(page);
  else if (!strcmp(s, "sizes")) sizes(page);
  else if (!strcmp(s, "data")) data(page);
  else grid(page, false);
}
