// The mascot runtime (core/mascot.h): parts as tiny LVGL objects, moods from mas::MOODS, motion = mas::PATTERNS
// keyframe tracks played by one lv_anim per pattern (linear clock; each keyframe segment eased with the pattern's
// motion.json easing, like CSS keyframes). Every part's x / y / height / opacity / rotation = its mood pose + the sum of
// what the playing patterns add, written only when the whole px value changes (an unchanged frame invalidates nothing).
#include <cstdint>
#include "fw.h"

namespace fw {
namespace {
using namespace mas;

constexpr int MAX_TRACKS = 3;
struct M;
struct Play { M *m; uint8_t pat; };
struct M {
  lv_obj_t *root, *body, *face, *eye[2], *arch[2], *brow[2], *bang[2], *z[Z_COUNT], *dot[DOT_COUNT];
  lv_point_precise_t zp[Z_COUNT][4];
  const Theme *th;
  size_t col;          // override colour token (Theme offset), SIZE_MAX = the mood's (resolved at once: names may not outlive the call)
  Mood mood;
  int k;               // scale
  uint32_t phase, only;
  Play play[N_PATS];
  bool on[N_PATS];
  float val[N_PATS][MAX_TRACKS][3];  // current value per pattern x track x instance (z / dots: up to 3)
  int32_t last[24];                  // last written whole values (skip unchanged writes)
  int zx[Z_COUNT], zy[Z_COUNT], zs[Z_COUNT];  // a z's current offset (px) and scale (permille)
};
int live = 0;  // mascots alive (the auto instance delay)

lv_obj_t *part(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE));
  lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
  return o;
}
lv_color_t tok(const Theme *t, size_t off) { return *(const lv_color_t *) ((const char *) t + off); }
size_t colour_off(const char *name) {  // colour.json token name -> Theme offset; nullptr / "mood" / unknown = SIZE_MAX
  if (name && *name && strcmp(name, "mood"))
    for (int i = 0; i < tokens::N_COLOURS; i++)
      if (!strcmp(tokens::COLOURS[i].name, name)) return tokens::COLOURS[i].off;
  return SIZE_MAX;
}

