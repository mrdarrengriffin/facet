// Foundations > Icons: tokens/icons.json (via the generated icons_gen.h + this density's A8 masks) drawn by the panel
// renderer, every icon at every size it ships in, true size. Sections: the size comparison (Facet docs Icons > Optical
// sizes: the same five icons at each UI size), one per group (UI groups, screen icons, weather), every icon, and the
// flash the masks cost on this build. Playground: search (name contains), size (one size token or all), accent tint
// (the ink in accent instead of text; a screen icon's accent part is always accent), boxes (outline each icon's
// S x S box). Theme + accent set come from the workbench toolbar. Status (WIP/Done) = this file + tokens/icons.json.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *row(lv_obj_t *parent, int gap, lv_flex_align_t cross = LV_FLEX_ALIGN_CENTER) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, cross, cross);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s3) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(e, stroke::hair, 0);
  lv_obj_set_style_border_color(e, T->line, 0);
  return e;
}
bool is1x() { return D < 1.2f; }

struct Opts {
  const char *search;
  int size;  // -1 = every size
  bool accent, boxes;
  bool animated = true;  // weather: the animated icons (Weather icon component) or the static masters
  float phase = 0.25f;   // moon phase glyph

  lv_color_t ink() const { return accent ? T->accent : T->text; }
};

lv_obj_t *moon(lv_obj_t *parent, float phase, ic::Size s, bool live);  // below (the Moon phase component)

// one icon at one size: the S x S box (outlined when boxes is on), true size
lv_obj_t *draw(lv_obj_t *parent, int i, ic::Size s, const Opts &o) {
  lv_obj_t *ic = icon(parent, ic::ICONS[i].name, s, o.ink());
  if (ic && o.boxes) {
    lv_obj_set_style_outline_width(ic, stroke::hair, 0);
    lv_obj_set_style_outline_color(ic, T->line_hi, 0);
    lv_obj_set_style_outline_pad(ic, 0, 0);
  }
  return ic;
}

bool matches(int i, const char *group, const Opts &o) {
  const ic::Info &n = ic::ICONS[i];
  if (*o.search) return strstr(n.name, o.search) != nullptr;  // a search looks through every group
  return !group || !strcmp(n.group, group);
}
bool ships(int i, int s) { return (ic::ICONS[i].sizes >> s) & 1; }

// a tile: the icon at each size it ships in (bottom-aligned, smallest first), the name, the sizes in px
bool tile(lv_obj_t *wrap, int i, const Opts &o) {
  const ic::Info &n = ic::ICONS[i];
  lv_obj_t *t = column(wrap, sp::s2, LV_SIZE_CONTENT);
  lv_obj_t *r = row(t, sp::s4, LV_FLEX_ALIGN_END);
  char sizes[64] = "";
  int shown = 0;
  for (int s = 0; s < ic::N_SIZES; s++) {
    if (!ships(i, s) || (o.size >= 0 && s != o.size)) continue;
    if (draw(r, i, (ic::Size) s, o)) shown++;
    char b[12];
    snprintf(b, sizeof b, "%s%d", *sizes ? " · " : "", ic::PX[s]);
    strncat(sizes, b, sizeof sizes - strlen(sizes) - 1);
  }
  if (!shown) { lv_obj_delete(t); return false; }
  text(t, ty::c_label, n.name, T->text);
  text(t, ty::c_meta_sm, sizes, T->text_dim);
  return true;
}

