// Foundations > Motion: tokens/motion.json (via the generated motion_gen.h) drawn and played by the panel renderer.
// Sections: easing curves (plotted from the very path callbacks LVGL runs, a dot running along each curve and a box
// moving with it, slowed to 1 s; tap = replay), durations (bars; tap = a sweep at the real length), patterns (each
// named pattern live on a sample built from tokens: tap a sample to play it), reduced motion (what each pattern does).
// Playground: reduced_motion (handled by fw::render for the whole stage) and loop = one pattern's sample at full screen
// size, playing over and over (the frame-rate bench on the panels: GET /fw/perf while it runs).
// Status (WIP/Done) = this file + tokens/motion.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)
constexpr uint32_t CURVE_MS = 1000;  // the easing demo runs slowed down so the shape reads

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *row(lv_obj_t *parent, int gap) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
void fill(lv_obj_t *o, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
}
void outline(lv_obj_t *o, lv_color_t c, int w) {
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
}
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s2) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  outline(e, T->line, stroke::hair);
  return e;
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  return l;
}
int p7(float px720) { return px(px720 / 1.5f); }  // a Facet docs value given in 720 px (component CSS) -> this build
void on_tap(lv_obj_t *o, lv_event_cb_t cb, void *user) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, user);
}
void free_on_delete(lv_obj_t *o, void *mem) {
  lv_obj_add_event_cb(o, [](lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, mem);
}
// eased progress 0..1024 at time t of 0..1024 with easing e: the same callback LVGL runs
int32_t eased(int e, int32_t t) {
  lv_anim_t a;
  lv_anim_init(&a);
  a.act_time = t;
  a.duration = 1024;
  a.start_value = 0;
  a.end_value = 1024;
  return mo::EASINGS[e].path(&a);
}
const char *ms_text(uint32_t ms, char *b, size_t n) {
  if (ms >= 1000 && ms % 1000 == 0) snprintf(b, n, "%u s", (unsigned) (ms / 1000));
  else if (ms >= 1000) snprintf(b, n, "%.1f s", ms / 1000.0);
  else snprintf(b, n, "%u ms", (unsigned) ms);
  return b;
}

// ---------------------------------------------------------------- easing curves
struct Curve { int e; lv_obj_t *plot, *dot, *track, *puck; int in; };
StrokePt curve_at(float t, void *u) {  // the plotted curve: x = t, y = eased(t) (LVGL's own 1/1024 resolution), y up
  const Curve *c = (const Curve *) u;
  return {t * c->in, c->in - eased(c->e, (int32_t) std::lround(t * 1024)) * c->in / 1024.f};
}
void curve_dot(void *v, int32_t t) {  // t = time 0..1024 (linear): dot at (t, eased(t))
  auto *c = (Curve *) lv_obj_get_user_data((lv_obj_t *) v);
  const int d = lv_obj_get_width(c->dot);
  lv_obj_set_pos(c->dot, t * c->in / 1024 - d / 2, c->in - eased(c->e, t) * c->in / 1024 - d / 2);
}
void curve_play(Curve *c) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, c->dot);
  lv_anim_set_exec_cb(&a, curve_dot);
  lv_anim_set_values(&a, 0, 1024);
  lv_anim_set_duration(&a, CURVE_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
  lv_anim_init(&a);  // the box: the easing itself, as an animation would use it
  lv_anim_set_var(&a, c->puck);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t x) { lv_obj_set_x((lv_obj_t *) o, x); });
  lv_anim_set_values(&a, 0, lv_obj_get_content_width(c->track) - lv_obj_get_width(c->puck));
  lv_anim_set_duration(&a, CURVE_MS);
  lv_anim_set_path_cb(&a, mo::EASINGS[c->e].path);
  lv_anim_start(&a);
}
void easing(lv_obj_t *l) {
  const int P = px(128), dot = px(12), puck = px(24);
  for (int i = 0; i < mo::N_EASINGS; i++) {
    const mo::EasingDef &k = mo::EASINGS[i];
    lv_obj_t *e = entry(l, sp::s3);
    lv_obj_t *h = row(e, sp::s5);
    text(h, ty::c_caps_sm, k.id, T->text);
    text(h, ty::c_meta, k.css, T->text_dim);
    char b[160];
    snprintf(b, sizeof b, "LVGL: %s · %s", k.lvgl, k.use);
    text(e, ty::c_meta, b, T->text_dim, inner_w());
    lv_obj_t *r = row(e, sp::s7);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    auto *c = (Curve *) lv_malloc_zeroed(sizeof(Curve));
    free_on_delete(e, c);
    c->e = i;
    c->plot = box(r);
    lv_obj_set_size(c->plot, P, P);
    fill(c->plot, T->surface);
    outline(c->plot, T->line, stroke::hair);
    c->in = P - 2 * stroke::hair;
    // the linear reference (the diagonal, hairline, square ends) and the curve (edge, round caps): stroke atoms
    const StrokePt dg[2] = {{0, (float) c->in}, {(float) c->in, 0}};
    stroke_at(c->plot, dg, 2, stroke::hair, T->line, Cap::butt);
    const std::vector<StrokePt> cp = stroke_sample(curve_at, c);
    stroke_at(c->plot, cp.data(), (int) cp.size(), stroke::edge, T->accent);
    c->dot = box(c->plot);
    lv_obj_set_size(c->dot, dot, dot);
    lv_obj_set_style_radius(c->dot, LV_RADIUS_CIRCLE, 0);
    fill(c->dot, T->text);
    lv_obj_set_user_data(c->dot, c);
    curve_dot(c->dot, 1024);
    lv_obj_t *side = column(r, sp::s2, inner_w() - P - sp::s7);
    c->track = box(side);
    lv_obj_set_size(c->track, LV_PCT(100), puck + 2 * stroke::hair);
    outline(c->track, T->line, stroke::hair);
    lv_obj_set_style_radius(c->track, rad::button, 0);
    c->puck = box(c->track);
    lv_obj_set_size(c->puck, puck, puck);
    lv_obj_set_style_radius(c->puck, rad::button - stroke::hair > 0 ? rad::button - stroke::hair : 0, 0);
    fill(c->puck, T->accent);
    char m[64];
    snprintf(m, sizeof m, "shown at %s · tap to replay", ms_text(CURVE_MS, b, sizeof b));
    text(side, ty::c_meta_sm, m, T->text_dim);
    on_tap(e, [](lv_event_t *ev) { curve_play((Curve *) lv_event_get_user_data(ev)); }, c);
    lv_obj_update_layout(e);
    curve_play(c);
  }
}

