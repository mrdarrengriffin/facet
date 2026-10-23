// Sport foundation helpers (sport.h): circuits fitted to a box, team colours fitted to contrast, states, feeds,
// tennis rules, the sim runner.
#include "sport.h"
#include <cmath>
#include <cstring>

namespace fw::sport {
// ---------------------------------------------------------------- circuits
const Circuit *circuit(const char *id) {
  for (int i = 0; i < N_CIRCUITS; i++)
    if (id && !strcmp(CIRCUITS[i].id, id)) return &CIRCUITS[i];
  return nullptr;
}

Track fit(const Circuit &c, int w, int h, int pad, bool broadcast) {
  Track t;
  t.w = w, t.h = h;
  const int n = c.n;
  if (n < 3) return t;
  std::vector<StrokePt> raw(n);
  // decode (12-bit units, y down), turn for the broadcast view (y-up counter-clockwise = this matrix in y-down), fit
  const float a = broadcast ? c.rotation * 3.14159265f / 180 : 0, ca = std::cos(a), sa = std::sin(a);
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (int i = 0; i < n; i++) {
    const uint8_t *b = c.pts + i * 3;
    const float x = (float) (b[0] | ((b[1] & 0x0f) << 8)) - c.w / 2.0f, y = (float) ((b[1] >> 4) | (b[2] << 4)) - c.h / 2.0f;
    raw[i] = {x * ca + y * sa, -x * sa + y * ca};
    x0 = std::min(x0, raw[i].x), y0 = std::min(y0, raw[i].y), x1 = std::max(x1, raw[i].x), y1 = std::max(y1, raw[i].y);
  }
  const float bw = std::max(1.0f, x1 - x0), bh = std::max(1.0f, y1 - y0);
  const float s = std::min((w - 2.0f * pad) / bw, (h - 2.0f * pad) / bh);
  const float ox = (w - bw * s) / 2 - x0 * s, oy = (h - bh * s) / 2 - y0 * s;
  for (auto &p : raw) p = {ox + p.x * s, oy + p.y * s};
  // centripetal Catmull-Rom (alpha 0.5: no loops or overshoot at hairpins), sub-steps of about 3 px
  auto tj = [](float ti, StrokePt a, StrokePt b) { return ti + std::sqrt(std::max(1e-4f, std::hypot(b.x - a.x, b.y - a.y))); };
  for (int i = 0; i < n; i++) {
    const StrokePt p0 = raw[(i + n - 1) % n], p1 = raw[i], p2 = raw[(i + 1) % n], p3 = raw[(i + 2) % n];
    const float t0 = 0, t1 = tj(t0, p0, p1), t2 = tj(t1, p1, p2), t3 = tj(t2, p2, p3);
    const int steps = std::max(1, (int) std::ceil(std::hypot(p2.x - p1.x, p2.y - p1.y) / 3.0f));
    for (int k = 0; k < steps; k++) {
      const float u = t1 + (t2 - t1) * k / steps;
      auto L = [](StrokePt a, StrokePt b, float ta, float tb, float u) {
        const float d = tb - ta, wa = (tb - u) / d, wb = (u - ta) / d;
        return StrokePt{a.x * wa + b.x * wb, a.y * wa + b.y * wb};
      };
      const StrokePt A1 = L(p0, p1, t0, t1, u), A2 = L(p1, p2, t1, t2, u), A3 = L(p2, p3, t2, t3, u);
      const StrokePt B1 = L(A1, A2, t0, t2, u), B2 = L(A2, A3, t1, t3, u);
      t.pts.push_back(L(B1, B2, t1, t2, u));
    }
  }
  t.pts.push_back(t.pts.front());
  t.cum.resize(t.pts.size());
  t.cum[0] = 0;
  for (size_t i = 1; i < t.pts.size(); i++)
    t.cum[i] = t.cum[i - 1] + std::hypot(t.pts[i].x - t.pts[i - 1].x, t.pts[i].y - t.pts[i - 1].y);
  t.len = t.cum.back();
  return t;
}

static size_t seg_of(const Track &t, float frac, float *u) {
  frac -= std::floor(frac);
  const float d = frac * t.len;
  size_t lo = 0, hi = t.cum.size() - 1;
  while (hi - lo > 1) {
    const size_t m = (lo + hi) / 2;
    (t.cum[m] <= d ? lo : hi) = m;
  }
  const float sl = t.cum[hi] - t.cum[lo];
  *u = sl > 0 ? (d - t.cum[lo]) / sl : 0;
  return lo;
}
StrokePt point_at(const Track &t, float frac) {
  if (t.pts.size() < 2) return {t.w / 2.0f, t.h / 2.0f};
  float u;
  const size_t i = seg_of(t, frac, &u);
  return {t.pts[i].x + (t.pts[i + 1].x - t.pts[i].x) * u, t.pts[i].y + (t.pts[i + 1].y - t.pts[i].y) * u};
}
float heading_at(const Track &t, float frac) {
  if (t.pts.size() < 2) return 0;
  float u;
  const size_t i = seg_of(t, frac, &u);
  return std::atan2(t.pts[i + 1].y - t.pts[i].y, t.pts[i + 1].x - t.pts[i].x);
}

// ---------------------------------------------------------------- colour
lv_color_t hex(const char *s, lv_color_t fallback) {
  if (!s) return fallback;
  if (*s == '#') s++;
  if (strlen(s) != 6) return fallback;
  char *end;
  const unsigned long v = strtoul(s, &end, 16);
  return *end ? fallback : lv_color_hex((uint32_t) v);
}
static float lum(lv_color_t c) {
  const uint16_t v = lv_color_to_u16(c);  // the panel's 565 value, expanded back like the display does
  const float r = ((v >> 11) & 31) / 31.0f, g = ((v >> 5) & 63) / 63.0f, b = (v & 31) / 31.0f;
  auto lin = [](float x) { return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f); };
  return 0.2126f * lin(r) + 0.7152f * lin(g) + 0.0722f * lin(b);
}
float contrast(lv_color_t a, lv_color_t b) {
  const float la = lum(a), lb = lum(b);
  return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}