// a group (or every icon when group is null) as a wrapping grid of tiles
void grid(lv_obj_t *l, const char *group, const char *title, const Opts &o) {
  lv_obj_t *e = entry(l, sp::s5);
  if (title) text(e, ty::c_caps_sm, title, T->text_dim);
  lv_obj_t *w = box(e);
  lv_obj_set_size(w, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(w, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(w, sp::s7, 0);
  lv_obj_set_style_pad_row(w, sp::s7, 0);
  int n = 0;
  for (int i = 0; i < ic::N; i++)
    if (matches(i, group, o) && tile(w, i, o)) n++;
  if (group && !strcmp(group, "weather") && (!*o.search || strstr("moon", o.search))) {  // the moon phase glyph, drawn in code
    lv_obj_t *t = column(w, sp::s2, LV_SIZE_CONTENT);
    lv_obj_t *r = row(t, sp::s4, LV_FLEX_ALIGN_END);
    const ic::Size ms[] = {ic::glyph, ic::detail, ic::feature, ic::card, ic::hero};
    for (ic::Size s : ms)
      if (o.size < 0 || o.size == s) moon(r, o.phase, s, true);
    text(t, ty::c_label, "moon (phase)", T->text);
    text(t, ty::c_meta_sm, "drawn in code · any size", T->text_dim);
    n++;
  }
  if (!n) { lv_obj_delete(w); text(e, ty::c_meta, *o.search ? "No icon matches the search." : "None in this build.", T->text_dim); }
}

// the same five icons at every UI size: the optical stroke (1.5 / 2 / 2.5 / 3 px at 16 / 24 / 32 / 48) keeps them
// one family; at 480 the sizes are 12 / 16 / 20 / 32
void compare(lv_obj_t *l, const Opts &o) {
  for (int k = 0; k < ic::N_COMPARE_SIZES; k++) {
    const ic::Size s = ic::COMPARE_SIZES[k];
    if (o.size >= 0 && s != o.size) continue;
    lv_obj_t *e = entry(l, sp::s3);
    char b[64];
    const int p = ic::PX[s];
    const float tb = p <= ic::STROKE_TABLE[0][0] ? ic::STROKE_TABLE[0][1] : p >= ic::STROKE_TABLE[3][0] ? ic::STROKE_TABLE[3][1] : 0;
    float st = tb;
    for (int j = 1; !st && j < 4; j++)
      if (p <= ic::STROKE_TABLE[j][0])
        st = ic::STROKE_TABLE[j - 1][1] + (ic::STROKE_TABLE[j][1] - ic::STROKE_TABLE[j - 1][1]) * (p - ic::STROKE_TABLE[j - 1][0]) / (ic::STROKE_TABLE[j][0] - ic::STROKE_TABLE[j - 1][0]);
    snprintf(b, sizeof b, "ic::%s · %d px · stroke %.2g px", ic::SIZE_NAMES[s], p, (double) st);
    text(e, ty::c_caps_sm, b, T->text);
    lv_obj_t *r = row(e, sp::s7, LV_FLEX_ALIGN_END);
    for (int c = 0; c < ic::N_COMPARE; c++) {
      const int i = icon_index(ic::COMPARE[c]);
      if (i >= 0 && (!*o.search || strstr(ic::COMPARE[c], o.search))) draw(r, i, s, o);
    }
  }
  lv_obj_t *e = entry(l);
  text(e, ty::c_meta, "One master per icon, drawn at each size with Facet's optical stroke; every size is its own "
       "bitmap, never scaled on the panel.", T->text_dim, inner_w());
}

// flash per size token and per kind, this build
void memory(lv_obj_t *l) {
  char b[128];
  lv_obj_t *e = entry(l, sp::s2);
  snprintf(b, sizeof b, "%d icon x size bitmaps · %.1f KB flash (%s build%s)", icons_entries(), icons_bytes() / 1024.0,
           is1x() ? "480" : "720", icons_used_only() ? ", used icons only" : "");
  text(e, ty::c_caps_sm, b, T->text);
  text(e, ty::c_meta, "A8 alpha masks cropped to their ink, one per colour layer, tinted by a colour token on the panel.",
       T->text_dim, inner_w());
  for (int s = 0; s < ic::N_SIZES; s++) {
    int n = 0, by = 0;
    for (int i = 0; i < ic::N; i++)
      if (icon_has(i, (ic::Size) s)) n++, by += icon_bytes(i, (ic::Size) s);
    lv_obj_t *x = entry(l, sp::s1);
    snprintf(b, sizeof b, "ic::%s · %d px (%d at %s)", ic::SIZE_NAMES[s], ic::PX[s], is1x() ? ic::PX_V[s] : ic::PX_1X[s], is1x() ? "720" : "480");
    text(x, ty::c_caps_sm, b, T->text);
    snprintf(b, sizeof b, "%d icons · %.1f KB", n, by / 1024.0);
    text(x, ty::c_meta, b, T->text_dim);
    text(x, ty::c_meta_sm, ic::SIZE_USE[s], T->text_dim, inner_w());
  }
  const char *kinds[] = {"ui", "screen", "weather"};
  const char *labels[] = {"UI icons (one mask)", "Screen icons (ink + accent)", "Weather icons (one mask per colour)"};
  for (int k = 0; k < 3; k++) {
    int n = 0, by = 0;
    for (int i = 0; i < ic::N; i++) {
      if (strcmp(ic::ICONS[i].kind, kinds[k])) continue;
      bool any = false;
      for (int s = 0; s < ic::N_SIZES; s++)
        if (icon_has(i, (ic::Size) s)) any = true, by += icon_bytes(i, (ic::Size) s);
      n += any;
    }
    lv_obj_t *x = entry(l, sp::s1);
    text(x, ty::c_caps_sm, labels[k], T->text);
    snprintf(b, sizeof b, "%d icons · %.1f KB", n, by / 1024.0);
    text(x, ty::c_meta, b, T->text_dim);
  }
  int an = 0, ab = 0;
  for (int i = 0; i < ic::N; i++)
    for (int s = 0; s < ic::N_SIZES; s++)
      if (weather_anim_parts(i, (ic::Size) s)) an++, ab += weather_anim_bytes(i, (ic::Size) s);
  lv_obj_t *x = entry(l, sp::s1);
  text(x, ty::c_caps_sm, "Animated weather (parts, one mask per part and colour)", T->text);
  snprintf(b, sizeof b, "%d icon x size · %.1f KB", an, ab / 1024.0);
  text(x, ty::c_meta, b, T->text_dim);
}

// ---- animated weather + moon phase: the real components (Weather icon, Moon phase) built through the registry
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *comp(lv_obj_t *parent, const char *id, Doc &d) {
  const Component *c = find(id);
  if (!c) {
    char b[64];
    snprintf(b, sizeof b, "%s: component coming", id);
    return text(parent, ty::c_meta_sm, b, T->text_dim);
  }
  c->build(parent, d.as<Props>());
  return lv_obj_get_child(parent, -1);
}
lv_obj_t *weather(lv_obj_t *parent, const char *name, ic::Size s, bool animated, bool card = false) {
  Doc d;
  d["icon"] = name;
  d["size"] = ic::SIZE_NAMES[s];
  d["animated"] = animated;
  d["on"] = card ? "card" : "canvas";
  return comp(parent, "weather_icon", d);
}
constexpr lv_obj_flag_t LIVE_MOON = LV_OBJ_FLAG_USER_2;  // follows the playground phase (update path)
lv_obj_t *moon(lv_obj_t *parent, float phase, ic::Size s, bool live) {
  Doc d;
  d["phase"] = phase;
  d["size"] = ic::SIZE_NAMES[s];
  lv_obj_t *r = comp(parent, "moon_icon", d);
  if (live && r) lv_obj_add_flag(r, LIVE_MOON);
  return r;
}
bool is_anim(int s) { return (ic::ANIM_SIZES >> s) & 1; }
void walk(lv_obj_t *o, bool animated, float phase);
// the real Segmented / Slider on the page drive the icons in place; a choice there is a viewing setting, not a demo to
// reset (fw::demo_away false = the host's demo reset skips it)
void on_choice(lv_obj_t *, const char *name, const char *detail, void *user) {
  lv_obj_t *l = (lv_obj_t *) user;
  if (strcmp(name, "change") && strcmp(name, "input")) return;  // the phase follows the drag live (Slider "input")
  if (!strcmp(detail, "animated") || !strcmp(detail, "static")) walk(l, !strcmp(detail, "animated"), -1);
  else walk(l, true, (float) atof(detail) / 100.f);
  demo_away(l, false);
}
// Animated weather: every weather icon as the Weather icon component (animated or static = the playground toggle) at
// the chosen animated size (card by default), then the Home card / Weather hero sizes for one icon, on a card
void animated(lv_obj_t *l, const Opts &o) {
  const ic::Size s = o.size >= 0 && is_anim(o.size) ? (ic::Size) o.size : ic::card;
  lv_obj_t *e = entry(l, sp::s5);
  char b[96];
  {
    Doc d;
    d["options"] = "Animated, Static";
    d["selected"] = o.animated ? "animated" : "static";
    comp(e, "segmented", d);
    listen(e, on_choice, l);
  }
  snprintf(b, sizeof b, "ic::%s · %d px", ic::SIZE_NAMES[s], ic::PX[s]);
  text(e, ty::c_caps_sm, b, T->text_dim);
  lv_obj_t *w = box(e);
  lv_obj_set_size(w, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(w, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(w, sp::s7, 0);
  lv_obj_set_style_pad_row(w, sp::s7, 0);
  int n = 0;
  for (int i = 0; i < ic::N; i++) {
    if (strcmp(ic::ICONS[i].kind, "weather") || (*o.search && !strstr(ic::ICONS[i].name, o.search)) || !icon_has(i, s)) continue;
    lv_obj_t *t = column(w, sp::s2, LV_SIZE_CONTENT);
    weather(t, ic::ICONS[i].name, s, o.animated);
    text(t, ty::c_label, ic::ICONS[i].name, T->text);
    snprintf(b, sizeof b, "%d parts · %.1f KB", weather_anim_parts(i, s), weather_anim_bytes(i, s) / 1024.0);
    text(t, ty::c_meta_sm, b, T->text_dim);
    n++;
  }
  if (!n) { lv_obj_delete(w); text(e, ty::c_meta, "None in this build.", T->text_dim); }
  lv_obj_t *x = entry(l, sp::s3);
  text(x, ty::c_caps_sm, "On a card", T->text_dim);
  lv_obj_t *r = row(x, sp::s5, LV_FLEX_ALIGN_END);
  weather(r, "partly-day", ic::card, o.animated, true);
  weather(r, "thunder-rain", ic::card, o.animated, true);
  weather(r, "rain-heavy", ic::hero, o.animated, true);
  lv_obj_t *m = entry(l, sp::s2);
  text(m, ty::c_meta, "Each icon is the static master cut into its parts: the sun's rays turn once every 24 s, clouds "
       "drift 1-2 units, drops, snow and hail fall and fade, the bolt flashes twice every 4 s, fog and wind lines slide, "
       "stars twinkle. Timings and curves are Motion tokens (Motion > Patterns > Weather icon). A moving cloud is "
       "filled with the colour behind it, so it still hides the sun. With reduced motion, or the toggle off, the icon "
       "is exactly the static master.", T->text_dim, inner_w());
}
// Moon phase: the playground phase at every size, then the month in eight steps
void moons(lv_obj_t *l, const Opts &o) {
  lv_obj_t *e = entry(l, sp::s5);
  text(e, ty::c_caps_sm, "Phase (0 new · 50 full · 100 new again)", T->text_dim);
  {
    Doc d;
    d["value"] = (int) lroundf(o.phase * 100);
    d["min"] = 0;
    d["max"] = 100;
    d["step"] = 1;
    d["width"] = 288;
    comp(e, "slider", d);
    listen(e, on_choice, l);
  }
  lv_obj_t *r = row(e, sp::s5, LV_FLEX_ALIGN_END);
  const ic::Size sizes[] = {ic::icon, ic::glyph, ic::feature, ic::card, ic::hero};
  for (ic::Size s : sizes)
    if (o.size < 0 || o.size == s) moon(r, o.phase, s, true);
  lv_obj_t *x = entry(l, sp::s5);
  text(x, ty::c_caps_sm, "The month", T->text_dim);
  lv_obj_t *w = row(x, sp::s4, LV_FLEX_ALIGN_END);
  for (int k = 0; k < 8; k++) moon(w, k / 8.f, ic::feature, false);
  lv_obj_t *m = entry(l, sp::s2);
  text(m, ty::c_meta, "Drawn in code from the phase: a dark disc, the lit part (the lit edge plus the shadow line, "
       "half an ellipse) and the outline at the weather stroke. A new phase redraws only the lit part.", T->text_dim, inner_w());
}

int size_of(const char *s) {
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return i;
  return -1;
}
// the page into its root column l (build and refill)
std::string sig(Props p) {
  char b[160];
  snprintf(b, sizeof b, "%s|%s|%s|%d|%d", p["section"] | "compare", p["search"] | "", p["size"] | "all", (int) (p["accent"] | false), (int) (p["boxes"] | false));
  return b;
}
// animated -> every Weather icon on the page, phase (>= 0) -> every live Moon phase
void walk(lv_obj_t *o, bool animated, float phase) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, (int32_t) i);
    if (const char *n = weather_icon_name(c)) { if (phase < 0) weather_icon_set(c, n, animated); continue; }
    if (lv_obj_has_flag(c, LIVE_MOON)) { if (phase >= 0) moon_icon_set(lv_obj_get_child(c, 0), phase); continue; }
    walk(c, animated, phase);
  }
}
void page(lv_obj_t *l, Props p) {
  fw::shape(l, sig(p));
  lv_obj_set_width(l, px(W));
  lv_obj_set_height(l, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(l, 0, 0);
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  const char *s = p["section"] | "compare";
  const Opts o{p["search"] | "", size_of(p["size"] | "all"), p["accent"] | false, p["boxes"] | false, p["animated"] | true, p["phase"] | 0.25f};
  if (!strcmp(s, "memory")) memory(l);
  else if (!strcmp(s, "animated")) animated(l, o);
  else if (!strcmp(s, "moon")) moons(l, o);
  else if (*o.search) grid(l, nullptr, nullptr, o);  // a search: one grid across every group
  else if (!strcmp(s, "compare")) compare(l, o);
  else if (!strcmp(s, "all")) {
    for (int g = 0; g < ic::N_GROUPS; g++) grid(l, ic::GROUPS[g].id, ic::GROUPS[g].label, o);
  } else {
    const char *label = s;
    for (int g = 0; g < ic::N_GROUPS; g++)
      if (!strcmp(ic::GROUPS[g].id, s)) label = ic::GROUPS[g].label;
    grid(l, s, label, o);
  }
}
}  // namespace

FW_COMPONENT(icons, "Icons", "foundations", R"({
  "about": "The icon set, drawn at every size it ships in.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "type": "select", "options": )" FW_ICON_SECTION_IDS R"(, "default": "compare"},
    {"name": "search", "type": "text", "default": ""},
    {"name": "size", "type": "select", "options": )" FW_ICON_SIZE_IDS R"(, "default": "all"},
    {"name": "accent", "type": "bool", "default": false},
    {"name": "boxes", "type": "bool", "default": false},
    {"name": "animated", "type": "bool", "default": true},
    {"name": "phase", "type": "range", "min": 0, "max": 1, "step": 0.01, "default": 0.25}
  ],
  "variants": )" FW_ICON_SECTIONS R"(
})") {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  page(l, p);
}

// Update path: the page again in the same root (the Animated / Static choice and the phase are the real Segmented /
// Slider on the page, which drive the icons in place through the components' own calls). Silent.
FW_UPDATE(icons) {
  fw::refill(root);
  page(root, p);
  return true;
}
