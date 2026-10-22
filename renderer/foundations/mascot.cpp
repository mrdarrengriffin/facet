// Foundations > Mascots: tokens/mascot.json (via the generated mascot_gen.h) drawn and played by the real mascot code
// (core/mascot.cpp, fw::mascot): every sample here is a live fw::mascot, the 4x views are the same code with every
// part and amplitude x 4. Sections: moods (true size on the canvas + on a tile surface, and 4x; tap = replay the
// arrival), motion (each pattern alone on the mood that uses it, timing + reduced rule; tap = replay), desync (six
// mascots with the instance delay), tokens (both densities + the off-grid decisions), usage (where + rules, examples).
// Playground: colour (override every mood's colour token), reduced_motion (fw::render), bench = a full-screen view for
// the panels' frame stats (GET /fw/perf): agents = a 3 x 3 Agents grid at true size, big = six mascots at 4x.
// Status (WIP/Done) = this file + tokens/mascot.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)
constexpr int BIG = 4;    // the magnified view: every part x 4

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
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(r, gap, 0);
  lv_obj_set_style_pad_row(r, gap, 0);
  return r;
}
void fill(lv_obj_t *o, lv_color_t c) {
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}
void outline(lv_obj_t *o, lv_color_t c, int w) {
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s2) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  outline(e, T->line, stroke::hair);
  return e;
}
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  return l;
}
void free_on_delete(lv_obj_t *o, void *mem) {
  lv_obj_add_event_cb(o, [](lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, mem);
}
int grid4(int v) { return (v + 3) / 4 * 4; }

// a group of mascots replayed together by one tap
struct Group { lv_obj_t *m[8]; int n; };
Group *group(lv_obj_t *owner) {
  auto *g = (Group *) lv_malloc_zeroed(sizeof(Group));
  free_on_delete(owner, g);
  lv_obj_add_flag(owner, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(owner, [](lv_event_t *e) {
    auto *g = (Group *) lv_event_get_user_data(e);
    for (int i = 0; i < g->n; i++) mascot_replay(g->m[i]);
  }, LV_EVENT_CLICKED, g);
  return g;
}
// a cell with room for the extras + motion around the body, the mascot centred; surface = on a tile surface
lv_obj_t *cell(lv_obj_t *parent, Group *g, mas::Mood mood, int k, bool surface, const MascotOpts &base, const char *caption) {
  lv_obj_t *c = column(parent, sp::s2, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *b = box(c);
  const int r = mascot_reach(k);
  lv_obj_set_size(b, grid4(mas::G.body_w * k + 2 * r), grid4(mas::G.body_h * k + 2 * r));
  if (surface) { fill(b, T->surface); outline(b, T->line, stroke::hair); }
  MascotOpts o = base;
  o.scale = k;
  lv_obj_t *m = mascot(b, mood, o);
  lv_obj_center(m);
  if (g && g->n < 8) g->m[g->n++] = m;
  if (caption) text(c, ty::c_meta_sm, caption, T->text_dim);
  return m;
}
const char *ms_text(uint32_t ms, char *b, size_t n) {
  if (ms >= 1000 && ms % 100 == 0) snprintf(b, n, "%.1f s", ms / 1000.0);
  else snprintf(b, n, "%u ms", (unsigned) ms);
  return b;
}

// ---------------------------------------------------------------- moods
void moods(lv_obj_t *l, const char *colour) {
  for (int i = 0; i < mas::N_MOODS; i++) {
    const mas::MoodDef &md = mas::MOODS[i];
    lv_obj_t *e = entry(l, sp::s3);
    lv_obj_t *h = row(e, sp::s4);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(h, ty::c_caps_sm, md.label, T->text);
    char b[160];
    snprintf(b, sizeof b, "colour %s · eyes %s", colour ? colour : md.colour, mas::EYE_DEFS[md.eye].id);
    text(h, ty::c_meta, b, T->text_dim);
    text(e, ty::c_meta, md.what, T->text_dim, inner_w());
    lv_obj_t *r = row(e, sp::s5);
    Group *g = group(e);
    MascotOpts o;
    o.colour = colour;
    o.delay = i * (int) mas::INSTANCE_OFFSET_MS;
    cell(r, g, (mas::Mood) i, 1, false, o, "canvas");
    cell(r, g, (mas::Mood) i, 1, true, o, "tile surface");
    cell(r, g, (mas::Mood) i, BIG, false, o, "4x · tap to replay");
  }
}

// ---------------------------------------------------------------- motion
mas::Mood mood_for(int p) {  // the first mood that plays pattern p (bob: none yet -> idle)
  for (int i = 0; i < mas::N_MOODS; i++)
    if (mas::MOODS[i].pats & (1u << p)) return (mas::Mood) i;
  return mas::idle;
}
void motion(lv_obj_t *l, bool reduced) {
  for (int p = 0; p < mas::N_PATS; p++) {
    const mas::Pattern &P = mas::PATTERNS[p];
    lv_obj_t *e = entry(l, sp::s2);
    lv_obj_t *h = row(e, sp::s4);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(h, ty::c_caps_sm, P.label, T->text);
    char b[240], t[32];
    snprintf(b, sizeof b, "%s · %s · %s%s", ms_text(P.ms, t, sizeof t), P.easing_name, P.loop ? "loop" : "once", P.desync ? " · desynced" : "");
    text(h, ty::c_meta, b, T->text_dim);
    if (P.stagger) {
      snprintf(b, sizeof b, "each a further %s", ms_text(P.stagger, t, sizeof t));
      text(h, ty::c_meta, b, T->text_dim);
    }
    text(e, ty::c_meta, P.animates, T->text_dim, inner_w());
    if (!strcmp(P.reduced_name, "still")) snprintf(b, sizeof b, "Used by %s. Reduced motion: still, the frame at %d %% of the loop.", P.use, P.rest / 10);
    else snprintf(b, sizeof b, "Used by %s. Reduced motion: %s (%s).", P.use, P.reduced_name, P.loop ? "stopped" : "skipped");
    text(e, ty::c_meta_sm, b, reduced ? T->text : T->text_dim, inner_w());
    lv_obj_t *r = row(e, sp::s5);
    Group *g = group(e);
    MascotOpts o;
    o.delay = 0;
    o.only = 1u << p;
    cell(r, g, mood_for(p), 1, false, o, "true size");
    cell(r, g, mood_for(p), BIG, false, o, "4x · tap to replay");
  }
}

// ---------------------------------------------------------------- desync
void desync(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s3);
  char b[200];
  snprintf(b, sizeof b, "Instance delay: the n-th mascot starts its desynced loops (blink, glance, bob, breathe) n x %u ms into the cycle.", (unsigned) mas::INSTANCE_OFFSET_MS);
  text(e, ty::c_meta, b, T->text_dim, inner_w());
  lv_obj_t *r = row(e, sp::s4);
  Group *g = group(e);
  for (int i = 0; i < 6; i++) {
    MascotOpts o;
    o.delay = i * (int) mas::INSTANCE_OFFSET_MS;
    char c[24], t[16];
    snprintf(c, sizeof c, "+%s", ms_text((uint32_t) o.delay, t, sizeof t));
    cell(r, g, mas::idle, 2, false, o, i ? c : "+0");
  }
  text(e, ty::c_meta_sm, "2x · tap to restart them together (each keeps its delay)", T->text_dim);
}

// ---------------------------------------------------------------- tokens
void trow(lv_obj_t *l, const char *name, const char *v, const char *x1, lv_color_t c) {
  lv_obj_t *r = box(l);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(r, sp::s4, 0);
  lv_obj_set_style_pad_ver(r, sp::s1, 0);
  const int nw = inner_w() * 3 / 10, vw = (inner_w() - nw - 2 * sp::s4) / 2;
  lv_obj_set_width(text(r, ty::c_meta, name, c, nw), nw);
  lv_obj_set_width(text(r, ty::c_meta, v, c, vw), vw);
  lv_obj_set_width(text(r, ty::c_meta, x1, c, vw), vw);
}
void head(lv_obj_t *l, const char *what) {
  lv_obj_t *e = entry(l, sp::s1);
  lv_obj_set_style_pad_bottom(e, sp::s1, 0);
  text(e, ty::c_caps_sm, what, T->text);
}
void shape_txt(const mas::Shape &s, char *b, size_t n) {
  if (s.arch) snprintf(b, n, "x %d / %d y %d · arch %d wide, %d px ring", s.x, s.rx, s.y, s.w, s.arch);
  else snprintf(b, n, "x %d / %d y %d · %d x %d · r %d", s.x, s.rx, s.y, s.w, s.h, s.r);
}
void tokens_page(lv_obj_t *l) {
  char a[96], b[96];
  const mas::Geo &V = mas::GEO_V, &X = mas::GEO_1X;
  lv_obj_t *e = entry(l, sp::s2);
  text(e, ty::c_caps_sm, "The mascot follows the foundations", T->text);
  text(e, ty::c_meta, mas::GRID_RULE, T->text_dim, inner_w());
  for (int i = 0; i < mas::N_FOUNDATIONS; i++) text(e, ty::c_meta, mas::FOUNDATIONS[i], T->text, inner_w());
  e = entry(l, sp::s1);
  trow(e, "token", "720 (v)", "480 (1x)", T->text_dim);
  head(l, "Body");
  e = column(l, 0);
  snprintf(a, sizeof a, "%d x %d · r %d", V.body_w, V.body_h, V.body_r);
  snprintf(b, sizeof b, "%d x %d · r %d", X.body_w, X.body_h, X.body_r);
  trow(e, "body (w x h, radius)", a, b, T->text);
  snprintf(a, sizeof a, "%d", V.blink_h);
  snprintf(b, sizeof b, "%d", X.blink_h);
  trow(e, "blink-h", a, b, T->text);
  head(l, "Eyes (left / right x)");
  e = column(l, 0);
  for (int i = 0; i < mas::N_EYES; i++) {
    shape_txt(V.eyes[i], a, sizeof a);
    shape_txt(X.eyes[i], b, sizeof b);
    trow(e, mas::EYE_DEFS[i].id, a, b, T->text);
  }
  head(l, "Brows and extras");
  e = column(l, 0);
  snprintf(a, sizeof a, "x %d · %d x %d · round", V.brow.x, V.brow.w, V.brow.h);
  snprintf(b, sizeof b, "x %d · %d x %d · round", X.brow.x, X.brow.w, X.brow.h);
  trow(e, "brow", a, b, T->text);
  for (int i = 0; i < mas::N_MOODS; i++) {
    if (!mas::MOODS[i].brows) continue;
    char n[48];
    snprintf(n, sizeof n, "brow %s", mas::MOODS[i].id);
    snprintf(a, sizeof a, "y %d · %+d deg", V.brow_y[i], mas::MOODS[i].brow_angle);
    snprintf(b, sizeof b, "y %d · %+d deg", X.brow_y[i], mas::MOODS[i].brow_angle);
    trow(e, n, a, b, T->text);
  }
  snprintf(a, sizeof a, "x %d y %d · %d x %d+%d+%d", V.bang.x, V.bang.y, V.bang.w, V.bang.stem, V.bang.gap, V.bang.dot);
  snprintf(b, sizeof b, "x %d y %d · %d x %d+%d+%d", X.bang.x, X.bang.y, X.bang.w, X.bang.stem, X.bang.gap, X.bang.dot);
  trow(e, "\"!\" (stem+gap+dot)", a, b, T->text);
  snprintf(a, sizeof a, "x %d y %d · %d px · stroke %d", V.z.x, V.z.y, V.z.size, V.z.stroke);
  snprintf(b, sizeof b, "x %d y %d · %d px · stroke %d", X.z.x, X.z.y, X.z.size, X.z.stroke);
  trow(e, "z (x 3)", a, b, T->text);
  snprintf(a, sizeof a, "x %d y %d · %d px · pitch %d", V.dots.x, V.dots.y, V.dots.size, V.dots.pitch);
  snprintf(b, sizeof b, "x %d y %d · %d px · pitch %d", X.dots.x, X.dots.y, X.dots.size, X.dots.pitch);
  trow(e, "progress dots (x 3)", a, b, T->text);
  snprintf(a, sizeof a, "%d px each side", mascot_reach(1));
  trow(e, "reach (bleed)", a, "this build", T->text);

  head(l, "Colours");
  e = column(l, 0);
  for (int i = 0; i < mas::N_MOODS; i++) trow(e, mas::MOODS[i].id, mas::MOODS[i].colour, "body, dots", T->text);
  trow(e, "eyes, brows", mas::EYE_COLOUR, "cut-outs", T->text);
  trow(e, "\"!\"", mas::BANG_COLOUR, "needs you", T->text);
  trow(e, "z", mas::Z_COLOUR, "sleeping", T->text);
  snprintf(a, sizeof a, "%.2f", (double) mas::DOT_DIM);
  trow(e, "opacity dot-dim", a, "unlit dot", T->text);

  head(l, "Motion");
  e = column(l, 0);
  for (int p = 0; p < mas::N_PATS; p++) {
    const mas::Pattern &P = mas::PATTERNS[p];
    char t[16];
    snprintf(a, sizeof a, "%s · %s · %s", ms_text(P.ms, t, sizeof t), P.easing_name, P.loop ? (P.desync ? "loop, desynced" : "loop") : "once");
    snprintf(b, sizeof b, "reduced %s%s", P.reduced_name, P.stagger ? " · staggered" : "");
    trow(e, P.id, a, b, T->text);
    for (int k = 0; k < P.n_tracks; k++) {
      static const char *PART[] = {"body", "face", "eyes", "\"!\"", "z", "dot"}, *PROP[] = {"x", "y", "height", "opacity", "rotation", "close", "scale"};  // = mas::Prop (ASan 2026-10-22: "scale" was missing, read past the end)
      const mas::Track &tr = P.tracks[k];
      char n[48];
      snprintf(n, sizeof n, "  %s %s", PART[tr.part], PROP[tr.prop]);
      if (tr.prop <= mas::PROP_H) { snprintf(a, sizeof a, "amp %d px · %d keys", tr.amp_v, tr.n); snprintf(b, sizeof b, "amp %d px", tr.amp_1x); }
      else { snprintf(a, sizeof a, "%d keys", tr.n); snprintf(b, sizeof b, "%s", tr.prop == mas::PROP_ROT ? "degrees" : "0..1"); }
      trow(e, n, a, b, T->text_dim);
    }
  }
  snprintf(a, sizeof a, "%u ms", (unsigned) mas::INSTANCE_OFFSET_MS);
  trow(e, "delay instance-offset", a, "per mascot", T->text);

  head(l, "Working variants");
  e = column(l, 0);
  for (int i = 0; i < mas::N_WORK; i++) trow(e, mas::WORK[i].label, mas::WORK[i].status, strcmp(mas::WORK[i].id, mas::WORK_DEFAULT) ? "" : "default", T->text);

  head(l, "Changed from the old mascot (old -> new, why)");
  e = column(l, sp::s2);
  for (int i = 0; i < mas::N_CHANGES; i++) text(e, ty::c_meta, mas::CHANGES[i], T->text_dim, inner_w());
  lv_obj_set_style_pad_bottom(e, sp::s5, 0);
}

// ---------------------------------------------------------------- usage
lv_obj_t *coming(lv_obj_t *parent, const char *what, int w, int h) {  // a part not built yet: plain labelled outline
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, w, h);
  outline(o, T->line_hi, stroke::hair);
  char b[64];
  snprintf(b, sizeof b, "%s: component coming", what);
  lv_obj_t *t = text(o, ty::c_meta_sm, b, T->text_dim, w - 2 * lay::tile_pad);
  lv_obj_align(t, LV_ALIGN_BOTTOM_LEFT, lay::tile_pad, -lay::tile_pad);
  return o;
}
void usage(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s2);
  text(e, ty::c_caps_sm, "Where", T->text);
  for (int i = 0; i < mas::N_WHERE; i++) text(e, ty::c_meta, mas::WHERE[i], T->text_dim, inner_w());
  e = entry(l, sp::s2);
  text(e, ty::c_caps_sm, "Rules", T->text);
  for (int i = 0; i < mas::N_RULES; i++) text(e, ty::c_meta, mas::RULES[i], T->text_dim, inner_w());
  e = entry(l, sp::s3);
  text(e, ty::c_caps_sm, "Agents tiles", T->text);
  lv_obj_t *r = row(e, lay::gutter);
  const mas::Mood ms[] = {mas::working, mas::needs, mas::sleeping};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *t = coming(r, "tile", lay::tile_w, lay::tile_h);
    MascotOpts o;
    o.delay = i * (int) mas::INSTANCE_OFFSET_MS;
    lv_obj_t *m = mascot(t, ms[i], o);
    lv_obj_set_pos(m, lay::tile_pad, lay::tile_pad);
  }
  e = entry(l, sp::s3);
  text(e, ty::c_caps_sm, "Empty state", T->text);
  lv_obj_t *t = coming(e, "empty state", inner_w(), lay::tile_h);
  lv_obj_t *m = mascot(t, mas::idle, {});
  lv_obj_align(m, LV_ALIGN_CENTER, 0, -lay::tile_pad);
}