// eased keyframe value of a track at time `ms` of the pattern (wrapped into the loop)
float sample(const Pattern &p, const Track &tr, int32_t ms) {
  const int32_t L = (int32_t) p.ms;
  ms %= L;
  if (ms < 0) ms += L;
  const int32_t pm = (int32_t) ((int64_t) ms * 1000 / L);  // permille
  int j = 0;
  while (j + 2 < tr.n && tr.keys[j + 1].t <= pm) j++;
  const Key &a = tr.keys[j], &b = tr.keys[j + 1];
  if (b.t == a.t) return b.v;
  lv_anim_t an;  // the motion.json easing, as LVGL runs it, over this segment
  lv_anim_init(&an);
  an.duration = 1024;
  an.act_time = (int32_t) ((int64_t) (ms * 1000 - a.t * L) * 1024 / ((b.t - a.t) * L));
  an.act_time = std::max<int32_t>(0, std::min<int32_t>(1024, an.act_time));
  an.start_value = 0;
  an.end_value = 1024;
  const int32_t e = mo::EASINGS[p.easing].path(&an);
  return a.v + (b.v - a.v) * e / 1024.f;
}
int instances(Part pt) { return pt == PART_Z ? Z_COUNT : pt == PART_DOT ? DOT_COUNT : 1; }
// every track of pattern p at time t (instance i of z / dots a stagger later)
void eval(M *m, int p, int32_t t) {
  const Pattern &P = PATTERNS[p];
  for (int k = 0; k < P.n_tracks && k < MAX_TRACKS; k++)
    for (int i = 0; i < instances(P.tracks[k].part) && i < 3; i++) m->val[p][k][i] = sample(P, P.tracks[k], t - (int32_t) (i * P.stagger));
}
// sum of what the playing patterns add to (part, prop, instance); `none` = the value when nothing plays it
float sum(M *m, Part pt, Prop pr, int i, float none, bool *any = nullptr) {
  float s = 0;
  bool hit = false;
  for (int p = 0; p < N_PATS; p++) {
    if (!m->on[p]) continue;
    const Pattern &P = PATTERNS[p];
    for (int k = 0; k < P.n_tracks && k < MAX_TRACKS; k++)
      if (P.tracks[k].part == pt && P.tracks[k].prop == pr) {
        const float v = m->val[p][k][i];
        s += (pr == PROP_X || pr == PROP_Y || pr == PROP_H) ? v * amp(P.tracks[k]) * m->k : v;
        hit = true;
      }
  }
  if (any) *any = hit;
  return hit ? s : none;
}
bool changed(M *m, int slot, int32_t v) {
  if (m->last[slot] == v) return false;
  m->last[slot] = v;
  return true;
}
// a z: its glyph (size G.z.size, bottom-left 1 px right of and 1 px above the anchor, like the docs SVG) scaled by
// zs permille about the anchor (G.z.x, G.z.y), moved by (zx, zy); the line object sits at the glyph's top-left
void z_place(M *m, int i, int zx, int zy, int zs) {
  const int k = m->k, S = G.z.size;
  static const int RX[4] = {0, 1, 0, 1}, RY[4] = {0, 0, 1, 1};  // top bar, diagonal back, bottom bar
  const float sc = zs / 1000.f * k;
  const int top = (int) lroundf((1 + S) * sc);  // glyph top above the anchor
  for (int j = 0; j < 4; j++)
    m->zp[i][j] = {(lv_value_precise_t) lroundf((1 + RX[j] * S) * sc), (lv_value_precise_t) (top - (int) lroundf((1 + (1 - RY[j]) * S) * sc) + 0)};
  lv_line_set_points(m->z[i], m->zp[i], 4);
  lv_obj_set_pos(m->z[i], G.z.x * k + zx, G.z.y * k + zy - top);
}
void apply(M *m) {
  const MoodDef &md = MOODS[m->mood];
  const int k = m->k;
  // body: pose + y (hop, bob, tilt lift) + height (breathe, bottom fixed) + rotation (pose + tilt), pivot bottom centre
  const int dh = (int) lroundf(sum(m, PART_BODY, PROP_H, 0, 0));
  const int bh = G.body_h * k + dh;
  const int by = md.pose_y * k + (int) lroundf(sum(m, PART_BODY, PROP_Y, 0, 0)) - dh;
  if (changed(m, 0, bh)) { lv_obj_set_height(m->body, bh); lv_obj_set_style_transform_pivot_y(m->body, bh, 0); }
  if (changed(m, 1, by)) lv_obj_set_y(m->body, by);
  const int rot = (int) lroundf((md.pose_rot + sum(m, PART_BODY, PROP_ROT, 0, 0)) * 10);
  if (changed(m, 2, rot)) lv_obj_set_style_transform_rotation(m->body, rot, 0);
  // face (eyes + brows): x (glance, scan), y (sigh)
  const int fx = (int) lroundf(sum(m, PART_FACE, PROP_X, 0, 0)), fy = (int) lroundf(sum(m, PART_FACE, PROP_Y, 0, 0));
  if (changed(m, 3, fx)) lv_obj_set_x(m->face, fx);
  if (changed(m, 4, fy)) lv_obj_set_y(m->face, fy);
  // eyes: close 0..1 = height from the shape's down to blink-h, centred
  const Shape &e = G.eyes[md.eye];
  const float c = std::min(1.f, std::max(0.f, sum(m, PART_EYES, PROP_CLOSE, 0, 0)));
  const int eh = (int) lroundf(e.h * k - c * (e.h - G.blink_h) * k);
  if (changed(m, 5, eh))
    if (!e.arch) for (int s = 0; s < 2; s++) { lv_obj_set_height(m->eye[s], eh); lv_obj_set_y(m->eye[s], e.y * k + (e.h * k - eh) / 2); }
  // "!": opacity (tilt)
  const int bo = (int) lroundf(sum(m, PART_BANG, PROP_OPA, 0, 1) * 255);
  if (changed(m, 6, bo)) for (auto *b : m->bang) lv_obj_set_style_opa(b, (lv_opa_t) bo, 0);
  // z: x, y, opacity per instance; dots: opacity per instance
  for (int i = 0; i < Z_COUNT; i++) {
    const int zx = (int) lroundf(sum(m, PART_Z, PROP_X, i, 0)), zy = (int) lroundf(sum(m, PART_Z, PROP_Y, i, 0));
    const int zs = (int) lroundf(sum(m, PART_Z, PROP_SCALE, i, 1) * 1000);
    const int zo = (int) lroundf(sum(m, PART_Z, PROP_OPA, i, 1) * 255);
    if (changed(m, 7 + i * 3, zx) | changed(m, 8 + i * 3, zy) | changed(m, 19 + i, zs)) z_place(m, i, zx, zy, zs);
    // the whole z as one layer: its three strokes composite once (a plain opa would blend each stroke, so the
    // joints would show darker while it fades)
    if (changed(m, 9 + i * 3, zo)) lv_obj_set_style_opa_layered(m->z[i], (lv_opa_t) zo, 0);
  }
  for (int i = 0; i < DOT_COUNT; i++) {
    const int o = (int) lroundf(sum(m, PART_DOT, PROP_OPA, i, DOT_DIM) * 255);
    if (changed(m, 16 + i, o)) lv_obj_set_style_opa(m->dot[i], (lv_opa_t) o, 0);
  }
}

