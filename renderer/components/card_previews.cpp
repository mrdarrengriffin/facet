// Card previews: the artwork at the top right of a Home card (core/previews.h has the list and the rules; docs
// Components > Graphics > Card previews, @facet/ui previews.ts). This file is the implementation (fw::card_preview,
// used by the Card's "preview" prop too) and its workbench page: one preview at the size it has on a card, every kind a
// variant. Contrast: tokens/colour.json section "card_preview" (+ "radar", "media_visual", "mascot", "chart").
#include <cstring>
#include "chart.h"
#include "fw.h"
#include "lightning.h"
#include "previews.h"

#ifndef ESP_PLATFORM
namespace fw::qa { void legacy(lv_obj_t *o, const char *why); }  // core/fw_core.cpp: the pixel contrast proof
#endif
namespace fw {
namespace {
int S() { return ic::PX[ic::card]; }
// a docs length on the 72 px box, scaled to this box and put on the 4 px grid (minimum 4)
int g(float v72) { const int v = (int) std::lround(v72 * S() / 72.0f / 4) * 4; return std::max(4, v); }

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *circle(lv_obj_t *parent, int x, int y, int d) {
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, d, d);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  return o;
}
lv_obj_t *centred(lv_obj_t *parent, int d) { return circle(parent, (S() - d) / 2, (S() - d) / 2, d); }
void fill(lv_obj_t *o, lv_color_t c) { lv_obj_set_style_bg_color(o, c, 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); }
void ring(lv_obj_t *o, int w, lv_color_t c) { lv_obj_set_style_border_width(o, w, 0); lv_obj_set_style_border_color(o, c, 0); }
// a dot of diameter d centred r px from the box centre at `deg` (LVGL degrees: 0 = 3 o'clock, clockwise)
lv_obj_t *dot_at(lv_obj_t *parent, float r, float deg, int d, lv_color_t c, const char *why) {
  const StrokePt p = polar(S() / 2.0f, S() / 2.0f, r, deg);
  lv_obj_t *o = circle(parent, (int) std::lround(p.x - d / 2.0f), (int) std::lround(p.y - d / 2.0f), d);
  fill(o, c);
  qa::free(o, why);
  return o;
}

// ---- dial / timer: a value arc + the blue hub
struct ArcPV { lv_obj_t *arc; int32_t shown; };
void arc_show(void *o, int32_t v) {
  auto *root = (lv_obj_t *) o;
  auto *a = (ArcPV *) lv_obj_get_user_data(root);
  a->shown = v;
  lv_arc_set_value(a->arc, v);
}
void arc_pv(lv_obj_t *root, bool timer, int value) {
  const Theme &t = *T;
  ArcLook l;
  l.start = timer ? 270.0f : 135.0f;
  l.range = timer ? 360.0f : 270.0f;
  l.track_w = stroke::arc;
  l.track_c = t.line;
  l.ind_w = stroke::indicator;
  l.ind_c = t.accent;
  l.steps = 100;
  auto *a = new ArcPV{value_arc(root, S() - g(4), l), value};
  lv_obj_center(a->arc);
  lv_arc_set_value(a->arc, value);
  lv_obj_set_user_data(root, a);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (ArcPV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, a);
  fill(centred(root, lay::preview_home), t.blue);
}

// ---- switch: pill + knob; v 0 (off) .. 255 (on) mixes the looks and slides the knob
// geometry (docs previewSwitch: a 68 x 36 pill at 2, 18 and a 24 knob 6 in from its ends, on the 72 box; scaled, the
// pill centred): kept here, never read back from the objects (their coords are not laid out yet at build: the first
// version read 0 and put the knob half below the pill, Darren 2026-10-22 "switch on / off broken")
struct SwPV { lv_obj_t *pill, *knob; int32_t shown; bool avail; int x0, x1; };
void sw_show(void *o, int32_t v) {
  auto *root = (lv_obj_t *) o;
  auto *s = (SwPV *) lv_obj_get_user_data(root);
  const Theme &t = *T;
  s->shown = v;
  lv_obj_set_x(s->knob, s->x0 + (s->x1 - s->x0) * v / 255);
  // unavailable: an empty line outline, no knob, whatever the value (docs: dim, no knob)
  lv_obj_set_style_bg_opa(s->pill, s->avail ? (lv_opa_t) v : LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(s->pill, s->avail ? lv_color_mix(t.accent, t.line_hi, (uint8_t) v) : t.line, 0);
  lv_obj_set_style_bg_color(s->knob, lv_color_mix(t.on_accent, t.steel, (uint8_t) v), 0);
  lv_obj_set_style_opa(s->knob, s->avail ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}
int sw_target(Props p, bool *avail) {
  *avail = !(p["unavailable"] | false);
  return (p["on"] | true) ? 255 : 0;
}

// ---- energy: bars by price band
lv_color_t band(const Theme &t, const std::string &b) { return b == "cheap" ? t.blue : b == "peak" ? t.accent : t.band_normal; }
// 8 px bars sp::s1 apart (coordinator for Darren 2026-10-22: chunkier than 4 px, closer to the docs' filled look):
// 6 at 72, 4 at 48 (the last 3 / 2 hours)
int n_bars() { return std::min(12, (S() + sp::s1) / (2 * sp::s1 + sp::s1)); }
void bar_data(Props p, std::vector<float> *v, std::vector<lv_color_t> *c) {
  const std::vector<float> all = chart::values(p, "values", "0.6,0.4,0.3,0.3,0.5,0.8,1.2,1.6,1.9,1.4,0.9,0.7");
  const std::vector<std::string> bands = chart::words(p, "bands", "normal,cheap,cheap,cheap,normal,normal,normal,peak,peak,peak,normal,normal");
  const int n = std::min((int) all.size(), n_bars()), from = (int) all.size() - n;  // the last n half-hours
  for (int i = from; i < (int) all.size(); i++) {
    v->push_back(all[i]);
    c->push_back(band(*T, i < (int) bands.size() ? bands[i] : "normal"));
  }
}

// ---- lightning: rings, home, strikes of the last 15 minutes (one draw event)
struct LxPV { fw::lx::Data d; double now; lv_obj_t *dots; };
double lx_now(Props p) { const double f = p["now"] | 0.0; return f > 0 ? f : (double) time(nullptr) * 1000; }
void lx_draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  const LxPV &x = *(const LxPV *) lv_event_get_user_data(e);
  const Theme &t = *T;
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const float C = S() / 2.0f, D = (float) lay::preview_strike, R = C - D;
  int n = 0;
  for (size_t i = x.d.strikes.size(); i-- > 0;) {  // oldest first
    const fw::lx::Strike &s = x.d.strikes[i];
    if (x.now - s.t >= 15 * 60000.0 || ++n > 24) continue;
    const float r = R * std::sqrt(std::min(std::max(s.d, 0.0f), (float) x.d.radius) / x.d.radius), an = s.az * 3.14159265f / 180;
    lv_area_t q;
    q.x1 = a.x1 + (int) lroundf(C + r * sinf(an) - D / 2);
    q.y1 = a.y1 + (int) lroundf(C - r * cosf(an) - D / 2);
    q.x2 = q.x1 + (int) D - 1;
    q.y2 = q.y1 + (int) D - 1;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = t.accent;
    if (fw::lx::age_class(s.t, x.now) == 2) dsc.bg_opa = (lv_opa_t) lroundf(255 * tokens::comp::radar_faded);
    lv_draw_rect(lv_event_get_layer(e), &dsc, &q);
  }
}

void build_kind(lv_obj_t *root, const char *kind, Props p) {
  const Theme &t = *T;
  const int s = S();
  if (!strcmp(kind, "agents")) {
    const mas::Mood moods[4] = {mas::working, mas::done, mas::needs, mas::working};
    for (int i = 0; i < 4; i++) {
      lv_obj_t *m = mascot(root, moods[i]);
      const int w = mas::G.body_w, h = mas::G.body_h;  // the body box (its size is set, not laid out yet)
      // rows: the lower one leaves room under it for the working dots (G.dots, inside the box at both sizes: at 480 the
      // centred rows put them on y 48 of a 48 box, audit "off the stage"); docs 0 / 40 x 8 / 40
      const int down = mas::G.dots.y + mas::G.dots.size - h, y1 = (s - h - down) / 4 * 4, y0 = y1 - sp::s1 - h;
      lv_obj_set_pos(m, i % 2 ? s - w : 0, i / 2 ? y1 : std::max(0, y0));
    }
  } else if (!strcmp(kind, "media")) {
    lv_obj_t *face = centred(root, g(68));
    fill(face, t.disc);
    ring(face, stroke::hair, t.line_hi);
    ring(centred(root, g(44)), stroke::hair, t.groove);
    fill(centred(root, g(22)), t.accent);
    fill(centred(root, g(4)), t.bg);
    dot_at(root, (g(68) + g(44)) / 4.0f, 315, g(8), t.blue, "the record's marker: placed by angle");
  } else if (!strcmp(kind, "dial") || !strcmp(kind, "timer")) {
    arc_pv(root, kind[0] == 't', std::clamp((int) (p["value"] | (kind[0] == 't' ? 65 : 64)), 0, 100));
  } else if (!strcmp(kind, "switch")) {
    auto *sw = new SwPV{};
    const int pw = g(68), ph = s / 2, kd = g(24), px = (s - pw) / 2, pad = (ph - kd) / 2;
    sw->pill = box(root);
    lv_obj_set_size(sw->pill, pw, ph);
    lv_obj_set_pos(sw->pill, px, (s - ph) / 2);  // centred
    lv_obj_set_style_radius(sw->pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw->pill, t.accent, 0);
    lv_obj_set_style_border_width(sw->pill, stroke::edge, 0);
    sw->knob = circle(root, px + pad, (s - kd) / 2, kd);  // centred in the pill's height
    lv_obj_set_style_bg_opa(sw->knob, LV_OPA_COVER, 0);
    qa::free(sw->knob, "the switch knob: placed by its value along the pill (docs 6 px in from the ends)");
    sw->x0 = px + pad;
    sw->x1 = px + pw - pad - kd;
    lv_obj_update_layout(root);
    lv_obj_set_user_data(root, sw);
    lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (SwPV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, sw);
    sw_show(root, sw_target(p, &sw->avail));
  } else if (!strcmp(kind, "clock")) {
    const int dot = g(14), outer = s - dot, inner = g(46);
    ring(centred(root, outer), stroke::hair, t.line_hi);
    ring(centred(root, inner), stroke::hair, t.groove);
    fill(circle(root, (s - dot) / 2, 0, dot), t.accent);  // on the outer ring at 12
    dot_at(root, inner / 2.0f, 20, g(8), t.blue, "an orbit dot: placed by angle");
    dot_at(root, inner / 2.0f, 125, g(6), t.steel, "an orbit dot: placed by angle");
  } else if (!strcmp(kind, "github")) {
    const int d = g(20), in = g(4), w = stroke::edge;
    ring(circle(root, in, in, d), w, t.steel);
    ring(circle(root, in, s - in - d, d), w, t.steel);
    lv_obj_t *stem = box(root);
    lv_obj_set_size(stem, w, s - 2 * in - 2 * d);
    lv_obj_set_pos(stem, in + d / 2 - w / 2, in + d);
    fill(stem, t.steel);
    // the branch: from the top commit's right, across, then a quarter turn down into the merge dot
    const float y0 = in + d / 2.0f, x1 = s - in - d / 2.0f, r = d / 2.0f, x0 = in + d + w;
    std::vector<StrokePt> pts{{x0, y0}, {x1 - r, y0}};
    for (int k = 1; k <= 8; k++) {
      const float an = k * 3.14159265f / 16;
      pts.push_back({x1 - r + r * sinf(an), y0 + r - r * cosf(an)});
    }
    pts.push_back({x1, (float) (s - in - d)});
    lv_obj_t *br = stroke_at(root, pts.data(), (int) pts.size(), w, t.blue);
    qa::free(br, "the branch stroke: a mask sized to the path");
    fill(circle(root, s - in - d, s - in - d, d), t.accent);
  } else if (!strcmp(kind, "energy")) {
    chart::BarOpts o;
    bar_data(p, &o.v, &o.col);
    o.w = s;
    o.h = s - g(8);
    o.gap = sp::s1;
    o.scrub = o.readout = false;
    chart::Bars *B = chart::bars(root, t, o);
    lv_obj_update_layout(B->root);  // the chart hugs its bars: its width is known after layout
    lv_obj_set_pos(B->root, (s - lv_obj_get_width(B->root)) / 2 / 4 * 4, g(8));
    lv_obj_set_user_data(root, B);
  } else if (!strcmp(kind, "lightning")) {
    ring(centred(root, s - g(4)), stroke::hair, t.line_hi);
    ring(centred(root, g(36)), stroke::hair, t.groove);
    fill(centred(root, lay::preview_home), t.blue);
    auto *x = new LxPV{fw::lx::sample(lx_now(p), p["quiet"] | false), lx_now(p), box(root)};
    lv_obj_set_size(x->dots, s, s);
    lv_obj_add_event_cb(x->dots, lx_draw, LV_EVENT_DRAW_MAIN, x);
    lv_obj_set_user_data(root, x);
    lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (LxPV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, x);
  }
}
}  // namespace

bool is_preview(const char *kind) {
  for (const char *k : PREVIEW_KINDS) if (kind && !strcmp(k, kind)) return true;
  return false;
}
// root (user data = the kind's index + 1) > body (the kind's parts; its user data = the kind's state)
int kind_index(const char *kind) {
  for (int i = 0; i < N_PREVIEW_KINDS; i++) if (kind && !strcmp(PREVIEW_KINDS[i], kind)) return i;
  return -1;
}
lv_obj_t *card_preview(lv_obj_t *parent, const char *kind, Props p) {
  lv_obj_t *root = box(parent);
  lv_obj_set_size(root, S(), S());
  lv_obj_set_user_data(root, (void *) (intptr_t) (kind_index(kind) + 1));
#ifndef ESP_PLATFORM
  fw::qa::legacy(root, "the card preview artwork is not on the contract yet (its own piece)");
#endif
  lv_obj_t *body = box(root);
  lv_obj_set_size(body, S(), S());
  build_kind(body, kind, p);
  return root;
}
bool card_preview_set(lv_obj_t *preview, const char *kind, Props p) {
  if ((intptr_t) lv_obj_get_user_data(preview) != kind_index(kind) + 1) return false;  // another kind: rebuild
  lv_obj_t *root = lv_obj_get_child(preview, 0);
  if (!root) return false;
  if (!strcmp(kind, "dial") || !strcmp(kind, "timer")) {
    auto *a = (ArcPV *) lv_obj_get_user_data(root);
    if (!a) return false;
    const int to = std::clamp((int) (p["value"] | (kind[0] == 't' ? 65 : 64)), 0, 100);
    if (to != a->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) arc_show)) animate(root, st::SWEEP, {arc_show, a->shown, to, mo::VALUE});
  } else if (!strcmp(kind, "switch")) {
    auto *s = (SwPV *) lv_obj_get_user_data(root);
    if (!s) return false;
    bool avail;
    const int to = sw_target(p, &avail);
    if (avail != s->avail) { s->avail = avail; sw_show(root, s->shown); }
    if (to != s->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) sw_show)) animate(root, st::SWITCH, {sw_show, s->shown, to, mo::MOVE});
  } else if (!strcmp(kind, "energy")) {
    auto *B = (chart::Bars *) lv_obj_get_user_data(root);
    std::vector<float> v;
    std::vector<lv_color_t> c;
    bar_data(p, &v, &c);
    if (!B || (int) v.size() != B->n || c.size() != B->col.size()) return false;
    for (size_t i = 0; i < c.size(); i++) if (!lv_color_eq(c[i], B->col[i])) return false;  // a band changed: rebuild
    chart::bars_play(B, &v);
  } else if (!strcmp(kind, "lightning")) {
    auto *x = (LxPV *) lv_obj_get_user_data(root);
    if (!x) return false;
    x->now = lx_now(p);
    x->d = fw::lx::sample(x->now, p["quiet"] | false);
    lv_obj_invalidate(x->dots);
  }
  return true;  // agents, media, clock, github: nothing changes
}
}  // namespace fw

