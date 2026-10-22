// Toast: one Facet notification (docs: Components > Toasts > Toast, @facet/ui toast.ts toast(), CSS .f-toast): a
// message over any screen with its tone, icon, title, body, an optional action and a timer bar.
//   box      content width (lay::content_w, inset to inset) x lay::toast_h (120 / 80), surface fill, 1 px line-hi
//            edge, rad::card corners (square: cards and surfaces), padding toast-pad - hair (the edge makes it
//            toast-pad); flex row, centred, gap toast-pad (24 / 16)
//   icon     a lay::toast_icon square (48 / 32, rad::button) filled with the tone at chip.tint (colour.json, docs
//            opacity chip-bg 0.14; = mix chip-<tone>-surface), the icon at ic::glyph (32 / 20, icons.json "toasts")
//            centred in it, in the tone's ink (working = accent-text, idle = text-dim: text-mute is disabled-only)
//   text     grows: title (type title, 40 / 28 box, text) over body (type body, 32 / 20 box, text-dim), one line
//            each with an ellipsis; 40 + 32 = 72 = toast-h - 2 x toast-pad (48 at 480), so both fill the content box
//   action   optional: the Button component (secondary, fw::find), its own tap + sound; it dismisses the toast
//   bar      stroke::edge tall along the bottom inside the edge, the tone colour (working = accent), shrinking left
//            over the toast's life (layout.json toast life 6 s, life-urgent 12 s), linear; paused while pressed
// Gestures (docs toast.ts): finger down pauses the timer; a sideways drag follows the finger (translate_x only: the
// docs also fade it while dragging, left out because an opacity under 100 % renders the whole toast through a layer
// on every move, the slow part on the Guition); released past toast.swipe (64 / 44) = dismissed by swipe, moved less
// than toast.tap-slop = a tap (dismiss), anything between springs back (Motion toast-in) and the timer resumes.
// Dismiss = event "dismiss" (detail tap | swipe | timeout | action | pushed) and, for a tap or swipe, sound "back"
// (it closes, like the sheet's scrim / Close; the docs give the toast no sound of its own, open question), then
// leaves (Motion toast-out: fade + slide back to its edge by the offset, or sideways by toast.swipe-out after a
// swipe; reduced = fade). The Toast layer (toast_layer.cpp) builds toasts with internal props `layer` + `edge`,
// listens for LV_EVENT_VALUE_CHANGED (dismissed: restack the rest) and LV_EVENT_READY (gone: delete it), and sends
// LV_EVENT_CANCEL to push the oldest out (reason "pushed", silent). On its own (this page) a dismissed toast stays
// hidden; the workbench demo reset (1.8 s) brings it back.
// Perf: the bar is the only thing that moves while a toast just sits there: a stroke::edge high strip, so each frame
// redraws ~656 x 2 px (720) / 440 x 2 (480); no opacity, no layer. In / out (160 / 120 ms) fade the whole toast.
// Props: icon (glyph icons the docs' demo toasts use), tone, title, body, action, timer (off = static, the docs'
// documentation toast: a full bar, no timeout), life (normal | urgent). Contrast pairs: colour.json section "toast".
#include "fw.h"

namespace {
struct Tone { const char *id; lv_color_t fill, ink; };
Tone tone_of(const char *t) {
  const fw::Theme &T = *fw::T;
  if (!strcmp(t, "ok")) return {"ok", T.ok, T.ok};
  if (!strcmp(t, "warn")) return {"warn", T.warn, T.warn};
  if (!strcmp(t, "error")) return {"error", T.error, T.error};
  if (!strcmp(t, "working")) return {"working", T.accent, T.accent_text};
  if (!strcmp(t, "info")) return {"info", T.blue, T.blue};
  return {"idle", T.text_dim, T.text_dim};
}

struct Toast {
  lv_obj_t *o = nullptr, *bar = nullptr;
  int bar_w = 0;
  uint32_t life = 0;
  bool timer = true, layer = false, top = true, closed = false, down = false, dragged = false;
  int32_t x0 = 0, dx = 0;
};

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

void set_w(void *o, int32_t v) { lv_obj_set_width((lv_obj_t *) o, v); }
void set_tx(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *) o, v, 0); }
void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *) o, v, 0); }
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }

void dismiss(Toast *t, const char *how, bool quiet = false);

// the timer bar: shrink from its current width to 0 over what is left of the life (linear, Motion easing linear)
void bar_run(Toast *t) {
  if (!t->timer || t->closed || !t->bar) return;
  const int32_t w = lv_obj_get_style_width(t->bar, LV_PART_MAIN);  // the set width (no layout pass needed)
  if (w <= 0) { dismiss(t, "timeout"); return; }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, t->bar);
  lv_anim_set_exec_cb(&a, set_w);
  lv_anim_set_values(&a, w, 0);
  lv_anim_set_duration(&a, (uint32_t) ((uint64_t) t->life * w / t->bar_w));
  lv_anim_set_path_cb(&a, fw::mo::ease::linear);
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
  if (!quiet) fw::emit(t->o, "dismiss", how);
  if (!quiet && (!strcmp(how, "tap") || !strncmp(how, "swipe", 5))) fw::sound(t->o, "back");
  lv_obj_send_event(t->o, LV_EVENT_VALUE_CHANGED, (void *) how);  // the layer restacks the others now
  // leave (Motion toast-out): a swipe carries on sideways, anything else slides back to its edge; both fade
  if (!strncmp(how, "swipe", 5)) {
    const int dir = t->dx > 0 ? 1 : -1;
    fw::animate(t->o, fw::mo::toast_out, {set_tx, lv_obj_get_style_translate_x(t->o, LV_PART_MAIN), dir * fw::toast::swipe_out, fw::mo::MOVE});
    fw::Anim f{set_opa, lv_obj_get_style_opa(t->o, LV_PART_MAIN), LV_OPA_TRANSP, fw::mo::OPA};
    f.done = gone;
    f.user = t;
    fw::animate(t->o, fw::mo::toast_out, f);
  } else fw::appear(t->o, fw::mo::toast_out, false, t->top ? -1 : 1, 0, gone, t);
}

void spring_back(Toast *t) {
  t->down = false;
  fw::animate(t->o, fw::mo::toast_in, {set_tx, lv_obj_get_style_translate_x(t->o, LV_PART_MAIN), 0, fw::mo::MOVE});
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
      if (std::abs(t->dx) >= fw::toast::tap_slop) t->dragged = true;
      set_tx(t->o, t->dx);
      break;
    case LV_EVENT_RELEASED:
      if (!t->down) break;
      t->down = false;
      if (std::abs(t->dx) >= fw::toast::swipe) dismiss(t, t->dx > 0 ? "swipe-right" : "swipe-left");
      else if (!t->dragged && std::abs(t->dx) < fw::toast::tap_slop) { set_tx(t->o, 0); dismiss(t, "tap"); }
      else spring_back(t);
      break;
    case LV_EVENT_PRESS_LOST:
      if (t->down) spring_back(t);
      break;
    default: break;
  }
}
}  // namespace

