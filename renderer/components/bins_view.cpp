// Bins view: the Bins screen's body (the panel lab's Bins screen, board facet-72, its v3 = v1's layout with the
// "Faithful" bin; the factory spec renderer/specs/bins.json adds the header): when the bins go out.
//   hero     a flex row, a gutter apart: the next collection's bins drawn in their colours (lay::bin_hero tall, each
//            next bin 55 % of a bin to the right and behind) and a column: the caption (type label, caps: "put out
//            tonight" / "collection today" = accent-text, "collected today · bring it in" / "next collection" =
//            text-dim), sp::s2, when (type value: "Tomorrow", "Today", the weekday), which bins (type title, text:
//            "Green bin + Black bin"), the date and how often (type body, text-dim: "Fri 24 Oct · every 2 weeks")
//   coming   lay::group down: a real Section header "coming up" and one real List row per later collection (title =
//            the bins, sub = the date, trailing value "in N days")
//   bins     lay::group down: "your bins", one List row per bin (title, sub "every 2 weeks", value "next Fri 31 Oct")
//   none     no collections: a real Empty state ("No collections found", where to pick the schedule)
// The bin (the "Faithful" pick, apps/hub panel/bins/bin-art.ts in plain boxes, LVGL-safe): handle loop, a slab lid
// over the body, a band under the lid, three slots; two flat tones = the bin colour (tokens/colour.json bins group)
// and its shade (black over it at comp bin.shade); black and grey bins get a line-hi edge so they read on the canvas
// (stroke::edge on the hero, stroke::hair on smaller bins). Its parts sit at the bin's own proportions (qa::free).
// Contextual (the lab's phases): tonight (collection tomorrow) > today (before 12:00) > collected (today after 12:00) >
// later; accent only for the first two, the moments that matter.
// Feed: props sample (tonight / today / collected / later / two / none; sample dates from Thu 23 Oct 2026). Live = a
// waste calendar in Home Assistant (Waste Collection Schedule, HACS) or a council iCal link (sources.json "bins"):
// the panel reads the calendar's events; until that read exists on the device, the sample.
// Static to touch. Update path: refilled in place. Contrast: bins (decor), text, section_header, list_row, empty_state.
// audit deps: section_header, list_row, empty_state
#include "fw.h"