void tick(void *var, int32_t v) {
  Play *pl = (Play *) var;
  M *m = pl->m;
  const Pattern &P = PATTERNS[pl->pat];
  eval(m, pl->pat, v + (P.desync ? (int32_t) (m->phase % P.ms) : 0));
  apply(m);
}
void ended(lv_anim_t *a) {  // a one-shot (hop) finished: it adds nothing any more
  Play *pl = (Play *) lv_anim_get_user_data(a);
  pl->m->on[pl->pat] = false;
  apply(pl->m);
}
uint32_t pats_of(M *m) { return m->only ? m->only : MOODS[m->mood].pats; }
// (re)start the mood's patterns; arrive = play the one-shots (hop) too
void start(M *m, bool arrive) {
  const bool reduced = mo::reduced(m->root);
  for (int p = 0; p < N_PATS; p++) {
    lv_anim_delete(&m->play[p], nullptr);
    m->on[p] = false;
    const Pattern &P = PATTERNS[p];
    if (!(pats_of(m) & (1u << p))) continue;
    if (!P.loop && !arrive) continue;
    if (reduced) {
      if (P.reduced != mo::R_still) continue;  // instant: skipped
      m->on[p] = true;                         // still: the rest frame
      eval(m, p, (int32_t) (P.ms * P.rest / 1000) + (P.desync ? (int32_t) (m->phase % P.ms) : 0));
      continue;
    }
    m->on[p] = true;
    eval(m, p, P.desync ? (int32_t) (m->phase % P.ms) : 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &m->play[p]);
    lv_anim_set_exec_cb(&a, tick);
    lv_anim_set_values(&a, 0, (int32_t) P.ms);
    lv_anim_set_duration(&a, P.ms);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);  // the clock; easing per keyframe segment in sample()
    lv_anim_set_user_data(&a, &m->play[p]);
    if (P.loop) lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    else lv_anim_set_completed_cb(&a, ended);
    lv_anim_start(&a);
  }
  apply(m);
}
void look(M *m) {  // colours, eye shape, brows, extras of the current mood
  const MoodDef &md = MOODS[m->mood];
  const int k = m->k;
  const lv_color_t body = tok(m->th, m->col != SIZE_MAX ? m->col : md.colour_off), eye = tok(m->th, EYE_OFF);
  lv_obj_set_style_bg_color(m->body, body, 0);
  const Shape &e = G.eyes[md.eye];
  for (int s = 0; s < 2; s++) {
    const int x = (s ? e.rx : e.x) * k;
    lv_obj_t *o = m->eye[s], *a = m->arch[s];
    if (e.arch) {  // happy: a half ring with round ends in a w x w box (antialiased lv_arc), the rect eye hidden
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(a, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(a, x, e.y * k);
      lv_obj_set_size(a, e.w * k, e.w * k);
      lv_obj_set_style_arc_width(a, e.arch * k, 0);
      lv_obj_set_style_arc_color(a, eye, 0);
      continue;
    }
    lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(o, x, e.y * k);
    lv_obj_set_size(o, e.w * k, e.h * k);
    lv_obj_set_style_radius(o, e.r * k, 0);
    lv_obj_set_style_bg_color(o, eye, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  }
  for (int s = 0; s < 2; s++) {
    lv_obj_t *b = m->brow[s];
    if (!md.brows) { lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_remove_flag(b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(b, s ? (G.body_w - G.brow.x - G.brow.w) * k : G.brow.x * k, G.brow_y[m->mood] * k);
    lv_obj_set_style_bg_color(b, eye, 0);
    lv_obj_set_style_transform_rotation(b, (s ? -md.brow_angle : md.brow_angle) * 10, 0);
  }
  auto show = [](lv_obj_t *o, bool on) { if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); };
  for (auto *b : m->bang) show(b, md.extras & X_BANG);
  for (auto *z : m->z) show(z, md.extras & X_Z);
  for (auto *d : m->dot) { show(d, md.extras & X_DOTS); lv_obj_set_style_bg_color(d, body, 0); }
  for (auto &l : m->last) l = INT32_MIN;  // rewrite everything once
}
M *of(lv_obj_t *root) { return root ? (M *) lv_obj_get_user_data(root) : nullptr; }

// the furthest a part moves (px at scale 1) along (part, prop): max |key value| x amp over every pattern
int travel(Part pt, Prop pr) {
  float r = 0;
  for (const Pattern &P : PATTERNS)
    for (int t = 0; t < P.n_tracks; t++)
      if (P.tracks[t].part == pt && P.tracks[t].prop == pr)
        for (int j = 0; j < P.tracks[t].n; j++) r = std::max(r, fabsf(P.tracks[t].keys[j].v) * amp(P.tracks[t]));
  return (int) ceilf(r);
}
}  // namespace

int mascot_reach(int k) {
  // above: a z at the top of its rise, the "!", the body at the top of a hop; right: a z at the end of its drift, the
  // "!"; below: the dots; + the body's rotated corners (a tilt of a few degrees moves them about 2 px)
  const int up = std::max({-(G.z.y - G.z.size) + travel(PART_Z, PROP_Y) + G.z.stroke, -G.bang.y, travel(PART_BODY, PROP_Y)});
  const int right = std::max(G.z.x + G.z.size + travel(PART_Z, PROP_X) + G.z.stroke, G.bang.x + G.bang.w) - G.body_w;
  const int down = G.dots.y + G.dots.size - G.body_h;
  return (std::max({up, right, down}) + 2) * k;
}

mas::Mood mascot_mood(const char *id) {
  for (int i = 0; i < N_MOODS; i++)
    if (id && !strcmp(MOODS[i].id, id)) return (Mood) i;
  return idle;
}

lv_obj_t *mascot(lv_obj_t *parent, Mood mood, const MascotOpts &o) {
  M *m = (M *) lv_malloc_zeroed(sizeof(M));
  m->th = T;
  m->col = colour_off(o.colour);
  m->mood = mood;
  m->k = std::max(1, o.scale);
  m->only = o.only;
  m->phase = o.delay >= 0 ? (uint32_t) o.delay : (uint32_t) live * INSTANCE_OFFSET_MS;
  live++;
  const int k = m->k;
  for (int p = 0; p < N_PATS; p++) m->play[p] = {m, (uint8_t) p};
  m->root = part(parent);
  lv_obj_set_size(m->root, G.body_w * k, G.body_h * k);
  lv_obj_set_user_data(m->root, m);
  lv_obj_add_event_cb(m->root, [](lv_event_t *e) {
    M *m = (M *) lv_event_get_user_data(e);
    for (auto &p : m->play) lv_anim_delete(&p, nullptr);
    live = std::max(0, live - 1);
    lv_free(m);
  }, LV_EVENT_DELETE, m);
  m->body = part(m->root);
  lv_obj_set_size(m->body, G.body_w * k, G.body_h * k);
  lv_obj_set_style_radius(m->body, G.body_r * k, 0);
  lv_obj_set_style_bg_opa(m->body, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(m->body, G.body_w * k / 2, 0);
  m->face = part(m->body);
  lv_obj_set_size(m->face, G.body_w * k, G.body_h * k);
  for (auto &b : m->brow) {
    b = part(m->face);
    lv_obj_set_size(b, G.brow.w * k, G.brow.h * k);
    lv_obj_set_style_radius(b, G.brow.r * k, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(b, G.brow.w * k / 2, 0);
    lv_obj_set_style_transform_pivot_y(b, G.brow.h * k / 2, 0);
  }
  for (auto &e : m->eye) e = part(m->face);
  for (auto &a : m->arch) {  // happy arches: lv_arc, only the background arc drawn (no indicator, no knob)
    a = lv_arc_create(m->face);
    lv_obj_remove_style_all(a);
    lv_obj_remove_flag(a, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(a, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_arc_set_rotation(a, 0);
    lv_arc_set_bg_angles(a, 180, 360);
    lv_obj_set_style_arc_rounded(a, true, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
  }
  const lv_color_t bang = tok(m->th, BANG_OFF), zc = tok(m->th, Z_OFF);
  for (int i = 0; i < 2; i++) {  // "!" stem + dot
    lv_obj_t *b = m->bang[i] = part(m->root);
    lv_obj_set_pos(b, G.bang.x * k, (G.bang.y + (i ? G.bang.stem + G.bang.gap : 0)) * k);
    lv_obj_set_size(b, G.bang.w * k, (i ? G.bang.dot : G.bang.stem) * k);
    lv_obj_set_style_radius(b, G.bang.r * k, 0);
    lv_obj_set_style_bg_color(b, bang, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  }
  const int zw = G.z.stroke * k;
  for (int i = 0; i < Z_COUNT; i++) {  // z = 3 segments: top bar, diagonal back, bottom bar
    lv_obj_t *z = m->z[i] = lv_line_create(m->root);
    lv_obj_remove_flag(z, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(z, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_line_width(z, zw, 0);
    lv_obj_set_style_line_color(z, zc, 0);
    lv_obj_set_style_line_rounded(z, true, 0);
  }
  for (int i = 0; i < DOT_COUNT; i++) {
    lv_obj_t *d = m->dot[i] = part(m->root);
    lv_obj_set_pos(d, (G.dots.x + i * G.dots.pitch) * k, G.dots.y * k);
    lv_obj_set_size(d, G.dots.size * k, G.dots.size * k);
    lv_obj_set_style_radius(d, G.dots.r * k, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  }
  look(m);
  bleed(m->root, mascot_reach(k));
  start(m, true);  // arriving: done hops (docs: the hop plays whenever the mood becomes done)
  return m->root;
}

void mascot_set(lv_obj_t *root, Mood mood, const char *colour) {
  M *m = of(root);
  if (!m) return;
  const bool arrive = mood != m->mood;
  m->mood = mood;
  m->col = colour_off(colour);
  look(m);
  start(m, arrive);
}

void mascot_replay(lv_obj_t *root) {
  M *m = of(root);
  if (!m) return;
  look(m);
  start(m, true);
}

}  // namespace fw
