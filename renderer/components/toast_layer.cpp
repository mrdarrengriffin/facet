// Toast layer: the stack of toasts over a screen (docs: Components > Toasts > Toast layer, @facet/ui toast.ts
// toastLayer(), CSS .f-toasts; createDevice() puts one over every screen), on the contract (renderer/CONTRACT.md,
// "contract": 2). The layer itself (geometry, stacking, motion, the arrival's sound) is the core's: core/toasts.h.
// This file is the workbench page: the sample toasts and the screen under them.
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
// Workbench page (meta "stage": "screen"): the layer over a Page of real components (the Header "Toasts" + six List
// rows, one per tone: a tap on a row pushes that tone's toast). Props: position, push (the toast that arrives on build, so the
// 1.8 s demo reset replays it; none = start empty), stack (how many arrive on build, toast-in apart: 3 shows the
// oldest pushed out), timer (off = they stay until tapped or swiped: the variants, so the grid does not empty
// itself), action (the warn toast gets the docs' "View" button), screen. Internal: `overlay` (only the layer + its
// toasts, built into the caller's screen; the Layout foundation's overlay sample) with `still` (no motion, no timer);
// `loop` (a new toast every toast-in + toast-out, quietly: the frame-rate bench).
// Contrast pairs: tokens/colour.json section "toast" (+ header, list_row, button).
#include "fw.h"
#include "fx.h"
#include "toasts.h"

namespace {
using Doc = ArduinoJson::JsonDocument;

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

// the workbench's own state, on the overlay's user data (the core's layer rides on its DELETE event)
struct Demo {
  lv_obj_t *ov = nullptr;
  bool timer = true, action = true;
  lv_timer_t *stagger = nullptr, *loop = nullptr;
  int next = 0, left = 0;  // build-time pushes still to come (stack): `left` more, sample `next` first
};

// quiet = a build-time follow-up (stack > 1): no event, no sound, and the toast it pushes out leaves silently too, so a
// workbench sample's arrival never re-arms its demo reset (the hub mutes only the first 250 ms of a rebuild)
void push(Demo *D, int si, bool quiet = false) {
  if (si < 0 || si >= N_SAMPLES) return;
  const Sample &s = SAMPLES[si];
  Doc d;
  d["icon"] = s.icon, d["tone"] = s.tone, d["title"] = s.title, d["body"] = s.body;
  d["action"] = D->action ? s.action : "";
  d["life"] = s.urgent ? "urgent" : "normal";
  d["timer"] = D->timer;
  // detail "<tone> <icon> <life>: <title>" (functional tests read what arrived; toast_ha.test.mjs)
  char det[160];
  snprintf(det, sizeof det, "%s %s %s: %s", s.tone, s.icon, s.urgent ? "urgent" : "normal", s.title);
  fw::toasts::show(D->ov, d.as<fw::Props>(), quiet ? nullptr : det);
}

Demo *demo(lv_obj_t *host, fw::Props p) {
  auto *D = new Demo;
  D->timer = p["timer"] | true;
  D->action = p["action"] | true;
  D->ov = fw::toasts::layer(host, strcmp(p["position"] | (fw::toast::top ? "top" : "bottom"), "bottom") != 0, p["still"] | false);
  lv_obj_set_user_data(D->ov, D);  // the update path finds the demo here (USER_4 marks the overlay)
  lv_obj_add_event_cb(D->ov, [](lv_event_t *e) {
    auto *D = (Demo *) lv_event_get_user_data(e);
    if (D->stagger) lv_timer_delete(D->stagger);
    if (D->loop) lv_timer_delete(D->loop);
    delete D;
  }, LV_EVENT_DELETE, D);
  return D;
}

// the pushes a build makes: `push` arrives now (quiet = no event / sound: an update), with stack > 1 the next tones
// follow, toast-in apart (always quiet)
void pushes(Demo *D, fw::Props p, bool quiet) {
  const int first = sample_of(p["push"] | "ok");
  const int stack = std::min(std::max((int) (p["stack"] | 1), 1), N_SAMPLES);
  if (first < 0) return;
  push(D, first, quiet);
  D->next = first + 1;
  D->left = stack - 1;
  if (p["still"] | false)
    for (; D->left > 0; D->left--) push(D, D->next++ % N_SAMPLES, true);
  if (D->left > 0)
    D->stagger = lv_timer_create([](lv_timer_t *tm) {
      auto *D = (Demo *) lv_timer_get_user_data(tm);
      push(D, D->next++ % N_SAMPLES, true);
      if (--D->left <= 0) { D->stagger = nullptr; lv_timer_delete(tm); }
    }, fw::mo::ms::toast_in, D);
}
// what needs the screen again (fw::refill): the rest (push, stack, timer, action) moves in place
std::string sig(fw::Props p) {
  return std::string(p["position"] | (fw::toast::top ? "top" : "bottom")) + "|" + ((p["screen"] | true) ? "s" : "") + "|" +
         ((p["loop"] | false) ? "l" : "") + ((p["still"] | false) ? "t" : "");
}
// the screen (root = its box; nullptr = make it on parent) + the layer; returns the demo
Demo *fill(lv_obj_t *parent, lv_obj_t *root, fw::Props p) {
  const bool overlay = p["overlay"] | false;
  lv_obj_t *host = parent;
  lv_obj_t *rows[N_SAMPLES] = {};
  if (!overlay) {
    host = root ? root : fx::box(parent, "free");  // the screen: the Page, then the overlay over all of it
    for (const char *t : {"fill", "bg:bg"}) fx::use(host, t);
    fx::name(host, "toast_layer");
    fw::shape(host, sig(p));
    if (p["screen"] | true) {
      const fx::Page pg = fx::page(fx::box(host, "free"), "list");  // the Page structure (C1-C3)
      Doc hd;
      hd["label"] = "Toasts";
      build("header", pg.head, hd);
      for (int i = 0; i < N_SAMPLES; i++) {  // six rows fill the content height at both densities
        Doc d;
        d["title"] = SAMPLES[i].row;
        d["icon"] = "none";
        d["trailing"] = "value";
        d["value"] = SAMPLES[i].tone;
        rows[i] = build("list_row", pg.body, d);
      }
    }
  }
  Demo *D = demo(host, p);
  // a tap on a tone row pushes its toast: the List row's own tap event goes out, its tap sound does not (the toast's
  // notify is the one sound of the gesture, as when HA sends one: BF-034). Preprocess = before the row's handler.
  static fw::Quiet *row_quiet = nullptr;
  for (int i = 0; i < N_SAMPLES; i++) {
    if (!rows[i]) continue;
    lv_obj_add_event_cb(rows[i], [](lv_event_t *) { if (!row_quiet) row_quiet = new fw::Quiet; },
                        (lv_event_code_t) (LV_EVENT_CLICKED | LV_EVENT_PREPROCESS), nullptr);
    lv_obj_add_event_cb(rows[i], [](lv_event_t *e) {
      delete row_quiet;
      row_quiet = nullptr;
      auto *D = (Demo *) lv_event_get_user_data(e);
      push(D, (int) lv_obj_get_index((lv_obj_t *) lv_event_get_current_target(e)));
    }, LV_EVENT_CLICKED, D);
  }
  if (p["loop"] | false)  // frame-rate bench: a new toast every toast-in + toast-out (arrive, restack, push the oldest out)
    D->loop = lv_timer_create([](lv_timer_t *tm) {
      auto *D = (Demo *) lv_timer_get_user_data(tm);
      push(D, D->next++ % N_SAMPLES, true);
    }, fw::mo::ms::toast_in + fw::mo::ms::toast_out, D);
  return D;
}
}  // namespace

