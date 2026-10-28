// toasts.cpp: the toasts' behaviour (core/toasts.h says what it is). Moved out of components/toast.cpp and
// toast_layer.cpp when they went on the contract (2026-10-28); the gestures, the timer bar and the motion are as they
// were there.
#include <algorithm>
#include <vector>
#include "fw.h"
#include "device.h"
#include "toasts.h"

namespace fw {
namespace toasts {
namespace {
using Doc = ArduinoJson::JsonDocument;

// ---- one toast
struct Toast {
  lv_obj_t *o = nullptr, *bar = nullptr;
  int bar_w = 0;
  uint32_t life = 0;
  bool timer = false, layer = false, top = toast::top, closed = false, down = false, dragged = false;
  int32_t x0 = 0, dx = 0;
};
void toast_free(lv_event_t *e) {
  auto *t = (Toast *) lv_event_get_user_data(e);
  lv_anim_delete(t->o, nullptr);
  if (t->bar) lv_anim_delete(t->bar, nullptr);
  delete t;
}
Toast *of(lv_obj_t *o) {  // the Toast rides on the root's DELETE event (the root's user data is the component's)
  if (!o) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(o); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(d) == toast_free) return (Toast *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}

void set_w(void *o, int32_t v) { lv_obj_set_width((lv_obj_t *) o, v); }
void set_tx(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *) o, v, 0); }
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }

void dismiss(Toast *t, const char *how, bool quiet = false);

// the timer bar: shrink from its current width to 0 over what is left of the life (linear, Motion easing linear)
void bar_run(Toast *t) {
  if (!t->timer || t->closed || !t->bar || t->bar_w <= 0) return;
  const int32_t w = lv_obj_get_style_width(t->bar, LV_PART_MAIN);  // the set width (no layout pass needed)
  if (w <= 0) { dismiss(t, "timeout"); return; }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, t->bar);
  lv_anim_set_exec_cb(&a, set_w);
  lv_anim_set_values(&a, w, 0);
  lv_anim_set_duration(&a, (uint32_t) ((uint64_t) t->life * w / t->bar_w));
  lv_anim_set_path_cb(&a, mo::ease::linear);
  lv_anim_set_user_data(&a, t);
  lv_anim_set_completed_cb(&a, [](lv_anim_t *a) { dismiss((Toast *) lv_anim_get_user_data(a), "timeout"); });
  lv_anim_start(&a);
}
void bar_pause(Toast *t) { if (t->bar) lv_anim_delete(t->bar, set_w); }

void gone(lv_anim_t *a) {
  auto *t = (Toast *) lv_anim_get_user_data(a);
  if (t->layer) lv_obj_send_event(t->o, LV_EVENT_READY, nullptr);  // the layer deletes it
  else lv_obj_add_flag(t->o, LV_OBJ_FLAG_HIDDEN);
}

void dismiss(Toast *t, const char *how, bool quiet) {
  if (t->closed) return;
  t->closed = true;
  bar_pause(t);
  lv_obj_remove_flag(t->o, LV_OBJ_FLAG_CLICKABLE);  // leaving: taps go through (docs pointer-events none)
  if (!quiet) emit(t->o, "dismiss", how);
  if (!quiet && (!strcmp(how, "tap") || !strcmp(how, "button") || !strncmp(how, "swipe", 5))) sound(t->o, "back");
  lv_obj_send_event(t->o, LV_EVENT_VALUE_CHANGED, (void *) how);  // the layer restacks the others now
  // leave (Motion toast-out): a swipe carries on sideways, anything else slides back to its edge; both fade
  if (!strncmp(how, "swipe", 5)) {
    const int dir = t->dx > 0 ? 1 : -1;
    animate(t->o, mo::toast_out, {set_tx, lv_obj_get_style_translate_x(t->o, LV_PART_MAIN), dir * toast::swipe_out, mo::MOVE});
    Anim f{set_opa, lv_obj_get_style_opa(t->o, LV_PART_MAIN), LV_OPA_TRANSP, mo::OPA};
    f.done = gone;
    f.user = t;
    animate(t->o, mo::toast_out, f);
  } else appear(t->o, mo::toast_out, false, t->top ? -1 : 1, 0, gone, t);
}

void spring_back(Toast *t) {
  t->down = false;
  animate(t->o, mo::toast_in, {set_tx, lv_obj_get_style_translate_x(t->o, LV_PART_MAIN), 0, mo::MOVE});
  bar_run(t);
}