// ---------------------------------------------------------------- durations
struct Bar { lv_obj_t *run; int w; uint32_t ms; };
void durations(lv_obj_t *l) {
  const int name_w = px(112), val_w = px(56);
  const int bar_w = inner_w() - name_w - val_w - 2 * sp::s4;
  for (int g = 0; g < mo::N_GROUPS; g++) {
    const char *gid = mo::GROUPS[g].id;
    uint32_t max = 0;
    for (int i = 0; i < mo::N_DURATIONS; i++)
      if (!strcmp(mo::DURATIONS[i].group, gid)) max = std::max(max, mo::DURATIONS[i].ms);
    const bool log = !strcmp(gid, "ambient");
    lv_obj_t *e = entry(l, sp::s1);
    text(e, ty::c_caps_sm, mo::GROUPS[g].label, T->text);
    char about[200];
    snprintf(about, sizeof about, "%s%s", mo::GROUPS[g].about, log ? " Bars on a log scale (100 ms to 60 s)." : "");
    text(e, ty::c_meta, about, T->text_dim, inner_w());
    for (int i = 0; i < mo::N_DURATIONS; i++) {
      const mo::Duration &k = mo::DURATIONS[i];
      if (strcmp(k.group, gid)) continue;
      lv_obj_t *re = entry(l, sp::s1);
      lv_obj_t *r = row(re, sp::s4);
      char b[48];
      snprintf(b, sizeof b, "ms::%s", k.id);
      for (char *c = b; *c; c++) if (*c == '-') *c = '_';
      lv_obj_set_width(text(r, ty::c_caps_sm, b, T->text), name_w);
      lv_obj_t *track = box(r);
      lv_obj_set_size(track, bar_w, sp::s2);
      fill(track, T->line);
      int w = log ? (int) lroundf(bar_w * (log10f((float) k.ms) - 2.f) / (log10f(60000.f) - 2.f)) : (int) (bar_w * (int64_t) k.ms / max);
      w = std::max(w, sp::s1);
      lv_obj_t *f = box(track);
      lv_obj_set_size(f, w, sp::s2);
      fill(f, T->accent_dim);
      auto *bar = (Bar *) lv_malloc(sizeof(Bar));
      free_on_delete(re, bar);
      bar->run = box(track);
      lv_obj_set_size(bar->run, 0, sp::s2);
      fill(bar->run, T->accent);
      bar->w = w;
      bar->ms = k.ms;
      lv_obj_t *v = text(r, ty::c_meta, ms_text(k.ms, b, sizeof b), T->text_dim);
      lv_obj_set_width(v, val_w);
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
      text(re, ty::c_meta_sm, k.use, T->text_dim, inner_w());
      on_tap(re, [](lv_event_t *ev) {
        auto *b = (Bar *) lv_event_get_user_data(ev);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, b->run);
        lv_anim_set_exec_cb(&a, [](void *o, int32_t x) { lv_obj_set_width((lv_obj_t *) o, x); });
        lv_anim_set_values(&a, 0, b->w);
        lv_anim_set_duration(&a, b->ms);
        lv_anim_set_path_cb(&a, lv_anim_path_linear);
        lv_anim_set_completed_cb(&a, [](lv_anim_t *x) { lv_obj_set_width((lv_obj_t *) x->var, 0); });
        lv_anim_start(&a);
      }, bar);
    }
  }
}