FW_COMPONENT(toast_layer, "Toast layer", "components", R"({
  "about": "The layer that stacks toasts over any screen (two at most), animates them in and out and plays the notify sound. Tap a row to send that tone's toast.",
  "contract": 2,
  "parts": [],
  "stage": "screen",
  "frame": [480, 480],
  "icons": ["timer@glyph", "person@glyph", "alert@glyph", "agent@glyph", "washer@glyph", "vacuum@glyph", "back@glyph"],
  "props": [
    {"name": "position", "about": "Where messages appear: from the top or from the bottom of the screen.", "type": "select", "options": ["top", "bottom"], "default": "top"},
    {"name": "push", "about": "For previews only: the tone of the first sample message that arrives, or none.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle", "none"], "default": "ok"},
    {"name": "stack", "about": "For previews only: how many sample messages are stacked, up to two showing at once.", "type": "range", "min": 1, "max": 3, "step": 1, "default": 1},
    {"name": "timer", "about": "Shows the bar on each message that runs down until it goes away.", "type": "bool", "default": true},
    {"name": "action", "about": "For previews only: gives the sample messages a button.", "type": "bool", "default": true},
    {"name": "screen", "about": "For previews only: draws a sample screen under the messages.", "type": "bool", "default": true}
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
  pushes(fill(parent, nullptr, p), p, false);
}

// Update path: the layer's root = the screen box (the overlay itself when built as an overlay). push / stack / timer /
// action = the toasts showing leave quietly (Motion toast-out) and the new ones arrive (toast-in), no event or sound;
// position, screen = the screen again (fw::refill, snap) and the toasts arrive. Silent.
FW_UPDATE(toast_layer) {
  lv_obj_t *ov = lv_obj_has_flag(root, LV_OBJ_FLAG_USER_4) ? root : lv_obj_get_child(root, -1);
  if (!ov || !lv_obj_has_flag(ov, LV_OBJ_FLAG_USER_4)) return false;
  if (ov != root && !fw::shape(root, sig(p))) {
    fw::refill(root);  // the old overlay goes with it (its DELETE frees the layer and its timers)
    pushes(fill(nullptr, root, p), p, true);
    return true;
  }
  auto *D = (Demo *) lv_obj_get_user_data(ov);
  if (D->stagger) { lv_timer_delete(D->stagger); D->stagger = nullptr; }
  D->timer = p["timer"] | true;
  D->action = p["action"] | true;
  D->left = 0;
  fw::toasts::clear(ov);
  pushes(D, p, true);
  return true;
}