void on_pointer(lv_event_t *e) {
  auto *t = (Toast *) lv_event_get_user_data(e);
  if (t->closed) return;
  lv_indev_t *in = lv_indev_active();
  lv_point_t p{};
  if (in) lv_indev_get_point(in, &p);
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      t->down = true;
      t->dragged = false;
      t->x0 = p.x - lv_obj_get_style_translate_x(t->o, LV_PART_MAIN);  // a grab during the spring-back keeps its offset
      t->dx = p.x - t->x0;
      lv_anim_delete(t->o, set_tx);
      bar_pause(t);
      break;
    case LV_EVENT_PRESSING:
      if (!t->down) break;
      t->dx = p.x - t->x0;
      if (std::abs(t->dx) >= toast::tap_slop) t->dragged = true;
      set_tx(t->o, t->dx);
      break;
    case LV_EVENT_RELEASED:
      if (!t->down) break;
      t->down = false;
      if (std::abs(t->dx) >= toast::swipe) dismiss(t, t->dx > 0 ? "swipe-right" : "swipe-left");
      else if (!t->dragged && std::abs(t->dx) < toast::tap_slop) { set_tx(t->o, 0); dismiss(t, "tap"); }
      else spring_back(t);
      break;
    case LV_EVENT_PRESS_LOST:
      if (t->down) spring_back(t);
      break;
    default: break;
  }
}

// ---- the layer
struct Layer {
  lv_obj_t *ov = nullptr;
  std::vector<lv_obj_t *> shown;  // newest first
  bool top = toast::top, still = false;
};
std::vector<Layer *> layers;  // live layers (the contrast proofs ask what a toast covers: under())
void layer_free(lv_event_t *e) {
  auto *L = (Layer *) lv_event_get_user_data(e);
  layers.erase(std::remove(layers.begin(), layers.end(), L), layers.end());
  delete L;
}
Layer *layer_of(lv_obj_t *ov) {
  if (!ov) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(ov); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(ov, i);
    if (lv_event_dsc_get_cb(d) == layer_free) return (Layer *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
// slot i's top in the overlay's own box (a caller may give it any size: the screen, or a workbench frame)
int slot_y(const Layer *L, int i) {
  const int step = lay::toast_h + lay::gutter;
  return L->top ? lay::toast_top + i * step : lv_obj_get_height(L->ov) - lay::inset - lay::toast_h - i * step;
}
void set_y(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *) o, v); }
// every shown toast to its slot (Motion toast-in on y: the docs' CSS transition on top)
void restack(Layer *L) {
  for (size_t i = 0; i < L->shown.size(); i++) {
    lv_obj_t *t = L->shown[i];
    const int32_t from = lv_obj_get_y(t), to = slot_y(L, (int) i);
    if (from == to) continue;
    if (L->still) { lv_obj_set_y(t, to); continue; }
    animate(t, mo::toast_in, {set_y, from, to, mo::MOVE});
  }
}
}  // namespace

void own(lv_obj_t *o) {
  auto *t = new Toast;
  t->o = o;
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, toast_free, LV_EVENT_DELETE, t);
  for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(o, on_pointer, c, t);
  // the panel's hardware key (core/device.h, Contextual): dismisses the newest toast shown (reason "button")
  dev::claim_button(o, dev::BUTTON_ALERT, [](void *u, const char *press) {
    auto *t = (Toast *) u;
    if (t->closed || strcmp(press, "single")) return false;
    dismiss(t, "button");
    return true;
  }, t);
  // pushed out by a newer toast (the layer; param 1 = a quiet push: no event)
  lv_obj_add_event_cb(o, [](lv_event_t *e) { dismiss((Toast *) lv_event_get_user_data(e), "pushed", lv_event_get_param(e) != nullptr); }, LV_EVENT_CANCEL, t);
}

int32_t left(lv_obj_t *o, bool timer, bool urgent) {
  Toast *t = of(o);
  if (!t) return -1;
  bar_pause(t);
  const uint32_t life = urgent ? toast::life_urgent : toast::life;
  if (!t->bar || timer != t->timer || life != t->life || t->closed) return -1;
  return lv_obj_get_style_width(t->bar, LV_PART_MAIN);
}

