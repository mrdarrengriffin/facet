// Toast: one Facet notification (docs: Components > Toasts > Toast, @facet/ui toast.ts toast(), CSS .f-toast), on the
// contract (renderer/CONTRACT.md, "contract": 2): a message over any screen with its tone, icon, title, body, an
// optional action and a timer bar. What moves and what a finger does is the core's (core/toasts.h: gestures, timer
// bar, dismiss, in / out motion, the panel key); this file builds the look from token names:
//   box      the "toast" style: the width it is given (the layer: inset to inset), layout toast-h tall (120 / 80),
//            padding toast-pad less the hair edge, a row centred, toast-pad between; bg:surface, border:line-hi,
//            radius:card, then part:card (a theme's card: a toast is a surface over the screen)
//   icon     a layout toast-icon square (48 / 32, radius:button), tint:<tone> (the tone at chip.tint, = the Status
//            chip's fill), the glyph icon centred in it in the tone's ink (working = accent-text, idle = text-dim)
//   text     the free width (grow-w): title (type title, text) over body (type body, text-dim), one line each, "..."
//   action   optional: the Button (secondary, fw::find), its own tap + sound; it dismisses the toast (core)
//   bar      a box in bg:<tone> (working = accent); the core places it along the bottom and runs it down
// States: n/a (pressed = the drag; the core's). Props: icon (glyph icons the docs' demo toasts use), tone, title,
// body, action, timer (off = static, the docs' documentation toast: a full bar, no timeout), life (normal | urgent).
// Internal: layer (built by the Toast layer: core/toasts.h show). Contrast pairs: colour.json section "toast".
#include "fw.h"
#include "fx.h"
#include "toasts.h"

namespace {
const char *tint_of(const char *t) {
  if (!strcmp(t, "ok")) return "tint:ok";
  if (!strcmp(t, "warn")) return "tint:warn";
  if (!strcmp(t, "error")) return "tint:error";
  if (!strcmp(t, "working")) return "tint:accent";
  if (!strcmp(t, "info")) return "tint:blue";
  return "tint:text-dim";  // idle (text-mute is for disabled only)
}
const char *tone_word(const char *t) {  // the tone's colour token
  for (const char *k : {"ok", "warn", "error"}) if (!strcmp(t, k)) return k;
  if (!strcmp(t, "working")) return "accent";
  if (!strcmp(t, "info")) return "blue";
  return "text-dim";
}

// the toast's look and contents on o; returns the timer bar (the core places and runs it)
lv_obj_t *fill(lv_obj_t *o, fw::Props p) {
  const char *tone = p["tone"] | "ok";
  const std::string word = tone_word(tone), fg = !strcmp(tone, "working") ? "fg:accent-text" : "fg:" + word;
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  for (const char *t : {"toast", "bg:surface", "border:line-hi", "radius:card", "part:card"}) fx::use(o, t);

  lv_obj_t *sq = fx::box(o, "over");  // the icon square, its glyph centred
  for (const char *t : {"hug", "w:toast-icon", "h:toast-icon", "radius:button"}) fx::use(sq, t);
  fx::use(sq, tint_of(tone));
  fx::icon(sq, p["icon"] | "timer", "glyph", fg.c_str());

  lv_obj_t *col = fx::box(o, "col");
  fx::use(col, "grow-w");
  const struct { const char *txt, *type, *fg; } lines[] = {{p["title"] | "", "type:title", "fg:text"}, {p["body"] | "", "type:body", "fg:text-dim"}};
  for (const auto &l : lines) {
    lv_obj_t *lb = fx::text(col, l.txt, l.type, l.fg);
    fx::use(lb, "fill-w");
    lv_label_set_long_mode(lb, LV_LABEL_LONG_MODE_DOTS);
  }

  const char *action = p["action"] | "";
  if (*action)
    if (const fw::Component *bc = fw::find("button")) {
      ArduinoJson::JsonDocument bp;
      bp["kind"] = "secondary";
      bp["label"] = action;
      const uint32_t n = lv_obj_get_child_count(o);
      bc->build(o, bp.as<fw::Props>());
      if (lv_obj_get_child_count(o) > n) fw::toasts::action(o, lv_obj_get_child(o, -1));
    }

  lv_obj_t *bar = fx::box(o, "free");
  fx::use(bar, ("bg:" + word).c_str());
  return bar;
}
bool urgent(fw::Props p) { return !strcmp(p["life"] | "normal", "urgent"); }
}  // namespace

FW_COMPONENT(toast, "Toast", "components", R"({
  "about": "A message over any screen: a tone-tinted icon, a title over a body line, an optional action and a timer bar. Tap or swipe it away.",
  "contract": 2,
  "parts": ["card"],
  "frame": [480, 0],
  "icons": ["timer@glyph", "agent@glyph", "person@glyph", "washer@glyph", "vacuum@glyph", "bolt@glyph", "alert@glyph", "done@glyph", "bell@glyph", "check@glyph", "pr@glyph"],
  "props": [
    {"name": "icon", "about": "The icon at the start, picked for what the message is about, such as a timer, a person or a washing machine.", "type": "select", "options": ["timer", "agent", "person", "washer", "vacuum", "bolt", "alert", "done", "bell", "check", "pr"], "default": "timer"},
    {"name": "tone", "about": "The icon's colour, by what the message means: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "ok"},
    {"name": "title", "about": "The message's heading.", "type": "text", "default": "Pasta is ready"},
    {"name": "body", "about": "One line of detail under the heading.", "type": "text", "default": "Your 9 min timer has finished."},
    {"name": "action", "about": "The words of a button on the message, such as Open. Leave empty for no button.", "type": "text", "default": ""},
    {"name": "timer", "about": "Shows a thin bar that runs down until the message goes away on its own.", "type": "bool", "default": false},
    {"name": "life", "about": "How long the message stays: normal, or urgent which stays much longer.", "type": "select", "options": ["normal", "urgent"], "default": "normal"}
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
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  fx::name(o, "toast");
  fw::toasts::own(o);
  lv_obj_t *bar = fill(o, p);
  fw::toasts::arm(o, bar, p["timer"] | false, urgent(p));
}

// Update path: new icon, tone, texts or action = the contents again in the same toast (fw::refill; a new tone fades
// like a tap); the timer bar keeps how much life is left when the timer and life stay (else it starts full). A
// dismissed toast comes back in place (Motion toast-in from its edge, silent). Silent: no dismiss / arrival events or sounds.
FW_UPDATE(toast) {
  const bool timer = p["timer"] | false;
  const int32_t keep = fw::toasts::left(root, timer, urgent(p));
  lv_obj_t *bar = nullptr;
  fw::st::refill_fade(root, [&] {
    fw::refill(root);
    bar = fill(root, p);
  }, true, true);
  fw::toasts::again(root, bar, timer, urgent(p), keep);
  return true;
}