lv_color_t team_ink(lv_color_t team, lv_color_t bg, lv_color_t toward, float min) {
  for (int k = 0; k <= 16; k++) {
    const lv_color_t c = lv_color_mix(toward, team, (uint8_t) std::min(255, k * 16));
    if (contrast(c, bg) >= min) return c;
  }
  return toward;
}

// ---------------------------------------------------------------- marks
Mark mark_of(const char *s) { return s && !strcmp(s, "code") ? Mark::code : s && !strcmp(s, "number") ? Mark::number : Mark::dot; }
int mark_size(Mark m) { return m == Mark::number ? lay::sport_marker_lg : lay::sport_marker; }
// the mark IS the disc (bg, ring, lead outline); its only child = the number (number mark), flex-centred like the
// Count badge. The tag is a SIBLING in the parent (a pill: behind-colour box + caps label; a child outside the mark's
// box would not be redrawn when the mark moves), kept in the mark's user data and deleted with it.
static lv_obj_t *tag_of(lv_obj_t *mk) { return (lv_obj_t *) lv_obj_get_user_data(mk); }
static void drop_tag(lv_event_t *e) {
  lv_obj_t *tag = (lv_obj_t *) lv_event_get_user_data(e);
  if (tag && lv_obj_is_valid(tag)) lv_obj_delete(tag);
}
static lv_obj_t *plain(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return o;
}
lv_obj_t *marker(lv_obj_t *parent, const MarkLook &l) {
  const int d = mark_size(l.mark);
  lv_obj_t *mk = plain(parent);
  lv_obj_set_size(mk, d, d);
  lv_obj_set_style_radius(mk, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(mk, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(mk, stroke::edge, 0);
  lv_obj_set_style_outline_width(mk, stroke::edge, 0);
  lv_obj_set_style_outline_pad(mk, 0, 0);
  if (l.mark == Mark::number) {
    lv_obj_add_flag(mk, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // two digits may reach into the ring
    fw::text(mk, ty::c_meta_sm, "", T->text);
  }
  lv_obj_t *tag = plain(parent);
  lv_obj_set_size(tag, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(tag, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(tag, sp::s1, 0);
  lv_obj_set_style_radius(tag, rad::chip, 0);
  fw::text(tag, ty::c_caps_sm, "", T->text);
  fw::qa::free(mk, "placed by value (a lap fraction, a formation place)");
  lv_obj_set_user_data(mk, tag);
  lv_obj_add_event_cb(mk, drop_tag, LV_EVENT_DELETE, tag);
  fw::qa::free(tag, "beside its mark, placed by value");
  marker_set(mk, l);
  return mk;
}
// a disc's fill + ring cross-fade (pit in / out, a restyle from an update): one small anim on the mark
struct DiscFade { lv_obj_t *mk; lv_color_t bg0, bd0, bg1, bd1; };
static void disc_fade_cb(lv_anim_t *a, int32_t x) {
  const DiscFade *f = (const DiscFade *) lv_anim_get_user_data(a);
  lv_obj_t *mk = f->mk;
  lv_obj_set_style_bg_color(mk, lv_color_mix(f->bg1, f->bg0, (uint8_t) x), 0);
  lv_obj_set_style_border_color(mk, lv_color_mix(f->bd1, f->bd0, (uint8_t) x), 0);
}
static void disc_fade_free(lv_anim_t *a) { delete (DiscFade *) lv_anim_get_user_data(a); }
void marker_set(lv_obj_t *mk, const MarkLook &l, bool fade) {
  const lv_color_t bg0 = lv_obj_get_style_bg_color(mk, LV_PART_MAIN), bd0 = lv_obj_get_style_border_color(mk, LV_PART_MAIN);
  lv_anim_delete(mk, nullptr);
  lv_obj_t *tag = tag_of(mk);
  const lv_color_t team = team_ink(l.team, l.behind, T->text);
  if (l.mark == Mark::number) {
    lv_obj_set_style_bg_color(mk, l.behind, 0);
    lv_obj_set_style_border_color(mk, team, 0);
    char b[8];
    snprintf(b, sizeof b, "%d", l.num);
    fw::set_text(lv_obj_get_child(mk, 0), ty::c_meta_sm, b);
  } else {
    lv_obj_set_style_bg_color(mk, l.pit ? l.behind : team, 0);
    lv_obj_set_style_border_color(mk, l.pit ? team : l.behind, 0);
  }
  lv_obj_set_style_outline_color(mk, T->text, 0);
  lv_obj_set_style_outline_opa(mk, l.lead ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_t *tl = lv_obj_get_child(tag, 0);
  const std::string txt = fw::cased(ty::c_caps_sm, l.code ? l.code : "");
  if (strcmp(lv_label_get_text(tl), txt.c_str())) {
    fw::set_text(tl, ty::c_caps_sm, l.code ? l.code : "");
    // the tag's size, measured once per text (marker_place runs every frame: no layout pass there)
    lv_point_t sz;
    lv_text_get_size(&sz, txt.c_str(), fw::font(ty::c_caps_sm), lv_obj_get_style_text_letter_space(tl, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int tw = (int) sz.x + 2 * sp::s1, th = fw::style(ty::c_caps_sm).box;
    lv_obj_set_user_data(tag, (void *) (intptr_t) ((th << 16) | (tw & 0xffff)));
  }
  lv_obj_set_style_bg_color(tag, l.behind, 0);
  const bool show_tag = (l.mark == Mark::code || l.label) && l.code && *l.code && !l.out;
  if (show_tag) lv_obj_remove_flag(tag, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
  if (l.out) lv_obj_add_flag(mk, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(mk, LV_OBJ_FLAG_HIDDEN);
  const lv_color_t bg1 = lv_obj_get_style_bg_color(mk, LV_PART_MAIN), bd1 = lv_obj_get_style_border_color(mk, LV_PART_MAIN);
  if (fade && !mo::reduced(mk) && (!lv_color_eq(bg0, bg1) || !lv_color_eq(bd0, bd1))) {
    lv_obj_set_style_bg_color(mk, bg0, 0), lv_obj_set_style_border_color(mk, bd0, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, mk);
    lv_anim_set_user_data(&a, new DiscFade{mk, bg0, bd0, bg1, bd1});
    lv_anim_set_custom_exec_cb(&a, disc_fade_cb);
    lv_anim_set_deleted_cb(&a, disc_fade_free);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, mo::ms::fade);
    lv_anim_start(&a);
  }
}
lv_obj_t *marker_tag(lv_obj_t *mk) { return tag_of(mk); }
MarkInk mark_ink(const MarkLook &l) {
  MarkInk k;
  const lv_color_t team = team_ink(l.team, l.behind, T->text);
  if (l.mark == Mark::number) k.fill = l.behind, k.ring = team;
  else k.fill = l.pit ? l.behind : team, k.ring = l.pit ? team : l.behind;
  k.tag = (l.mark == Mark::code || l.label) && l.code && *l.code && !l.out;
  lv_point_t sz{0, 0};
  const std::string txt = fw::cased(ty::c_caps_sm, l.code ? l.code : "");
  lv_text_get_size(&sz, txt.c_str(), fw::font(ty::c_caps_sm), fw::style(ty::c_caps_sm).letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  k.tag_w = (int) sz.x + 2 * sp::s1, k.tag_h = fw::style(ty::c_caps_sm).box;
  return k;
}
void mark_boxes(const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, lv_area_t *disc, lv_area_t *tag) {
  const int d = mark_size(l.mark);
  const int x = (int) std::lround(cx - d / 2.0f), y = (int) std::lround(cy - d / 2.0f);
  *disc = {x, y, x + d - 1, y + d - 1};
  const bool left = x + d + sp::s1 + k.tag_w > box_w;
  const int tx = left ? x - sp::s1 - k.tag_w : x + d + sp::s1, ty0 = y + (d - k.tag_h) / 2;
  *tag = {tx, ty0, tx + k.tag_w - 1, ty0 + k.tag_h - 1};
}
lv_area_t mark_extent(const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, bool with_tag) {
  lv_area_t d, t;
  mark_boxes(l, k, cx, cy, box_w, &d, &t);
  const int o = stroke::edge + 1;  // the lead ring + the AA fringe
  lv_area_t a = {d.x1 - o, d.y1 - o, d.x2 + o, d.y2 + o};
  if (with_tag && k.tag) a = {std::min(a.x1, t.x1), std::min(a.y1, t.y1), std::max(a.x2, t.x2), std::max(a.y2, t.y2)};
  return a;
}
void draw_mark(lv_layer_t *layer, const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, int parts, lv_point_t at) {
  if (l.out) return;
  lv_area_t d, t;
  mark_boxes(l, k, cx, cy, box_w, &d, &t);
  lv_area_move(&d, at.x, at.y), lv_area_move(&t, at.x, at.y);
  if (parts & 1) {
  lv_draw_rect_dsc_t r;
  lv_draw_rect_dsc_init(&r);
  r.radius = LV_RADIUS_CIRCLE, r.bg_color = k.fill, r.bg_opa = LV_OPA_COVER;
  r.border_color = k.ring, r.border_width = stroke::edge, r.border_opa = LV_OPA_COVER;
  if (l.lead) r.outline_color = T->text, r.outline_width = stroke::edge, r.outline_pad = 0, r.outline_opa = LV_OPA_COVER;
  lv_draw_rect(layer, &r, &d);
  if (l.mark == Mark::number) {
    char b[8];
    snprintf(b, sizeof b, "%d", l.num);
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    ld.font = fw::font(ty::c_meta_sm), ld.color = T->text, ld.align = LV_TEXT_ALIGN_CENTER, ld.text = b;
    const int lh = lv_font_get_line_height(ld.font);
    lv_area_t la = {d.x1 - sp::s2, (d.y1 + d.y2 + 1 - lh) / 2, d.x2 + sp::s2, (d.y1 + d.y2 + 1 - lh) / 2 + lh - 1};
    ld.text_local = 1;
    lv_draw_label(layer, &ld, &la);
  }
  }
  if ((parts & 2) && k.tag) {
    lv_draw_rect_dsc_t p;
    lv_draw_rect_dsc_init(&p);
    p.radius = rad::chip, p.bg_color = l.behind, p.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &p, &t);
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    const std::string txt = fw::cased(ty::c_caps_sm, l.code);
    ld.font = fw::font(ty::c_caps_sm), ld.color = T->text, ld.align = LV_TEXT_ALIGN_CENTER, ld.text = txt.c_str();
    ld.letter_space = fw::style(ty::c_caps_sm).letter_space, ld.text_local = 1;
    const int lh = lv_font_get_line_height(ld.font);
    lv_area_t la = {t.x1, (t.y1 + t.y2 + 1 - lh) / 2, t.x2, (t.y1 + t.y2 + 1 - lh) / 2 + lh - 1};
    lv_draw_label(layer, &ld, &la);
  }
}
void marker_place(lv_obj_t *mk, float cx, float cy, int box_w) {
  const int d = lv_obj_get_style_width(mk, LV_PART_MAIN);  // the set size (the layout may not have run yet)
  const int x = (int) std::lround(cx - d / 2.0f), y = (int) std::lround(cy - d / 2.0f);
  lv_obj_set_pos(mk, x, y);
  lv_obj_t *tag = tag_of(mk);
  if (lv_obj_has_flag(tag, LV_OBJ_FLAG_HIDDEN)) return;
  const intptr_t wh = (intptr_t) lv_obj_get_user_data(tag);
  const int tw = (int) (wh & 0xffff), th = (int) (wh >> 16);
  const bool left = x + d + sp::s1 + tw > box_w;
  lv_obj_set_pos(tag, left ? x - sp::s1 - tw : x + d + sp::s1, y + (d - th) / 2);
}

// a label's own translate_y is its text-box offset (fw::text_style): the lift moves relative to it. The base is kept
// in the label's user data (a plain label of ours, never an atom's object) as base + 1000.
static int base_of(lv_obj_t *o) {
  intptr_t u = (intptr_t) lv_obj_get_user_data(o);
  if (!u) u = lv_obj_get_style_translate_y(o, LV_PART_MAIN) + 1000, lv_obj_set_user_data(o, (void *) u);
  return (int) u - 1000;
}
static void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *) o, base_of((lv_obj_t *) o) + v, 0); }
static void set_op(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void lift_in(lv_obj_t *o) {
  base_of(o);
  fw::bleed(o, sp::s2);  // it rises from a step below its box: let that step draw (and redraw) on every ancestor
  lv_anim_delete(o, set_ty);
  lv_anim_delete(o, set_op);
  if (mo::reduced(o)) { set_ty(o, 0), set_op(o, LV_OPA_COVER); return; }
  for (int k = 0; k < 2; k++) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, k ? set_op : set_ty);
    lv_anim_set_values(&a, k ? 0 : sp::s2, k ? LV_OPA_COVER : 0);
    lv_anim_set_duration(&a, mo::ms::sport_flash);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
  }
}

// ---------------------------------------------------------------- states
State state_of(const char *s, bool stale) {
  if (stale) return State::stale;
  if (!s) return State::upcoming;
  if (!strcmp(s, "live")) return State::live;
  if (!strcmp(s, "break") || !strcmp(s, "half_time") || !strcmp(s, "paused")) return State::brk;
  if (!strcmp(s, "final")) return State::final;
  if (!strcmp(s, "delayed") || !strcmp(s, "suspended")) return State::delayed;
  return State::upcoming;
}
const char *state_word(State s) {
  static const char *W[] = {"Upcoming", "Live", "Break", "Final", "Delayed", "Stale"};
  return W[(int) s];
}
const char *state_tone(State s) {
  static const char *T[] = {"idle", "ok", "info", "idle", "warn", "warn"};
  return T[(int) s];
}

// ---------------------------------------------------------------- feed
ArduinoJson::JsonVariantConst feed_of(Props p, ArduinoJson::JsonDocument &doc) {
  ArduinoJson::JsonVariantConst f = p["feed"];
  if (f.is<ArduinoJson::JsonObjectConst>()) return f;
  const char *s = f | "";
  if (*s && !deserializeJson(doc, s) && doc.is<ArduinoJson::JsonObject>()) return doc.as<ArduinoJson::JsonVariantConst>();
  return ArduinoJson::JsonVariantConst();
}

// ---------------------------------------------------------------- tennis
TennisFormat tennis_format(ArduinoJson::JsonVariantConst feed) {
  TennisFormat f;
  f.best_of = feed["format"]["best_of"] | 3;
  if (f.best_of != 5) f.best_of = 3;
  const char *fs = feed["format"]["final_set"] | "tb10";
  f.final_set = !strcmp(fs, "adv") ? 'A' : !strcmp(fs, "tb7") ? '7' : 'T';
  return f;
}
bool tennis_tiebreak_at(int ga, int gb, bool final_set, char rule, int *to) {
  if (ga != 6 || gb != 6) return false;
  if (final_set && rule == 'A') return false;
  if (to) *to = final_set && rule == 'T' ? 10 : 7;
  return true;
}
bool tennis_game_won(int a, int b, bool tiebreak, int tb_to) { return a >= (tiebreak ? tb_to : 4) && a - b >= 2; }
bool tennis_set_won(int ga, int gb, bool final_set, char rule) {
  if (ga == 7 && gb == 6 && !(final_set && rule == 'A')) return true;  // won the tiebreak
  return ga >= 6 && ga - gb >= 2;
}
TennisNow tennis_now(ArduinoJson::JsonVariantConst feed) {
  TennisNow n;
  const TennisFormat f = tennis_format(feed);
  ArduinoJson::JsonArrayConst sets = feed["sets"];
  const int ns = (int) sets.size();
  const bool done = feed["winner"].is<int>();
  for (int i = 0; i < ns; i++) {
    const int a = sets[i][0] | 0, b = sets[i][1] | 0;
    const bool last = i == ns - 1, fin = i == f.best_of - 1;
    if (!last || done) {
      if (tennis_set_won(a, b, fin, f.final_set)) n.sets_won[0]++;
      else if (tennis_set_won(b, a, fin, f.final_set)) n.sets_won[1]++;
    }
  }
  n.set = std::max(0, ns - 1);
  n.games[0] = ns ? (sets[ns - 1][0] | 0) : 0, n.games[1] = ns ? (sets[ns - 1][1] | 0) : 0;
  const bool fin = n.set == f.best_of - 1;
  n.tiebreak = tennis_tiebreak_at(n.games[0], n.games[1], fin, f.final_set, &n.tb_to);
  n.server = feed["server"] | -1;
  n.winner = done ? (feed["winner"] | -1) : -1;
  ArduinoJson::JsonArrayConst pts = feed["points"];
  if (pts.size() == 2 && !done) {
    const int a = pts[0] | 0, b = pts[1] | 0;
    n.points[0] = a, n.points[1] = b;
    if (n.tiebreak) {
      snprintf(n.label[0], 4, "%d", a), snprintf(n.label[1], 4, "%d", b);
    } else {
      static const char *L[] = {"0", "15", "30", "40"};
      if (a >= 3 && b >= 3) {
        strcpy(n.label[0], a > b ? "AD" : "40"), strcpy(n.label[1], b > a ? "AD" : "40");
        if (a == b) strcpy(n.label[0], "40"), strcpy(n.label[1], "40");
      } else {
        strcpy(n.label[0], L[std::min(a, 3)]), strcpy(n.label[1], L[std::min(b, 3)]);
      }
    }
    // a game point: one more point wins the game (or the tiebreak) for that player
    for (int p = 0; p < 2; p++) {
      const int me = n.points[p] + 1, them = n.points[1 - p];
      if (!tennis_game_won(me, them, n.tiebreak, n.tb_to)) continue;
      n.game_point = p;
      n.break_point = !n.tiebreak && n.server >= 0 && n.server != p;
      int g[2] = {n.games[0], n.games[1]};
      g[p]++;
      if (tennis_set_won(g[p], g[1 - p], fin, f.final_set)) {
        n.set_point = true;
        n.match_point = n.sets_won[p] + 1 > f.best_of / 2;
      }
    }
  }
  return n;
}

// ---------------------------------------------------------------- sim runner
namespace {
struct Run {
  std::string key;
  SimBase *sim = nullptr;
  float t = 0, speed = 1;
  std::string jump;
  Apply apply = nullptr;
  lv_timer_t *timer = nullptr;
  lv_obj_t *root = nullptr;
};
Run *run_of(lv_obj_t *root) { return (Run *) lv_obj_get_user_data(root); }
void tick(lv_timer_t *tm) {
  Run *r = (Run *) lv_timer_get_user_data(tm);
  r->t = std::min(r->sim->duration() + 30.0f, r->t + SIM_TICK_MS / 1000.0f * r->speed);
  ArduinoJson::JsonDocument d;
  r->sim->feed(d, r->t);
  fw::Quiet q;
  r->apply(r->root, d.as<ArduinoJson::JsonVariantConst>());
}
void free_run(Run *r) {
  if (!r) return;
  if (r->timer) lv_timer_delete(r->timer);
  delete r->sim;
  delete r;
}
void on_delete(lv_event_t *e) {
  lv_obj_t *root = (lv_obj_t *) lv_event_get_target(e);
  free_run(run_of(root));
  lv_obj_set_user_data(root, nullptr);
}
}  // namespace

void sim_run(lv_obj_t *root, const std::string &key, SimBase *(*make)(void *), void *arg, float speed,
             const char *jump, Apply apply, ArduinoJson::JsonDocument &first) {
  Run *r = run_of(root);
  if (r && r->key != key) {
    free_run(r);
    lv_obj_set_user_data(root, nullptr);
    r = nullptr;
  }
  if (!r) {
    r = new Run;
    r->key = key, r->root = root, r->sim = make(arg);
    lv_obj_set_user_data(root, r);
    bool has = false;  // once per root (user data and callbacks survive refill)
    for (uint32_t i = 0; i < lv_obj_get_event_count(root); i++)
      has = has || lv_event_dsc_get_cb(lv_obj_get_event_dsc(root, i)) == on_delete;
    if (!has) lv_obj_add_event_cb(root, on_delete, LV_EVENT_DELETE, nullptr);
    r->timer = lv_timer_create(tick, SIM_TICK_MS, r);
    r->jump = "\x01";  // nothing yet: the first call always places the sim (at "start" when no jump is given)
  }
  r->apply = apply, r->speed = speed;
  const std::string j = jump ? jump : "";
  if (j != r->jump) {  // a new jump target: go there (none = back to the start)
    r->jump = j;
    // "start" (or none) = each sim's own start: just before the action (a demo moves at once); "formation" etc. by name
    const float m = r->sim->moment(j.empty() ? "start" : j.c_str());
    if (m >= 0) r->t = m;
  }
  r->sim->feed(first, r->t);
}
void sim_stop(lv_obj_t *root) {
  Run *r = run_of(root);
  if (!r) return;
  if (r->timer) lv_timer_delete(r->timer);
  r->timer = nullptr;
  delete r->sim;
  r->sim = nullptr;
  r->key.clear();
  lv_obj_set_user_data(root, nullptr);
  delete r;
}
bool sim_on(lv_obj_t *root) { return run_of(root) != nullptr; }

namespace {
struct Args { uint32_t seed; std::string track; int cars, best_of; char final_set; };
Args g_args;  // read synchronously by make() inside sim_run
SimBase *mk_race(void *) { return make_race(g_args.seed, g_args.track.c_str(), g_args.cars); }
SimBase *mk_tennis(void *) { return make_tennis(g_args.seed, g_args.best_of, g_args.final_set); }
SimBase *mk_match(void *) { return make_match(g_args.seed); }
float speed_of(Props p) {
  const char *s = p["speed"] | "1x";
  return !strcmp(s, "60x") ? 60 : !strcmp(s, "10x") ? 10 : 1;
}
ArduinoJson::JsonVariantConst any_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply, char kind) {
  ArduinoJson::JsonVariantConst f = feed_of(p, doc);
  if (!f.isNull() || !(p["sim"] | true)) { sim_stop(root); return f; }
  g_args.seed = p["seed"] | 7, g_args.track = p["track"] | "us-2012", g_args.cars = p["cars"] | 22;
  g_args.best_of = p["best_of"] | 3;
  const char *fs = p["final_set"] | "tb10";
  g_args.final_set = !strcmp(fs, "adv") ? 'A' : !strcmp(fs, "tb7") ? '7' : 'T';
  char key[96];
  snprintf(key, sizeof key, "%c|%s|%d|%d|%c|%u", kind, kind == 'r' ? g_args.track.c_str() : "", kind == 'r' ? g_args.cars : 0,
           kind == 't' ? g_args.best_of : 0, kind == 't' ? g_args.final_set : '-', (unsigned) g_args.seed);
  sim_run(root, key, kind == 'r' ? mk_race : kind == 't' ? mk_tennis : mk_match, nullptr, speed_of(p), p["jump"] | "", apply, doc);
  return doc.as<ArduinoJson::JsonVariantConst>();
}
}  // namespace
ArduinoJson::JsonVariantConst f1_race_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply) { return any_feed(root, p, doc, apply, 'r'); }
ArduinoJson::JsonVariantConst tennis_match_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply) { return any_feed(root, p, doc, apply, 't'); }
ArduinoJson::JsonVariantConst football_match_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply) { return any_feed(root, p, doc, apply, 'm'); }
}  // namespace fw::sport