namespace {
// the toast's look and contents on o (user data = its Toast, made once in build); the bar starts full
void fill(lv_obj_t *o, fw::Props p) {
  const fw::Theme &T = *fw::T;
  const Tone tone = tone_of(p["tone"] | "ok");
  auto *t = (Toast *) lv_obj_get_user_data(o);
  t->timer = p["timer"] | false;  // the layer passes it; off = the docs' static toast
  t->layer = p["layer"] | false;
  t->top = strcmp(p["edge"] | (fw::toast::top ? "top" : "bottom"), "bottom") != 0;
  t->life = !strcmp(p["life"] | "normal", "urgent") ? fw::toast::life_urgent : fw::toast::life;

  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(o, fw::lay::content_w, fw::lay::toast_h);
  lv_obj_set_style_bg_color(o, T.surface, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(o, T.line_hi, 0);
  lv_obj_set_style_radius(o, fw::rad::card, 0);
  lv_obj_set_style_pad_all(o, fw::lay::toast_pad - fw::stroke::hair, 0);
  lv_obj_set_style_pad_column(o, fw::lay::toast_pad, 0);
  lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *sq = box(o);
  lv_obj_set_size(sq, fw::lay::toast_icon, fw::lay::toast_icon);
  lv_obj_set_style_radius(sq, fw::rad::button, 0);
  lv_obj_set_style_bg_color(sq, tone.fill, 0);
  lv_obj_set_style_bg_opa(sq, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255), 0);
  if (lv_obj_t *ic = fw::icon(sq, p["icon"] | "timer", fw::ic::glyph, tone.ink)) lv_obj_center(ic);

  lv_obj_t *col = box(o);
  lv_obj_set_height(col, LV_SIZE_CONTENT);
  lv_obj_set_width(col, 0);
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  const struct { fw::ty::Id s; const char *txt; lv_color_t c; } lines[] = {
      {fw::ty::title, p["title"] | "", T.text}, {fw::ty::body, p["body"] | "", T.text_dim}};
  for (const auto &l : lines) {
    lv_obj_t *lb = fw::text(col, l.s, l.txt, l.c);
    lv_label_set_long_mode(lb, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(lb, LV_PCT(100));
    lv_obj_set_height(lb, fw::style(l.s).box);
    lv_obj_remove_flag(lb, LV_OBJ_FLAG_CLICKABLE);
  }

  const char *action = p["action"] | "";
  if (*action)
    if (const fw::Component *bc = fw::find("button")) {
      ArduinoJson::JsonDocument bp;
      bp["kind"] = "secondary";
      bp["label"] = action;
      const uint32_t n = lv_obj_get_child_count(o);
      bc->build(o, bp.as<fw::Props>());
      if (lv_obj_get_child_count(o) > n)
        lv_obj_add_event_cb(lv_obj_get_child(o, -1), [](lv_event_t *e) { dismiss((Toast *) lv_event_get_user_data(e), "action"); }, LV_EVENT_CLICKED, t);
    }

  // the timer bar: along the bottom inside the edge (children are placed from the content box: step back by pad + edge)
  lv_obj_update_layout(o);
  const int sl = lv_obj_get_style_space_left(o, LV_PART_MAIN), st = lv_obj_get_style_space_top(o, LV_PART_MAIN);
  t->bar_w = fw::lay::content_w - 2 * fw::stroke::hair;
  t->bar = box(o);
  lv_obj_add_flag(t->bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(t->bar, t->bar_w, fw::stroke::edge);
  lv_obj_set_pos(t->bar, fw::stroke::hair - sl, fw::lay::toast_h - fw::stroke::hair - fw::stroke::edge - st);
  lv_obj_set_style_bg_color(t->bar, tone.fill, 0);
  lv_obj_set_style_bg_opa(t->bar, LV_OPA_COVER, 0);


}
}  // namespace

FW_COMPONENT(toast, "Toast", "components", R"({
  "about": "A message over any screen: a tone-tinted icon, a title over a body line, an optional action and a timer bar. Tap or swipe it away.",
  "frame": [480, 0],
  "icons": ["timer@glyph", "agent@glyph", "person@glyph", "washer@glyph", "vacuum@glyph", "bolt@glyph", "alert@glyph", "done@glyph", "bell@glyph", "check@glyph", "pr@glyph"],
  "props": [
    {"name": "icon", "type": "select", "options": ["timer", "agent", "person", "washer", "vacuum", "bolt", "alert", "done", "bell", "check", "pr"], "default": "timer"},
    {"name": "tone", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "ok"},
    {"name": "title", "type": "text", "default": "Pasta is ready"},
    {"name": "body", "type": "text", "default": "Your 9 min timer has finished."},
    {"name": "action", "type": "text", "default": ""},
    {"name": "timer", "type": "bool", "default": false},
    {"name": "life", "type": "select", "options": ["normal", "urgent"], "default": "normal"}
  ],
  "variants": [
    {"label": "ok · timer done", "props": {}},
    {"label": "warn · action", "props": {"icon": "person", "tone": "warn", "title": "Someone at the front door", "body": "Person on the doorstep", "action": "View"}},
    {"label": "error", "props": {"icon": "alert", "tone": "error", "title": "Garden sensor offline", "body": "No reading for 30 min"}},
    {"label": "working", "props": {"icon": "agent", "tone": "working", "title": "Recipe importer started", "body": "Importing 12 recipes"}},
    {"label": "info", "props": {"icon": "washer", "tone": "info", "title": "Washing machine done", "body": "Took 1 h 4 min."}},
    {"label": "idle", "props": {"icon": "vacuum", "tone": "idle", "title": "Back on the dock", "body": "Kitchen and hallway cleaned in 23 min."}},
    {"label": "no body", "props": {"icon": "bell", "tone": "info", "title": "Doorbell", "body": ""}},
    {"label": "long text", "props": {"icon": "bolt", "tone": "warn", "title": "Lightning 9 km W, closing at 48 km/h from the coast", "body": "6 strikes within 25 km in the last ten minutes", "action": "Radar"}},
    {"label": "timer running", "props": {"timer": true}}
  ]
})") {
  auto *t = new Toast;
  lv_obj_t *o = t->o = lv_obj_create(parent);
  lv_obj_set_user_data(o, t);
  fill(o, p);