// ---------------------------------------------------------------- pattern samples
// one live sample per pattern, in a frame = a small screen (bg, hairline edge, no radius: the panels are square glass).
// Every sample animates through fw::animate / appear / rise with its pattern: the same calls a component makes.
struct Demo {
  mo::Id p;
  lv_obj_t *frame = nullptr, *a = nullptr, *b = nullptr, *c = nullptr;
  lv_obj_t *rows[2][8]{};  // screen sample: [Home, Lights] header + blocks
  lv_obj_t *page[2]{};
  int n = 0, nr[2]{}, state = 0;
  int32_t shown = 0, target = 0, travel = 0, size = 0;
  StrokePt pts[2]{};  // clock hand: its two ends (mask px)
  lv_timer_t *timer = nullptr, *loop = nullptr;
};
void play(Demo *d);
lv_obj_t *frame(lv_obj_t *parent, int w, int h) {
  lv_obj_t *f = box(parent);
  lv_obj_set_size(f, w, h);
  fill(f, T->bg);
  outline(f, T->line, stroke::hair);
  return f;
}
void hint(lv_obj_t *f, const char *t) {
  lv_obj_t *h = text(f, ty::c_meta_sm, t, T->text_dim);
  lv_obj_add_flag(h, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(h, LV_ALIGN_BOTTOM_RIGHT, -sp::s3, -sp::s2);
}
void set_toggle(void *o, int32_t v) {  // v 0..255: off -> on (track line -> accent, knob text-dim -> on-accent, slides)
  auto *d = (Demo *) lv_obj_get_user_data((lv_obj_t *) o);
  lv_obj_set_style_bg_color(d->a, lv_color_mix(T->accent, T->line, (uint8_t) v), 0);
  lv_obj_set_style_bg_color(d->b, lv_color_mix(T->on_accent, T->text_dim, (uint8_t) v), 0);
  lv_obj_set_style_translate_x(d->b, d->travel * v / 255, 0);
  d->shown = v;
}
void set_value(void *o, int32_t v) {
  auto *d = (Demo *) lv_obj_get_user_data((lv_obj_t *) o);
  d->shown = v;
  char b[8];
  snprintf(b, sizeof b, "%d", (int) v);
  set_text(d->a, ty::c_number, b);
  lv_obj_set_width(d->c, std::max(1, (int) (lv_obj_get_content_width(d->b) * v / 100)));
}
void set_hand(void *o, int32_t deg10) {
  auto *d = (Demo *) lv_obj_get_user_data((lv_obj_t *) o);
  d->shown = deg10;
  const float c = d->size / 2.f - 0.5f, len = d->size / 2.f - sp::s4, a = (deg10 / 10.f - 90.f) * 3.14159265f / 180.f;
  const StrokePt np[2] = {{c, c}, {c + len * cosf(a), c + len * sinf(a)}};
  // re-raster the hand (a small mask); redraw only the box around the old and the new hand
  const float m = stroke::edge / 2.f + 1;
  float x0 = c, y0 = c, x1 = c, y1 = c;
  for (const StrokePt &q : {d->pts[1], np[1]}) x0 = std::min(x0, q.x), x1 = std::max(x1, q.x), y0 = std::min(y0, q.y), y1 = std::max(y1, q.y);
  const lv_area_t dirty = {std::max(0, (int) floorf(x0 - m)), std::max(0, (int) floorf(y0 - m)),
                           std::min(d->size - 1, (int) ceilf(x1 + m)), std::min(d->size - 1, (int) ceilf(y1 + m))};
  d->pts[0] = np[0];
  d->pts[1] = np[1];
  stroke_redraw(d->a, d->pts, 2, &dirty);
}
void toast_out(lv_timer_t *t) {
  auto *d = (Demo *) lv_timer_get_user_data(t);
  d->timer = nullptr;
  if (d->state) play(d);
}

// build the sample of pattern p into frame f (w x h): state objects into d
void sample(Demo *d, int w, int h) {
  lv_obj_t *f = d->frame;
  switch (d->p) {
    case mo::press:
    case mo::release: {  // the real Button: its press / release looks run these two patterns
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(f, lay::gutter, 0);
      ArduinoJson::JsonDocument pd;
      pd["kind"] = "primary";
      pd["label"] = d->p == mo::press ? "Hold me" : "Let go";
      const fw::Component *b = fw::find("button");
      if (b) b->build(f, pd.as<ArduinoJson::JsonObjectConst>());
      pd["kind"] = "secondary";
      if (b) b->build(f, pd.as<ArduinoJson::JsonObjectConst>());
      d->a = lv_obj_get_child(f, 0);
      d->b = lv_obj_get_child(f, 1);
      hint(f, d->p == mo::press ? "finger down: press" : "finger up: release");
      break;
    }
    case mo::toggle: {  // Facet toggle: 80 x 48 track, 32 knob 8 in, travel 32 (720 px)
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      d->a = box(f);
      lv_obj_set_size(d->a, p7(80), p7(48));
      lv_obj_set_style_radius(d->a, LV_RADIUS_CIRCLE, 0);
      fill(d->a, T->line);
      d->b = box(d->a);
      const int k = p7(32), in = (p7(48) - k) / 2;
      lv_obj_set_size(d->b, k, k);
      lv_obj_set_pos(d->b, in, in);
      lv_obj_set_style_radius(d->b, LV_RADIUS_CIRCLE, 0);
      fill(d->b, T->text_dim);
      d->travel = p7(80) - k - 2 * in;
      lv_obj_set_user_data(d->a, d);
      hint(f, "tap: on / off");
      break;
    }
    case mo::fade: {
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      d->a = box(f);
      lv_obj_set_size(d->a, w / 2, h / 2);
      fill(d->a, T->surface);
      outline(d->a, T->line, stroke::hair);
      lv_obj_set_flex_flow(d->a, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(d->a, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
      lv_obj_set_style_pad_hor(d->a, lay::card_pad, 0);
      text(d->a, ty::c_caps_sm, "Office", T->text_dim);
      text(d->a, ty::c_title, "21.5°", T->text);
      d->state = 1;
      hint(f, "tap: fade out / in");
      break;
    }
    case mo::sheet_open:
    case mo::sheet_close: {  // scrim (bg at opacity.scrim) + panel from 40 % down, ring radius, hairline top
      text(f, ty::c_title, "Living room", T->text);
      lv_obj_set_style_pad_all(f, sp::s5, 0);
      d->a = box(f);
      lv_obj_add_flag(d->a, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_set_size(d->a, w, h);
      lv_obj_set_pos(d->a, -sp::s5, -sp::s5);
      fill(d->a, T->bg, (lv_opa_t) lroundf(mo::OPA_SCRIM * 255));
      d->b = box(f);
      lv_obj_add_flag(d->b, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_set_size(d->b, w, h - h * 2 / 5 + rad::ring);
      lv_obj_set_pos(d->b, -sp::s5, h * 2 / 5 - sp::s5);
      fill(d->b, T->surface);
      outline(d->b, T->line_hi, stroke::hair);
      lv_obj_set_style_border_side(d->b, LV_BORDER_SIDE_TOP, 0);
      lv_obj_set_style_radius(d->b, rad::ring, 0);
      lv_obj_set_style_pad_all(d->b, sp::s5, 0);
      lv_obj_set_flex_flow(d->b, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_row(d->b, sp::s3, 0);
      text(d->b, ty::c_caps_sm, "Speakers", T->text_dim);
      text(d->b, ty::c_label, "Kitchen", T->text);
      text(d->b, ty::c_label, "Office", T->text);
      for (lv_obj_t *o : {d->a, d->b}) lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
      hint(f, "tap: open / close");
      lv_obj_move_to_index(lv_obj_get_child(f, -1), 1);  // hint under the scrim
      break;
    }
    case mo::toast_in:
    case mo::toast_out: {  // toast at the top inset: surface, hairline, button radius, accent dot + one line
      text(f, ty::c_title, "Home", T->text);
      lv_obj_set_style_pad_all(f, sp::s5, 0);
      d->a = box(f);
      lv_obj_add_flag(d->a, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_set_size(d->a, w - 2 * sp::s5, touch::button_h);
      lv_obj_set_pos(d->a, 0, 0);
      fill(d->a, T->surface);
      outline(d->a, T->line_hi, stroke::hair);
      lv_obj_set_style_radius(d->a, rad::button, 0);
      lv_obj_set_style_pad_hor(d->a, sp::s4, 0);
      lv_obj_set_flex_flow(d->a, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(d->a, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(d->a, sp::s3, 0);
      lv_obj_t *dot = box(d->a);
      lv_obj_set_size(dot, sp::s2, sp::s2);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      fill(dot, T->accent);
      text(d->a, ty::c_label, "Front door: someone is there", T->text);
      lv_obj_set_style_opa(d->a, LV_OPA_TRANSP, 0);
      hint(f, "tap: toast (leaves after 1.5 s here, toast-life 6 s)");
      break;
    }
    case mo::screen: {  // two small screens, Home (header + 2 x 2 cards) and Lights (back + title, 3 rows): a tap cuts to
      // the other one and its content rises (Home: drops in from above). Cards side by side = one row (rise together).
      const int pad = sp::s5, gap = sp::s3, iw = w - 2 * pad;
      const int hh = touch::back, bh = (h - 2 * pad - hh - 3 * gap) / 2, rh = (h - 2 * pad - hh - 4 * gap) / 3;
      for (int s = 0; s < 2; s++) {
        lv_obj_t *pg = box(f);
        d->page[s] = pg;
        lv_obj_set_size(pg, w - 2 * stroke::hair, h - 2 * stroke::hair);
        lv_obj_set_style_pad_all(pg, pad, 0);
        lv_obj_set_flex_flow(pg, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(pg, gap, 0);
        lv_obj_set_style_pad_column(pg, gap, 0);
        lv_obj_t *hd = row(pg, sp::s3);
        lv_obj_set_height(hd, hh);
        if (s) {
          lv_obj_t *back = box(hd);
          lv_obj_set_size(back, touch::back, touch::back);
          outline(back, T->line, stroke::hair);
          lv_obj_set_style_radius(back, rad::square, 0);
        }
        text(hd, ty::c_title, s ? "Lights" : "Home", T->text);
        d->rows[s][d->nr[s]++] = hd;
        const char *home[] = {"Lights", "Media", "Weather", "Timer"}, *lights[] = {"Ceiling", "Sofa table", "Chair"};
        for (int i = 0; i < (s ? 3 : 4); i++) {
          lv_obj_t *c = box(pg);
          lv_obj_set_size(c, s ? iw : (iw - gap) / 2, s ? rh : bh);
          fill(c, T->surface);
          outline(c, T->line, stroke::hair);
          lv_obj_set_style_pad_all(c, sp::s3, 0);
          text(c, ty::c_label, s ? lights[i] : home[i], T->text);
          d->rows[s][d->nr[s]++] = c;
        }
        if (s) lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
      }
      hint(f, "tap: open Lights / back Home");
      break;
    }
    case mo::value: {  // a number + bar sweeping to the new value (dial-sweep)
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_row(f, sp::s4, 0);
      d->a = text(f, ty::c_number, "18", T->text);
      d->b = box(f);
      lv_obj_set_size(d->b, w / 2, sp::s2);
      fill(d->b, T->line);
      d->c = box(d->b);
      lv_obj_set_height(d->c, sp::s2);
      fill(d->c, T->accent);
      lv_obj_set_user_data(d->a, d);
      lv_obj_update_layout(f);
      set_value(d->a, 18);
      d->target = 18;
      hint(f, "tap: 18 / 72");
      break;
    }
    case mo::clock_hand: {  // face + hand; a tap moves it 15 minutes so the turn reads
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      d->size = std::min(w, h) * 5 / 8 / 4 * 4;
      lv_obj_t *face = box(f);
      lv_obj_set_size(face, d->size, d->size);
      lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
      outline(face, T->line_hi, stroke::hair);
      d->size -= 2 * stroke::hair;
      d->pts[0] = d->pts[1] = {d->size / 2.f - 0.5f, d->size / 2.f - 0.5f};
      d->a = stroke_path(face, d->size, d->size, d->pts, 2, stroke::edge, T->accent);  // the hand: stroke atom, round caps
      lv_obj_set_user_data(d->a, d);
      lv_obj_t *hub = box(face);
      lv_obj_set_size(hub, sp::s2, sp::s2);
      lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
      fill(hub, T->accent);
      lv_obj_center(hub);
      set_hand(d->a, 0);
      hint(f, "tap: +15 min");
      break;
    }
    case mo::pulse: {  // a working dot breathing (loop); reduced = still
      lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(f, sp::s3, 0);
      d->a = box(f);
      lv_obj_set_size(d->a, sp::s4, sp::s4);
      lv_obj_set_style_radius(d->a, LV_RADIUS_CIRCLE, 0);
      fill(d->a, T->accent);
      text(f, ty::c_label, "Working", T->text);
      hint(f, "loops by itself");
      break;
    }
    default: break;
  }
}

void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void play(Demo *d) {
  switch (d->p) {
    case mo::press:
    case mo::release: {  // (loop / bench) press and release the Button in turn: the same state changes a finger makes
      lv_obj_t *b = d->a;
      if (lv_obj_has_state(b, LV_STATE_PRESSED)) lv_obj_remove_state(b, LV_STATE_PRESSED);
      else lv_obj_add_state(b, LV_STATE_PRESSED);
      break;
    }
    case mo::toggle:
      d->state = !d->state;
      animate(d->a, mo::toggle, {set_toggle, d->shown, d->state ? 255 : 0, mo::MOVE});
      break;
    case mo::fade:
      d->state = !d->state;
      appear(d->a, mo::fade, d->state, 0);
      break;
    case mo::sheet_open:
    case mo::sheet_close:
      d->state = !d->state;
      appear(d->a, d->state ? mo::sheet_open : mo::sheet_close, d->state, 0);
      appear(d->b, d->state ? mo::sheet_open : mo::sheet_close, d->state, 1);
      break;
    case mo::toast_in:
    case mo::toast_out:
      d->state = !d->state;
      appear(d->a, d->state ? mo::toast_in : mo::toast_out, d->state, -1);
      if (d->timer) { lv_timer_delete(d->timer); d->timer = nullptr; }
      if (d->state && !d->loop) {
        d->timer = lv_timer_create(toast_out, 1500, d);
        lv_timer_set_repeat_count(d->timer, 1);
        lv_timer_set_auto_delete(d->timer, true);
      }
      break;
    case mo::screen:  // cut (the old screen goes at once), then the new one's content rises
      d->state = !d->state;
      lv_obj_add_flag(d->page[!d->state], LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(d->page[d->state], LV_OBJ_FLAG_HIDDEN);
      rise(d->rows[d->state], d->nr[d->state], !d->state);
      break;
    case mo::value:
      d->target = d->target == 72 ? 18 : 72;
      animate(d->a, mo::value, {set_value, d->shown, d->target, mo::VALUE});
      break;
    case mo::clock_hand:
      d->target = d->shown + 900;  // +15 minutes = 90 degrees
      animate(d->a, mo::clock_hand, {set_hand, d->shown, d->target, mo::MOVE});
      break;
    case mo::pulse: {
      Anim a{set_opa, LV_OPA_COVER, (int32_t) lroundf(mo::OPA_PULSE_MIN * 255), mo::OPA};
      a.loop = true;
      animate(d->a, mo::pulse, a);
      break;
    }
    default: break;
  }
}
// loop period per pattern: one step per pattern length (+ one frame), so the bench animates without a pause
uint32_t period(mo::Id p) {
  const mo::Pattern &k = mo::PATTERNS[p];
  return k.ms + (uint32_t) (k.steps - 1) * k.stagger + 20;
}
Demo *demo(lv_obj_t *parent, mo::Id p, int w, int h, bool loop) {
  auto *d = new Demo();
  d->p = p;
  d->frame = frame(parent, w, h);
  lv_obj_add_event_cb(d->frame, [](lv_event_t *e) {
    auto *d = (Demo *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    if (d->loop) lv_timer_delete(d->loop);
    delete d;
  }, LV_EVENT_DELETE, d);
  sample(d, w, h);
  if (p == mo::pulse) play(d);
  else if (loop) {
    d->loop = lv_timer_create([](lv_timer_t *t) { play((Demo *) lv_timer_get_user_data(t)); }, period(p), d);
  } else if (p != mo::press && p != mo::release)
    on_tap(d->frame, [](lv_event_t *e) { play((Demo *) lv_event_get_user_data(e)); }, d);
  return d;
}

void spec_line(lv_obj_t *e, const mo::Pattern &k) {
  char b[160], m[24];
  int n = snprintf(b, sizeof b, "%s · %s", ms_text(k.ms, m, sizeof m), mo::EASINGS[k.easing].id);
  if (k.offset) n += snprintf(b + n, sizeof b - n, " · offset %d px", k.offset);
  if (k.stagger) snprintf(b + n, sizeof b - n, " · stagger %u ms x %d", (unsigned) k.stagger, k.steps);
  lv_obj_t *h = row(e, sp::s5);
  text(h, ty::c_caps_sm, k.label, T->text);
  text(h, ty::c_meta, b, T->text_dim);
}
void patterns(lv_obj_t *l, bool reduced) {
  const int w = inner_w(), h = px(176);
  for (int i = 0; i < mo::N; i++) {
    const mo::Pattern &k = mo::PATTERNS[i];
    lv_obj_t *e = entry(l, sp::s2);
    spec_line(e, k);
    char b[200];
    snprintf(b, sizeof b, "mo::%s: %s", k.id, k.animates);
    for (char *c = b + 4; *c && *c != ':'; c++) if (*c == '-') *c = '_';
    text(e, ty::c_meta, b, T->text_dim, w);
    snprintf(b, sizeof b, "Reduced motion%s: %s, %s", reduced ? " (on)" : "", mo::REDUCED_NAMES[k.reduced], mo::REDUCED_MODES[k.reduced]);
    text(e, ty::c_meta, b, reduced ? T->accent_text : T->text_dim, w);
    demo(e, (mo::Id) i, w, h, false);
  }
}
void reduced_page(lv_obj_t *l, bool on) {
  lv_obj_t *e = entry(l, sp::s2);
  text(e, ty::c_caps_sm, on ? "Reduced motion: on (playground)" : "Reduced motion: off (playground)", on ? T->accent_text : T->text);
  text(e, ty::c_meta, mo::REDUCED_ABOUT, T->text_dim, inner_w());
  for (int i = 0; i < mo::N; i++) {
    const mo::Pattern &k = mo::PATTERNS[i];
    lv_obj_t *r = row(entry(l, 0), sp::s4);
    lv_obj_set_width(text(r, ty::c_caps_sm, k.label, T->text), px(176));
    char b[160];
    snprintf(b, sizeof b, "%s: %s", mo::REDUCED_NAMES[k.reduced], mo::REDUCED_MODES[k.reduced]);
    text(r, ty::c_meta, b, T->text_dim, inner_w() - px(176) - sp::s4);
  }
}
mo::Id pattern_of(const char *s) {
  for (int i = 0; i < mo::N; i++)
    if (!strcmp(mo::PATTERNS[i].id, s)) return (mo::Id) i;
  return mo::N;
}
}  // namespace

FW_COMPONENT(motion, "Motion", "foundations", R"({
  "about": "Durations, easings and the motion patterns components use, including the reduced-motion rule.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "type": "select", "options": )" FW_MOTION_SECTION_IDS R"(, "default": "easing"},
    {"name": "reduced_motion", "type": "bool", "default": false},
    {"name": "loop", "type": "select", "options": )" FW_MOTION_PATTERN_IDS R"(, "default": "off"}
  ],
  "variants": )" FW_MOTION_SECTIONS R"(
})") {
  const char *s = p["section"] | "easing";
  const bool reduced = mo::reduced(parent);
  const mo::Id lp = pattern_of(p["loop"] | "off");
  if (lp != mo::N) {  // one pattern, full screen, over and over (frame-rate bench)
    demo(parent, lp, px(W), px(W), true);
    return;
  }
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "durations")) durations(l);
  else if (!strcmp(s, "patterns")) patterns(l, reduced);
  else if (!strcmp(s, "reduced")) reduced_page(l, reduced);
  else easing(l);
}
