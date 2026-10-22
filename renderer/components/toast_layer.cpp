// Toast layer: the stack of toasts over a screen (docs: Components > Toasts > Toast layer, @facet/ui toast.ts
// toastLayer(), CSS .f-toasts; createDevice() puts one over every screen).
//   layer    covers the screen it is built on (floating, ignores the screen's layout), NOT clickable: only the toasts
//            take taps, everything else reaches the screen under it (docs pointer-events none on the layer)
//   stack    up to toast.max (2) Toast components (fw::find("toast"), internal props layer + edge), newest nearest the
//            edge they enter from, lay::gutter apart; top (layout.json toast.position default) = the first at
//            lay::toast_top (96 / 64, just under the header), bottom = the first ends at the bottom inset; x = inset
//   push     the new toast starts at slot 0, fades in and slides from its edge by the offset (Motion toast-in, 160 ms
//            ease-out; reduced = fade); the others move one slot along (y, Motion toast-in, as the docs' CSS
//            transition on top); a toast over the max is pushed out (the oldest; Toast reason "pushed", silent)
//   sound    toast.sound (notify) when a toast arrives (docs device onToast -> play('notify')) + event "toast"
//   dismiss  the Toast leaves by itself (tap, swipe, timeout, action: toast.cpp); the layer restacks the rest at once
//            and deletes the toast when its leave finishes
// Perf (Guition): small invalidation only. A sitting toast redraws its timer bar strip (stroke::edge high); an
// arriving / leaving toast redraws its own box (~656 x 120 / 440 x 80 + the offset) while it fades (opacity under
// 100 % = one layer the size of the toast); a restack moves whole toasts by y (two boxes per frame, no opacity).
// Nothing full-screen: no scrim, the screen under it is never touched except where a toast was.
// Workbench page: the layer over a real-component screen at device size (the Header "Toasts" + six List rows, one per
// tone: a tap on a row pushes that tone's toast). Props: position, push (the toast that arrives on build, so the
// 1.8 s demo reset replays it; none = start empty), stack (how many arrive on build, toast-in apart: 3 shows the
// oldest pushed out), timer (off = they stay until tapped or swiped: the variants, so the grid does not empty
// itself), action (the warn toast gets the docs' "View" button), screen. Internal: `overlay` (only the layer + its
// toasts, built into the caller's screen; the Layout foundation's overlay sample) with `still` (no motion, no timer);
// `loop` (a new toast every toast-in + toast-out, quietly: the frame-rate bench).
// Contrast pairs: tokens/colour.json section "toast" (+ header, list_row, button).
#include <vector>
#include "fw.h"

namespace {
using Doc = ArduinoJson::JsonDocument;

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}

// one toast per tone (the docs' demo toasts, @facet/demo-data TOASTS; working + idle added so every tone has one)
struct Sample { const char *tone, *row, *icon, *title, *body, *action; bool urgent; };
const Sample SAMPLES[] = {
    {"ok", "Timer done", "timer", "Pasta is ready", "Your 9 min timer has finished.", "", false},
    {"warn", "Someone at the door", "person", "Someone at the front door", "Person on the doorstep", "View", true},
    {"error", "Something offline", "alert", "Garden sensor offline", "No reading for 30 min", "", false},
    {"working", "Agent started", "agent", "Recipe importer started", "Importing 12 recipes", "", false},
    {"info", "Washing done", "washer", "Washing machine done", "Took 1 h 4 min.", "", false},
    {"idle", "Vacuum docked", "vacuum", "Back on the dock", "Kitchen and hallway cleaned in 23 min.", "", false},
};
constexpr int N_SAMPLES = sizeof(SAMPLES) / sizeof(SAMPLES[0]);
int sample_of(const char *tone) {
  for (int i = 0; i < N_SAMPLES; i++)
    if (!strcmp(SAMPLES[i].tone, tone)) return i;
  return -1;
}

struct Layer {
  lv_obj_t *ov = nullptr;
  std::vector<lv_obj_t *> shown;  // newest first
  bool top = true, timer = true, action = true, still = false;
  lv_timer_t *stagger = nullptr, *loop = nullptr;
  int next = 0, left = 0;  // build-time pushes still to come (stack): `left` more, sample `next` first
};

int slot_y(const Layer *L, int i) {
  const int step = fw::lay::toast_h + fw::lay::gutter;
  return L->top ? fw::lay::toast_top + i * step : fw::lay::screen_h - fw::lay::inset - fw::lay::toast_h - i * step;
}
void set_y(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *) o, v); }

// every shown toast to its slot (Motion toast-in on y: the docs' CSS transition on top)
void restack(Layer *L) {
  for (size_t i = 0; i < L->shown.size(); i++) {
    lv_obj_t *t = L->shown[i];
    const int32_t from = lv_obj_get_y(t), to = slot_y(L, (int) i);
    if (from == to) continue;
    if (L->still) { lv_obj_set_y(t, to); continue; }
    fw::animate(t, fw::mo::toast_in, {set_y, from, to, fw::mo::MOVE});
  }
}