namespace {
void place(Toast *t, lv_obj_t *bar, bool timer, bool urgent, int32_t keep_w) {
  bar_pause(t);
  lv_anim_delete(t->o, set_tx);
  t->timer = timer;
  t->life = urgent ? toast::life_urgent : toast::life;
  t->closed = t->down = t->dragged = false;
  t->dx = 0;
  lv_obj_add_flag(t->o, LV_OBJ_FLAG_CLICKABLE);
  set_tx(t->o, 0);
  // the bar along the bottom inside the edge (children are placed from the content box: step back by pad + edge)
  t->bar = bar;
  if (!bar) return;
  lv_obj_update_layout(t->o);
  const int sl = lv_obj_get_style_space_left(t->o, LV_PART_MAIN), st = lv_obj_get_style_space_top(t->o, LV_PART_MAIN);
  t->bar_w = lv_obj_get_width(t->o) - 2 * stroke::hair;
  qa::free(bar, "the timer bar shrinks with the time left");
  lv_obj_add_flag(bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(bar, keep_w >= 0 ? std::min<int32_t>(keep_w, t->bar_w) : t->bar_w, stroke::edge);
  lv_obj_set_pos(bar, stroke::hair - sl, lv_obj_get_height(t->o) - stroke::hair - stroke::edge - st);
}
}  // namespace

void arm(lv_obj_t *o, lv_obj_t *bar, bool timer, bool urgent) {
  Toast *t = of(o);
  if (!t) return;
  place(t, bar, timer, urgent, -1);
  bar_run(t);
}

void again(lv_obj_t *o, lv_obj_t *bar, bool timer, bool urgent, int32_t keep_w) {
  Toast *t = of(o);
  if (!t) return;
  const bool was_closed = t->closed;
  place(t, bar, timer, urgent, keep_w);
  if (was_closed) {  // a dismissed toast comes back in place (Motion toast-in from its edge, silent)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    appear(o, mo::toast_in, true, t->top ? -1 : 1);
  }
  bar_run(t);
}

void action(lv_obj_t *o, lv_obj_t *button) {
  if (Toast *t = of(o))
    lv_obj_add_event_cb(button, [](lv_event_t *e) { dismiss((Toast *) lv_event_get_user_data(e), "action"); }, LV_EVENT_CLICKED, t);
}

lv_obj_t *layer(lv_obj_t *host, bool top, bool still) {
  auto *L = new Layer;
  L->top = top;
  L->still = still;
  L->ov = lv_obj_create(host);
  lv_obj_remove_style_all(L->ov);
  lv_obj_remove_flag(L->ov, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(L->ov, (lv_obj_flag_t) (LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_USER_4));
  lv_obj_set_size(L->ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(L->ov, 0, 0);
  qa::free(L->ov, "overlay: toast layer");  // sits over the screen on purpose (tools/audit_screens.mjs skips it)
  lv_obj_add_event_cb(L->ov, layer_free, LV_EVENT_DELETE, L);
  layers.push_back(L);
  return L->ov;
}

lv_obj_t *show(lv_obj_t *ov, Props p, const char *detail) {
  Layer *L = layer_of(ov);
  const Component *c = find("toast");
  if (!L || !c) return nullptr;
  Doc d;
  d.set(p);
  d["layer"] = true;
  if (L->still) d["timer"] = false;
  const uint32_t n = lv_obj_get_child_count(ov);
  c->build(ov, d.as<Props>());
  if (lv_obj_get_child_count(ov) <= n) return nullptr;
  lv_obj_t *t = lv_obj_get_child(ov, -1);
  if (Toast *s = of(t)) s->layer = true, s->top = L->top;
  lv_obj_add_flag(t, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_width(t, lv_obj_get_width(ov) - 2 * lay::inset);  // inset to inset (the content width on a screen)
  lv_obj_set_pos(t, lay::inset, slot_y(L, 0));
  if (Toast *s = of(t)) arm(t, s->bar, s->timer, s->life == toast::life_urgent);  // the bar again at the toast's width
  // dismissed: out of the stack, the rest move up; gone (its leave finished): delete it
  lv_obj_add_event_cb(t, [](lv_event_t *e) {
    auto *L = (Layer *) lv_event_get_user_data(e);
    auto *t = (lv_obj_t *) lv_event_get_current_target(e);
    for (size_t i = 0; i < L->shown.size(); i++)
      if (L->shown[i] == t) { L->shown.erase(L->shown.begin() + i); restack(L); break; }
  }, LV_EVENT_VALUE_CHANGED, L);
  lv_obj_add_event_cb(t, [](lv_event_t *e) { lv_obj_delete_async((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_READY, nullptr);
  L->shown.insert(L->shown.begin(), t);
  // over the max: the oldest leaves (it sends VALUE_CHANGED, which takes it out of `shown`)
  while ((int) L->shown.size() > toast::max) {
    lv_obj_t *old = L->shown.back();
    L->shown.pop_back();
    lv_obj_send_event(old, LV_EVENT_CANCEL, detail ? nullptr : (void *) 1);
  }
  restack(L);
  if (!L->still) {
    lv_obj_set_style_opa(t, LV_OPA_TRANSP, 0);
    appear(t, mo::toast_in, true, L->top ? -1 : 1);
  }
  if (!L->still && detail) {
    emit(t, "toast", detail);
    sound(t, toast::sound);
  }
  return t;
}

bool under(lv_obj_t *o) {
  lv_area_t a, b;
  lv_obj_get_coords(o, &a);
  for (Layer *L : layers) {
    if (lv_obj_get_screen(L->ov) != lv_obj_get_screen(o) || lv_obj_has_flag(L->ov, LV_OBJ_FLAG_HIDDEN)) continue;
    for (lv_obj_t *t : L->shown) {
      if (t == o || lv_obj_has_flag(t, LV_OBJ_FLAG_HIDDEN)) continue;
      bool inside = false;
      for (lv_obj_t *p = lv_obj_get_parent(o); p && !inside; p = lv_obj_get_parent(p)) inside = p == t;
      if (inside) continue;
      lv_obj_get_coords(t, &b);
      if (a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2) return true;
    }
  }
  return false;
}

void clear(lv_obj_t *ov) {
  Layer *L = layer_of(ov);
  if (!L) return;
  while (!L->shown.empty()) {  // every toast showing leaves quietly (it takes itself out of `shown`)
    lv_obj_t *old = L->shown.back();
    L->shown.pop_back();
    lv_obj_send_event(old, LV_EVENT_CANCEL, (void *) 1);
  }
}

void set_top(lv_obj_t *ov, bool top) {
  if (Layer *L = layer_of(ov))
    if (L->shown.empty()) L->top = top;
}

// The device's toast layer: core/device.cpp puts one over every screen of a mounted device (on the display's top
// layer) and pushes the toasts Home Assistant sends (fw::dev::toast): event "toast" + sound notify, timer on.
void push(lv_obj_t *ov, Props t, bool top) {
  if (!layer_of(ov)) return;
  set_top(ov, top);
  // an unknown tone = info; an unknown or empty icon = the tone's own (ok done, warn / error alert, working agent, else bell)
  static const char *TONES[] = {"ok", "warn", "error", "working", "info", "idle"};
  static const char *ICONS[] = {"timer", "agent", "person", "washer", "vacuum", "bolt", "alert", "done", "bell", "check", "pr"};
  auto known = [](const char *v, const char *const *list, size_t n) -> const char * {
    for (size_t i = 0; v && i < n; i++) if (!strcmp(v, list[i])) return list[i];
    return nullptr;
  };
  const char *tn = known(t["tone"] | "", TONES, sizeof TONES / sizeof *TONES);
  const std::string tone = tn ? tn : "info";
  const char *icon = tone == "ok" ? "done" : tone == "warn" || tone == "error" ? "alert" : tone == "working" ? "agent" : "bell";
  const char *ic = known(t["icon"] | "", ICONS, sizeof ICONS / sizeof *ICONS);
  const bool urgent = !strcmp(t["life"] | "normal", "urgent");
  Doc d;
  d["tone"] = tone, d["icon"] = ic ? ic : icon, d["title"] = t["title"] | "", d["body"] = t["body"] | "";
  d["action"] = t["action"] | "", d["life"] = urgent ? "urgent" : "normal", d["timer"] = true;
  // detail "<tone> <icon> <life>: <title>" (functional tests read what arrived; toast_ha.test.mjs)
  char det[160];
  snprintf(det, sizeof det, "%s %s %s: %s", tone.c_str(), ic ? ic : icon, urgent ? "urgent" : "normal", t["title"] | "");
  show(ov, d.as<Props>(), det);
}
}  // namespace toasts
}  // namespace fw