namespace {
struct Bin { const char *name, *colour; int every; };  // every = days between collections
const Bin BINS[] = {{"Black bin", "black", 14}, {"Green bin", "green", 14}, {"Brown bin", "brown", 14}};
struct Coll { int day; std::vector<int> bins; };  // day = days from the sample's today; bins = BINS indexes
const char *const WD[] = {"Thursday", "Friday", "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday"};  // day 0 = Thu
const char *const WD3[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};

lv_color_t colour_of(const std::string &c) {
  const fw::Theme &t = *fw::T;
  return c == "black" ? t.bin_black : c == "green" ? t.bin_green : c == "brown" ? t.bin_brown : c == "blue" ? t.bin_blue
       : c == "purple" ? t.bin_purple : c == "red" ? t.bin_red : c == "yellow" ? t.bin_yellow : t.bin_grey;
}
std::string date(int day) {  // the sample's calendar: day 0 = Thu 23 Oct 2026
  int d = 23 + day, m = 10;
  if (d > 31) d -= 31, m = 11;
  return std::string(WD3[((day % 7) + 7) % 7]) + " " + std::to_string(d) + (m == 10 ? " Oct" : " Nov");
}
std::vector<Coll> sample(const std::string &s, int &hour) {
  hour = s == "today" ? 7 : s == "collected" ? 15 : 19;
  if (s == "none") return {};
  const int first = s == "tonight" || s == "two" ? 1 : s == "later" ? 6 : 0;
  std::vector<Coll> c;
  for (int w = 0; w < 4; w++) {
    std::vector<int> b{w % 2 ? 1 : 0};
    if (w % 2 == (s == "two" ? 0 : 1)) b.push_back(2);
    c.push_back({first + 7 * w, b});
  }
  return c;
}
std::string names(const std::vector<int> &b) {
  std::string s;
  for (int i : b) s += (s.empty() ? "" : " + ") + std::string(BINS[i].name);
  return s;
}

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
lv_obj_t *part(lv_obj_t *bin, int x, int y, int w, int h, int r, lv_color_t c) {
  lv_obj_t *o = box(bin);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  fw::qa::free(o, "a bin part: placed at the bin's own proportions");
  return o;
}
int bin_w(int h) { return (h * 4 / 5 + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID; }
// one bin h tall at x in parent (the "Faithful" proportions, apps/hub panel/bins/bin-art.ts geo())
lv_obj_t *bin_art(lv_obj_t *parent, int x, int h, const char *colour) {
  const fw::Theme &t = *fw::T;
  const lv_color_t c = colour_of(colour);
  const lv_color_t k = lv_color_mix(lv_color_black(), c, (lv_opa_t) lroundf(255 * fw::tokens::comp::bin_shade));
  const bool edge = !strcmp(colour, "black") || !strcmp(colour, "grey");
  const int w = bin_w(h), e = h >= fw::lay::bin_hero ? fw::stroke::edge : fw::stroke::hair;
  lv_obj_t *b = box(parent);
  lv_obj_add_flag(b, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_pos(b, x, 0);
  lv_obj_set_size(b, w, h);
  const int hw = w * 36 / 100, hh = std::max(2, h / 10), lid = std::max(3, h * 13 / 100), o = std::max(1, w * 65 / 1000);
  const int rb = std::max(2, h / 10), band = std::max(2, h * 6 / 100), sw = std::max(2, w * 95 / 1000), top = hh + lid;
  lv_obj_t *hd = part(b, (w - hw) / 2, 0, hw, hh + rb, rb, k);  // the handle (its lower half under the lid)
  lv_obj_t *body = part(b, o, top - rb, w - 2 * o, h - top + rb, rb, c);
  const int eb = edge ? e : 0;  // inside the edge
  part(b, o + eb, top, w - 2 * o - 2 * eb, band, 0, k);  // the band under the lid
  const int sh = (h - top - band) * 55 / 100, sy = top + band + (h - top - band - sh) / 2;
  for (int i = -1; i <= 1; i++) part(b, w / 2 + i * (w * 228 / 1000) - sw / 2, sy, sw, sh, sw / 2, k);
  lv_obj_t *lt = part(b, 0, hh, w, lid, std::max(1, lid / 5), k);  // the slab lid over the body's top corners
  if (edge)
    for (lv_obj_t *p : {hd, body, lt}) {
      lv_obj_set_style_border_width(p, e, 0);
      lv_obj_set_style_border_color(p, t.line_hi, 0);
    }
  return b;
}
// a collection's bins, overlapping: each next one 55 % of a bin to the right, behind
lv_obj_t *bin_group(lv_obj_t *parent, const std::vector<int> &bins, int h) {
  const int w = bin_w(h), step = (w * 55 / 100) / fw::sp::GRID * fw::sp::GRID, n = (int) bins.size();
  lv_obj_t *g = box(parent);
  lv_obj_set_size(g, w + step * std::max(0, n - 1), h);
  for (int i = n - 1; i >= 0; i--) bin_art(g, i * step, h, BINS[bins[i]].colour);
  return g;
}
lv_obj_t *section(lv_obj_t *parent, const char *title) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_margin_top(s, fw::lay::group, 0);
  ArduinoJson::JsonDocument sp;
  sp["title"] = title, sp["badge"] = false, sp["right"] = "none";
  build("section_header", s, sp);
  return s;
}
void row(lv_obj_t *parent, const std::string &title, const std::string &sub, const std::string &value) {
  ArduinoJson::JsonDocument rp;
  rp["title"] = title, rp["sub"] = sub, rp["icon"] = "none", rp["trailing"] = "value", rp["value"] = value;
  build("list_row", parent, rp);
}
std::string in_days(int d) { return d == 0 ? "today" : d == 1 ? "tomorrow" : "in " + std::to_string(d) + " days"; }

void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_clean(root);
  int hour = 0;
  const std::vector<Coll> cs = sample(p["sample"] | "tonight", hour);
  if (cs.empty()) {
    ArduinoJson::JsonDocument ep;
    ep["icon"] = "alert", ep["title"] = "No collections found", ep["body"] = "Pick a bin calendar in Settings > Screens", ep["tone"] = "idle";
    build("empty_state", root, ep);
    return;
  }
  const Coll &next = cs[0];
  const bool tonight = next.day == 1 && hour >= 12, today = next.day == 0 && hour < 12, collected = next.day == 0 && hour >= 12;
  const char *caption = tonight ? "PUT OUT TONIGHT" : today ? "COLLECTION TODAY" : collected ? "COLLECTED TODAY \xC2\xB7 BRING IT IN" : "NEXT COLLECTION";
  const std::string when = next.day == 0 ? "Today" : next.day == 1 ? "Tomorrow" : WD[next.day % 7];

  lv_obj_t *hero = box(root);
  lv_obj_set_size(hero, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(hero, fw::lay::group, 0);
  bin_group(hero, next.bins, fw::lay::bin_hero);
  lv_obj_t *col = box(hero);
  lv_obj_set_size(col, 0, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  const bool accent = tonight || today;
  lv_obj_t *cap = fw::text(col, fw::ty::label, caption, accent ? t.accent_text : t.text_dim);
  lv_obj_set_style_margin_bottom(cap, fw::sp::s2, 0);
  for (auto [ty, s, c] : {std::tuple{fw::ty::value, when, t.text}, std::tuple{fw::ty::title, names(next.bins), t.text},
                          std::tuple{fw::ty::body, date(next.day) + " \xC2\xB7 every 2 weeks", t.text_dim}}) {
    lv_obj_t *l = fw::text(col, ty, s.c_str(), c);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  }

  lv_obj_t *up = section(root, "coming up");
  for (size_t i = 1; i < cs.size(); i++) row(up, names(cs[i].bins), date(cs[i].day), in_days(cs[i].day));
  lv_obj_t *yb = section(root, "your bins");
  for (int b = 0; b < 3; b++) {
    int nd = -1;
    for (const Coll &c : cs)
      if (nd < 0 && std::find(c.bins.begin(), c.bins.end(), b) != c.bins.end()) nd = c.day;
    row(yb, BINS[b].name, "every " + std::to_string(BINS[b].every / 7) + " weeks", nd < 0 ? "" : "next " + date(nd));
  }
}
}  // namespace

FW_COMPONENT(bins_view, "Bins view", "components", R"({
  "about": "The Bins screen's body: the next collection drawn as its bins with when to put them out, then what is coming up and every bin's rhythm.",
  "frame": [480, 0],
  "props": [
    {"name": "sample", "type": "select", "options": ["tonight", "today", "collected", "later", "two", "none"], "default": "tonight"}
  ],
  "variants": [
    {"label": "put out tonight", "props": {}},
    {"label": "collection today", "props": {"sample": "today"}},
    {"label": "collected today", "props": {"sample": "collected"}},
    {"label": "later this week", "props": {"sample": "later"}},
    {"label": "two bins", "props": {"sample": "two"}},
    {"label": "no schedule", "props": {"sample": "none"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  fill(root, p);
}

FW_UPDATE(bins_view) {
  fill(root, p);
  return true;
}