// ---------------------------------------------------------------- bench (full screen, for /fw/perf)
void bench(lv_obj_t *parent, const char *what) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, lay::screen_w, lay::screen_h);
  if (!strcmp(what, "big")) {  // six mascots at 4x, each mood
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    for (int i = 0; i < mas::N_MOODS; i++) {
      lv_obj_t *c = box(s);
      lv_obj_set_size(c, lay::screen_w / 3 - lay::gutter, lay::screen_h / 2 - lay::gutter);
      MascotOpts o;
      o.scale = BIG;
      lv_obj_center(mascot(c, (mas::Mood) i, o));
    }
    return;
  }
  // agents: 3 x 3 tile outlines at the tile tokens, a mascot in each (moods cycling), like the Agents screen
  for (int i = 0; i < 9; i++) {
    lv_obj_t *t = box(s);
    lv_obj_set_size(t, lay::tile_w, lay::tile_h);
    lv_obj_set_pos(t, lay::inset + (i % 3) * (lay::tile_w + lay::gutter), lay::inset + (i / 3) * (lay::tile_h + lay::gutter));
    fill(t, T->surface);
    outline(t, T->line, stroke::hair);
    lv_obj_t *m = mascot(t, (mas::Mood) (i % mas::N_MOODS), {});
    lv_obj_set_pos(m, lay::tile_pad, lay::tile_pad);
  }
}
}  // namespace

FW_COMPONENT(mascots, "Mascots", "foundations", R"({
  "about": "Small animated characters that show the mood of a card or screen, with their colours, motion and usage rules.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "type": "select", "options": )" FW_MASCOT_SECTION_IDS R"(, "default": "moods"},
    {"name": "colour", "type": "select", "options": )" FW_MASCOT_COLOURS R"(, "default": "mood"},
    {"name": "reduced_motion", "type": "bool", "default": false},
    {"name": "bench", "type": "select", "options": ["off", "agents", "big"], "default": "off"}
  ],
  "variants": )" FW_MASCOT_SECTIONS R"(
})") {
  const char *s = p["section"] | "moods";
  const char *b = p["bench"] | "off";
  const char *c = p["colour"] | "mood";
  if (strcmp(b, "off")) { bench(parent, b); return; }
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "motion")) motion(l, mo::reduced(parent));
  else if (!strcmp(s, "desync")) desync(l);
  else if (!strcmp(s, "tokens")) tokens_page(l);
  else if (!strcmp(s, "usage")) usage(l);
  else moods(l, strcmp(c, "mood") ? c : nullptr);
}
