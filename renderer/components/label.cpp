// Label: the Facet label atom (docs: @facet/ui atoms.ts label(text, type), CSS .f-label + fit8()): one line of text in
// one of the component type styles (tokens/type.json, the c-* "component text scale" = docs ft-<type>), no wrap.
//   box      width = the text width rounded UP to the grid (sp::GRID: 8 at 720, 4 at 480 = docs fit8), so whatever
//            follows it (a gap, a badge, the right side of a header) stays on the grid; height = the style's line box
//            (set here: fw::text leaves default labels at content height, see the core note in the facet skill)
//   colour   a colour token: text (default), text-dim, accent-text, blue, ok, warn, error; text-mute = disabled only
//            (contrast rule), so it is offered as "disabled"
// Static: no press look, no events, no sound (docs label is not interactive). Other parts build on it through
// fw::find("label") (Status chip, Count badge, Section header), so a theme that changes the label changes them all.
// Playground: an empty text shows the style's own sample (type.json "sample"), so switching the type shows that style
// with glyphs its font has (c-number is digits only). Contrast pairs: tokens/colour.json section "label".
#include "fw.h"

namespace {
struct TypeOpt { const char *name; fw::ty::Id id; };
// docs explorer TYPES order (site registry.ts), = the component text scale
const TypeOpt TYPES[] = {
    {"title", fw::ty::c_title},       {"card-title", fw::ty::c_card_title}, {"tile", fw::ty::c_tile},
    {"body", fw::ty::c_body},         {"label", fw::ty::c_label},           {"label-sm", fw::ty::c_label_sm},
    {"meta", fw::ty::c_meta},         {"meta-sm", fw::ty::c_meta_sm},       {"caps-sm", fw::ty::c_caps_sm},
    {"subtitle", fw::ty::c_subtitle}, {"date", fw::ty::c_date},             {"weather", fw::ty::c_weather},
    {"number", fw::ty::c_number},
};

fw::ty::Id type_of(const char *s) {
  for (const TypeOpt &t : TYPES)
    if (!strcmp(t.name, s)) return t.id;
  return fw::ty::c_meta;
}

lv_color_t colour_of(const char *c) {
  const fw::Theme &t = *fw::T;
  if (!strcmp(c, "text-dim")) return t.text_dim;
  if (!strcmp(c, "disabled")) return fw::st::disabled_ink(t);  // states.json
  if (!strcmp(c, "accent-text")) return t.accent_text;
  if (!strcmp(c, "blue")) return t.blue;
  if (!strcmp(c, "ok")) return t.ok;
  if (!strcmp(c, "warn")) return t.warn;
  if (!strcmp(c, "error")) return t.error;
  return t.text;
}
}  // namespace

FW_COMPONENT(label, "Label", "components", R"({
  "about": "One line of text in a component type style and colour.",
  "frame": [320, 80],
  "props": [
    {"name": "text", "type": "text", "default": ""},
    {"name": "type", "type": "select", "options": ["title", "card-title", "tile", "body", "label", "label-sm", "meta", "meta-sm", "caps-sm", "subtitle", "date", "weather", "number"], "default": "meta"},
    {"name": "colour", "type": "select", "options": ["text", "text-dim", "accent-text", "blue", "ok", "warn", "error", "disabled"], "default": "text"}
  ],
  "variants": [
    {"label": "title", "props": {"type": "title"}},
    {"label": "card-title", "props": {"type": "card-title"}},
    {"label": "tile", "props": {"type": "tile"}},
    {"label": "body", "props": {"type": "body"}},
    {"label": "label", "props": {"type": "label"}},
    {"label": "label-sm", "props": {"type": "label-sm"}},
    {"label": "meta", "props": {"type": "meta"}},
    {"label": "meta-sm", "props": {"type": "meta-sm"}},
    {"label": "caps-sm", "props": {"type": "caps-sm"}},
    {"label": "subtitle", "props": {"type": "subtitle"}},
    {"label": "date", "props": {"type": "date"}},
    {"label": "weather", "props": {"type": "weather"}},
    {"label": "number", "props": {"type": "number"}},
    {"label": "meta · text-dim", "props": {"type": "meta", "colour": "text-dim", "text": "done · 4h ago"}},
    {"label": "caps-sm · accent-text", "props": {"type": "caps-sm", "colour": "accent-text", "text": "3 working"}}
  ]
})") {
  const fw::ty::Id s = type_of(p["type"] | "meta");
  const char *txt = p["text"] | "";
  if (!txt[0]) txt = fw::style(s).sample;
  lv_obj_t *l = fw::text(parent, s, txt, colour_of(p["colour"] | "text"));
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_update_layout(l);
  const int g = fw::sp::GRID, w = lv_obj_get_width(l);
  lv_obj_set_width(l, (w + g - 1) / g * g);  // fit8: the box on the grid, the text at its left
  lv_obj_set_height(l, fw::style(s).box);    // the style's line box (fw::text leaves default labels at content height)
}