  for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(o, on_pointer, c, t);
  // pushed out by a newer toast (the layer; param 1 = a quiet build-time push: no event)
  lv_obj_add_event_cb(o, [](lv_event_t *e) { dismiss((Toast *) lv_event_get_user_data(e), "pushed", lv_event_get_param(e) != nullptr); }, LV_EVENT_CANCEL, t);
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *t = (Toast *) lv_event_get_user_data(e);
    lv_anim_delete(t->o, nullptr);
    if (t->bar) lv_anim_delete(t->bar, nullptr);
    delete t;
  }, LV_EVENT_DELETE, t);
  bar_run(t);
  bar_run(t);
}

// Update path: new icon, tone, texts or action = the contents again in the same toast (fw::refill, snap); the timer bar
// keeps how much life is left when the timer and life stay (else it starts full). A dismissed toast comes back in
// place (Motion toast-in from its edge, silent). Silent: no dismiss / arrival events or sounds.
FW_UPDATE(toast) {
  auto *t = (Toast *) lv_obj_get_user_data(root);
  if (!t) return false;
  const bool timer = p["timer"] | false;
  const uint32_t life = !strcmp(p["life"] | "normal", "urgent") ? fw::toast::life_urgent : fw::toast::life;
  int32_t left = -1;  // the bar's width to keep, -1 = full
  if (t->bar && timer == t->timer && life == t->life && !t->closed) left = lv_obj_get_style_width(t->bar, LV_PART_MAIN);
  bar_pause(t);
  const bool was_closed = t->closed;
  lv_anim_delete(root, nullptr);
  fw::refill(root);
  t->closed = t->down = t->dragged = false;
  t->dx = 0;
  fill(root, p);
  if (left >= 0 && t->bar) lv_obj_set_width(t->bar, left);
  lv_obj_set_style_translate_x(root, 0, 0);
  if (was_closed) {
    lv_obj_remove_flag(root, LV_OBJ_FLAG_HIDDEN);
    fw::appear(root, fw::mo::toast_in, true, t->top ? -1 : 1);
  }
  bar_run(t);
  return true;
}