FW_COMPONENT(card_previews, "Card previews", "components", R"({
  "about": "The artwork at the top right of a Home card, at its size on the card (icon size card: 72 / 48). Plain shapes from the atoms: mascots, value arcs, bars, rings and dots.",
  "frame": [48, 48],
  "props": [
    {"name": "preview", "about": "Which picture to draw, as it appears at the top right of a Home card: agents, media, a dial, a switch, the clock, a timer, GitHub, energy or lightning.", "type": "select", "options": ["agents", "media", "dial", "switch", "clock", "timer", "github", "energy", "lightning"], "default": "dial"},
    {"name": "value", "about": "The level the picture shows where it has one, such as how far round the dial is, from 0 to 100.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 64},
    {"name": "on", "about": "Whether the thing the picture stands for is on, such as the switch or the dial's light.", "type": "bool", "default": true},
    {"name": "unavailable", "about": "Shows the picture greyed out, for a device that cannot be reached.", "type": "bool", "default": false},
    {"name": "quiet", "about": "For the lightning picture: shows a quiet sky with no recent strikes.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "agents", "props": {"preview": "agents"}},
    {"label": "media", "props": {"preview": "media"}},
    {"label": "dial", "props": {"preview": "dial"}},
    {"label": "switch · on", "props": {"preview": "switch"}},
    {"label": "switch · off", "props": {"preview": "switch", "on": false}},
    {"label": "switch · unavailable", "props": {"preview": "switch", "unavailable": true}},
    {"label": "clock", "props": {"preview": "clock"}},
    {"label": "timer", "props": {"preview": "timer", "value": 65}},
    {"label": "github", "props": {"preview": "github"}},
    {"label": "energy", "props": {"preview": "energy"}},
    {"label": "lightning", "props": {"preview": "lightning", "now": 1792944000000}}
  ]
})") {
  fw::card_preview(parent, p["preview"] | "dial", p);
}

FW_UPDATE(card_previews) {
  return fw::card_preview_set(root, p["preview"] | "dial", p);
}