// quiet = a build-time follow-up (stack > 1): no event, no sound, and the toast it pushes out leaves silently too, so a
// workbench sample's arrival never re-arms its demo reset (the hub mutes only the first 250 ms of a rebuild)
void push(Layer *L, int si, bool quiet = false) {
  if (si < 0 || si >= N_SAMPLES) return;
  const Sample &s = SAMPLES[si];
  Doc d;
  d["icon"] = s.icon, d["tone"] = s.tone, d["title"] = s.title, d["body"] = s.body;
  d["action"] = L->action ? s.action : "";
  d["life"] = s.urgent ? "urgent" : "normal";
  d["timer"] = L->timer && !L->still;
  d["layer"] = true;
  d["edge"] = L->top ? "top" : "bottom";
  lv_obj_t *t = build("toast", L->ov, d);
  if (!t) return;
  lv_obj_add_flag(t, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_pos(t, fw::lay::inset, slot_y(L, 0));
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
  while ((int) L->shown.size() > fw::toast::max) {
    lv_obj_t *old = L->shown.back();
    L->shown.pop_back();
    lv_obj_send_event(old, LV_EVENT_CANCEL, quiet ? (void *) 1 : nullptr);
  }
  restack(L);
  if (!L->still) {
    lv_obj_set_style_opa(t, LV_OPA_TRANSP, 0);
    fw::appear(t, fw::mo::toast_in, true, L->top ? -1 : 1);
  }
  if (!L->still && !quiet) {
    fw::emit(t, "toast", s.title);
    fw::sound(t, fw::toast::sound);
  }
}

Layer *layer(lv_obj_t *host, fw::Props p) {
  auto *L = new Layer;
  L->top = strcmp(p["position"] | (fw::toast::top ? "top" : "bottom"), "bottom") != 0;
  L->timer = p["timer"] | true;
  L->action = p["action"] | true;
  L->still = p["still"] | false;
  L->ov = box(host);
  lv_obj_add_flag(L->ov, (lv_obj_flag_t) (LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_USER_4));
  lv_obj_set_user_data(L->ov, L);  // the update path finds the layer here (USER_4 marks the overlay)
  lv_obj_set_size(L->ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(L->ov, 0, 0);
  lv_obj_add_event_cb(L->ov, [](lv_event_t *e) {
    auto *L = (Layer *) lv_event_get_user_data(e);
    if (L->stagger) lv_timer_delete(L->stagger);
    if (L->loop) lv_timer_delete(L->loop);
    delete L;
  }, LV_EVENT_DELETE, L);
  return L;
}
}  // namespace

namespace {
// the pushes a build makes: `push` arrives now (quiet = no event / sound: an update), with stack > 1 the next tones
// follow, toast-in apart (always quiet)
void pushes(Layer *L, fw::Props p, bool quiet) {
  const int first = sample_of(p["push"] | "ok");
  const int stack = std::min(std::max((int) (p["stack"] | 1), 1), N_SAMPLES);
  if (first < 0) return;
  push(L, first, quiet);
  L->next = first + 1;
  L->left = stack - 1;
  if (L->still)
    for (; L->left > 0; L->left--) push(L, L->next++ % N_SAMPLES, true);
  if (L->left > 0)
    L->stagger = lv_timer_create([](lv_timer_t *tm) {
      auto *L = (Layer *) lv_timer_get_user_data(tm);
      push(L, L->next++ % N_SAMPLES, true);
      if (--L->left <= 0) { L->stagger = nullptr; lv_timer_delete(tm); }
    }, fw::mo::ms::toast_in, L);
}
// what needs the screen again (fw::refill): the rest (push, stack, timer, action) moves in place
std::string sig(fw::Props p) {
  return std::string(p["position"] | (fw::toast::top ? "top" : "bottom")) + "|" + ((p["screen"] | true) ? "s" : "") + "|" +
         ((p["loop"] | false) ? "l" : "") + ((p["still"] | false) ? "t" : "");
}
// the screen (root = its box; nullptr = make it on parent) + the layer; returns the layer
Layer *fill(lv_obj_t *parent, lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool overlay = p["overlay"] | false;
  lv_obj_t *host = parent;
  lv_obj_t *rows[N_SAMPLES] = {};
  if (!overlay) {
    host = root ? root : box(parent);
    lv_obj_remove_style_all(host);
    lv_obj_remove_flag(host, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    fw::shape(host, sig(p));
    lv_obj_set_size(host, fw::lay::screen_w, fw::lay::screen_h);
    lv_obj_set_style_bg_color(host, t.bg, 0);
    lv_obj_set_style_bg_opa(host, LV_OPA_COVER, 0);
    if (p["screen"] | true) {
      Doc hd;
      hd["label"] = "Toasts";
      if (lv_obj_t *h = build("header", host, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);
      lv_obj_t *c = box(host);
      lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
      lv_obj_set_size(c, fw::lay::content_w, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
      // six rows fill the content height at both densities (6 x 96 = 576 of 576; 6 x 64 = 384 of 388)
      static_assert(N_SAMPLES * fw::lay::list_row_h <= fw::lay::content_h, "the tone rows must fit the content area");
      for (int i = 0; i < N_SAMPLES; i++) {
        Doc d;
        d["title"] = SAMPLES[i].row;
        d["icon"] = "none";
        d["trailing"] = "value";
        d["value"] = SAMPLES[i].tone;
        rows[i] = build("list_row", c, d);
      }
    }
  }
  Layer *L = layer(host, p);
  // a tap on a tone row pushes its toast (after the List row's own tap: event + sound tap)
  for (int i = 0; i < N_SAMPLES; i++) {
    if (!rows[i]) continue;
    lv_obj_set_user_data(rows[i], (void *) (intptr_t) i);
    lv_obj_add_event_cb(rows[i], [](lv_event_t *e) {
      push((Layer *) lv_event_get_user_data(e), (int) (intptr_t) lv_obj_get_user_data((lv_obj_t *) lv_event_get_current_target(e)));
    }, LV_EVENT_CLICKED, L);
  }
  if (p["loop"] | false)  // frame-rate bench: a new toast every toast-in + toast-out (arrive, restack, push the oldest out)
    L->loop = lv_timer_create([](lv_timer_t *tm) {
      auto *L = (Layer *) lv_timer_get_user_data(tm);
      push(L, L->next++ % N_SAMPLES, true);
    }, fw::mo::ms::toast_in + fw::mo::ms::toast_out, L);
  return L;
}
}  // namespace

FW_COMPONENT(toast_layer, "Toast layer", "components", R"({
  "about": "The layer that stacks toasts over any screen (two at most), animates them in and out and plays the notify sound. Tap a row to send that tone's toast.",
  "frame": [480, 480],
  "icons": ["timer@glyph", "person@glyph", "alert@glyph", "agent@glyph", "washer@glyph", "vacuum@glyph", "back@glyph"],
  "props": [
    {"name": "position", "type": "select", "options": ["top", "bottom"], "default": "top"},
    {"name": "push", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle", "none"], "default": "ok"},
    {"name": "stack", "type": "range", "min": 1, "max": 3, "step": 1, "default": 1},
    {"name": "timer", "type": "bool", "default": true},
    {"name": "action", "type": "bool", "default": true},
    {"name": "screen", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "top · ok arrives", "props": {"timer": false}},
    {"label": "bottom · info arrives", "props": {"position": "bottom", "push": "info", "timer": false}},
    {"label": "two stacked · newest first", "props": {"push": "error", "stack": 2, "timer": false}},
    {"label": "a third pushes the oldest out", "props": {"push": "ok", "stack": 3, "timer": false}},
    {"label": "warn with its action", "props": {"push": "warn", "timer": false}},
    {"label": "timer running · 6 s, then it leaves", "props": {"push": "working"}},
    {"label": "empty · tap a row", "props": {"push": "none"}},
    {"label": "reduced motion", "props": {"push": "idle", "stack": 2, "timer": false, "reduced_motion": true}}
  ]
})") {
  Layer *L = fill(parent, nullptr, p);
  pushes(L, p, false);
}

// Update path: the layer's root = the screen box (the overlay itself when built as an overlay). push / stack / timer /
// action = the toasts showing leave quietly (Motion toast-out) and the new ones arrive (toast-in), no event or sound;
// position, screen = the screen again (fw::refill, snap) and the toasts arrive. Silent.
FW_UPDATE(toast_layer) {
  lv_obj_t *ov = lv_obj_has_flag(root, LV_OBJ_FLAG_USER_4) ? root : lv_obj_get_child(root, -1);
  if (!ov || !lv_obj_has_flag(ov, LV_OBJ_FLAG_USER_4)) return false;
  Layer *L = (Layer *) lv_obj_get_user_data(ov);
  if (ov != root && !fw::shape(root, sig(p))) {
    fw::refill(root);  // the old overlay goes with it (its DELETE frees the layer and its timers)
    pushes(fill(nullptr, root, p), p, true);
    return true;
  }
  if (L->stagger) { lv_timer_delete(L->stagger); L->stagger = nullptr; }
  L->timer = p["timer"] | true;
  L->action = p["action"] | true;
  L->left = 0;
  while (!L->shown.empty()) {  // every toast showing leaves quietly (it takes itself out of `shown`)
    lv_obj_t *old = L->shown.back();
    L->shown.pop_back();
    lv_obj_send_event(old, LV_EVENT_CANCEL, (void *) 1);
  }
  pushes(L, p, true);
  return true;
}
