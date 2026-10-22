// Motion runtime (core/fw.h "Motion", README "Motion tokens"): reduced motion per display, pattern durations, style
// transitions for colour patterns, and the animate / appear / rise helpers. Tokens: tokens/motion.json (motion_gen.h).
#include "fw.h"

namespace fw {
namespace mo {
namespace {
struct Flag { const lv_display_t *d; bool on; bool fades; };
Flag flags[16];
int n_flags = 0;
bool global = false;
#ifdef FW_1X
constexpr bool FADES_DEFAULT = false;  // Guition (core/fw.h fades)
#else
constexpr bool FADES_DEFAULT = true;
#endif
Flag *flag_of(const lv_display_t *d) {
  for (int i = 0; i < n_flags; i++)
    if (flags[i].d == d) return &flags[i];
  return nullptr;
}
}  // namespace

bool fades_default() { return FADES_DEFAULT; }
void set_fades(lv_display_t *d, bool on) {
  if (!flag_of(d)) set_reduced(d, global);
  if (Flag *f = flag_of(d)) f->fades = on;
}
bool fades(const lv_obj_t *o) {
  const Flag *f = flag_of(o ? lv_obj_get_display(o) : nullptr);
  return f ? f->fades : FADES_DEFAULT;
}

void set_reduced(lv_display_t *d, bool on) {
  for (int i = 0; i < n_flags; i++)
    if (flags[i].d == d) { flags[i].on = on; return; }
  if (n_flags < 16) flags[n_flags++] = {d, on, FADES_DEFAULT};
  else flags[15] = {d, on, FADES_DEFAULT};
}
bool reduced(const lv_obj_t *o) {
  const lv_display_t *d = o ? lv_obj_get_display(o) : nullptr;
  for (int i = 0; i < n_flags; i++)
    if (flags[i].d == d) return flags[i].on;
  return global;
}
void set_reduced_default(bool on) { global = on; }
bool reduced_default() { return global; }

uint32_t duration(const lv_obj_t *o, Id p, Channel ch) {
  const Pattern &k = PATTERNS[p];
  if (!reduced(o)) return k.ms;
  switch (k.reduced) {
    case R_keep: return k.ms;
    case R_fade: return ch == OPA ? (k.ms < REDUCED_FADE_MS ? k.ms : REDUCED_FADE_MS) : 0;
    default: return 0;  // instant, still
  }
}

void transition(lv_style_transition_dsc_t *d, Id p, const lv_style_prop_t *props) {
  // colour patterns only (reduced mode keep): the same in both settings, so one static descriptor is enough
  lv_style_transition_dsc_init(d, props, path(p), PATTERNS[p].ms, 0, nullptr);
}
}  // namespace mo

void animate(lv_obj_t *o, mo::Id p, const Anim &x) {
  lv_anim_delete(o, x.exec);
  const uint32_t ms = mo::duration(o, p, x.ch);
  if (!ms || (x.loop && mo::reduced(o))) {  // instant (or a loop with reduced motion: resting frame)
    x.exec(o, x.loop ? x.from : x.to);
    if (x.done) {
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, o);
      lv_anim_set_user_data(&a, x.user);
      x.done(&a);
    }
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, x.exec);
  lv_anim_set_values(&a, x.from, x.to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, x.delay);
  lv_anim_set_path_cb(&a, mo::path(p));
  if (x.loop) {
    lv_anim_set_reverse_duration(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  }
  if (x.done) lv_anim_set_completed_cb(&a, x.done);
  lv_anim_set_user_data(&a, x.user);
  if (x.delay && x.ch == mo::OPA) x.exec(o, x.from);  // hold the start look while waiting (rise rows stay hidden)
  lv_anim_start(&a);
}

namespace {
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *) o, v, 0); }
}  // namespace

void appear(lv_obj_t *o, mo::Id p, bool in, int dir, uint32_t delay, lv_anim_completed_cb_t done, void *user) {
  const int off = dir * mo::PATTERNS[p].offset;
  int32_t opa = lv_obj_get_style_opa(o, LV_PART_MAIN), ty = lv_obj_get_style_translate_y(o, LV_PART_MAIN);
  if (in && opa == LV_OPA_TRANSP) ty = off;  // hidden: come in from the offset
  if (in && delay) { set_opa(o, opa); set_ty(o, ty); }
  if (off || ty) {
    Anim m{set_ty, ty, in ? 0 : off, mo::MOVE, delay};
    if (mo::duration(o, p, mo::MOVE)) animate(o, p, m);
    else { lv_anim_delete(o, set_ty); set_ty(o, 0); }  // reduced: no movement at all
  }
  Anim f{set_opa, opa, in ? LV_OPA_COVER : LV_OPA_TRANSP, mo::OPA, delay};
  f.done = done;
  f.user = user;
  animate(o, p, f);
}

void rise(lv_obj_t *const *items, int n, bool home) {
  // Facet "Content rise" (old lab screens.ts T.rise = @facet/ui device.ts): the new screen is cut in, then its blocks
  // are grouped into rows by their top edge (blocks side by side, e.g. two Home cards, rise together), the header
  // (items[0]) is always the first row; each row fades in from offset px below (above when going Home) over the
  // pattern's duration, row i starting min(i, steps - 1) x stagger later, hidden until its start (fill backwards).
  const mo::Pattern &k = mo::PATTERNS[mo::screen];
  if (n <= 0) return;
  lv_obj_update_layout(lv_obj_get_screen(items[0]));
  std::vector<std::pair<int32_t, lv_obj_t *>> blocks;  // (top, item) of the content blocks, header excluded
  for (int i = 1; i < n; i++) {
    lv_area_t a;
    lv_obj_get_coords(items[i], &a);
    blocks.push_back({a.y1 - lv_obj_get_style_translate_y(items[i], LV_PART_MAIN), items[i]});
  }
  std::stable_sort(blocks.begin(), blocks.end(), [](auto &x, auto &y) { return x.first < y.first; });
  const bool instant = !mo::duration(items[0], mo::screen, mo::OPA) || !mo::fades(items[0]);  // reduced motion / no fades: instant
  int row = 0;
  auto start = [&](lv_obj_t *r, int i) {
    lv_anim_delete(r, nullptr);
    if (instant) { lv_obj_set_style_opa(r, LV_OPA_COVER, 0); lv_obj_set_style_translate_y(r, 0, 0); return; }
    lv_obj_set_style_opa(r, LV_OPA_TRANSP, 0);  // every block starts hidden at its offset
    lv_obj_set_style_translate_y(r, home ? -k.offset : k.offset, 0);
    appear(r, mo::screen, true, home ? -1 : 1, (uint32_t) std::min(i, k.steps - 1) * k.stagger);
  };
  start(items[0], row++);
  for (size_t i = 0; i < blocks.size(); i++) {
    if (i && blocks[i].first != blocks[i - 1].first) row++;
    start(blocks[i].second, row);
  }
}
}  // namespace fw
