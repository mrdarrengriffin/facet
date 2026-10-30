// Section header: the Facet section header (docs: @facet/ui lists.ts sectionHeader({title, count, tone, right}), CSS
// .f-section), on the contract (renderer/CONTRACT.md, "contract": 2): a caps title, an optional count badge, and an
// optional right side, above a group of rows.
//   size     fills its parent's width (C5; in a list Body = inset to inset, as before), h:section-h tall (48 / 32,
//            = touch min), a flex row centred, gap:5 (16 / 12 = docs gutter at both densities)
//   minimum  layout:card-min-w x layout:section-h. Steps: one (the title ends in "..." before the right side)
//   title    type c-caps-sm, text-dim (= docs .f-section-title); size label = the screen label type, the row then
//            section-label tall (label box + space 2 to the rows), items at the top (the panel lab's Door lock "last 24 h")
//   count    the Count badge component (prop "badge"; count / tone / zero passed on: 0 = muted or hidden)
//   right    none | meta (type c-meta, text-dim: docs .f-section-meta is text-mute, which is disabled-only here) |
//            action (the Button component, ghost small: see Action)
// Action: the Button component (C8b), ghost, size small (touch:min tall = section-h, its label space 4 in from its
// edge, lined up with the List rows' values and chevrons below); its press, disabled, tap and sound are the Button's.
// States: the action's are the Button's (default, pressed, disabled = "blocked" + error). n/a: on, unavailable,
// loading, unknown, empty, error (a caption: no data of its own; the count badge shows its own zero). The title,
// badge and meta are static (no events). Bleed: none.
// Contrast pairs: tokens/colour.json section "section_header" (+ label, count_badge).
// Built inside: the Count badge and the Button (fw::find).
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
lv_obj_t *words(lv_obj_t *h, const char *s, const char *type, const char *fg) {
  lv_obj_t *l = fx::text(h, s, type, fg);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  return l;
}

void fill(lv_obj_t *h, fw::Props p) {
  const bool big = !strcmp(p["size"] | "sm", "label");
  for (const char *t : {"box", "row-mid", "gap:5", big ? "section-label" : "h:section-h"}) fx::use(h, t);
  fx::name(h, "section_header");
  fx::fit_grid(words(h, p["title"] | "", big ? "type:label" : "type:c-caps-sm", "fg:text-dim"));  // the badge after it on the grid

  if (p["badge"].is<bool>() ? (bool) p["badge"] : !p["count"].isNull()) {  // no count asked for = no badge
    ArduinoJson::JsonDocument bp;
    bp["count"] = p["count"];  // a number or a bound state's text: the badge reads either
    bp["tone"] = p["tone"] | "warn";
    bp["zero"] = p["zero"] | "muted";
    build("count_badge", h, bp);
  }

  fx::use(fx::box(h, "free"), "grow");  // the free room between the left and the right side

  const char *right = p["right"] | "meta";
  if (!strcmp(right, "meta") && (p["meta"] | "")[0]) {  // an empty meta draws nothing (not the label's sample)
    words(h, p["meta"] | "", "type:c-meta", "fg:text-dim");
  } else if (!strcmp(right, "action")) {  // the Button component itself (C8b): ghost, small (touch:min = the row's height)
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "ghost";
    bp["size"] = "small";
    bp["label"] = p["action"] | "";
    bp["state"] = p["state"] | "default";
    build("button", h, bp);
  }
}
}  // namespace

FW_COMPONENT(section_header, "Section header", "components", R"({
  "about": "A caption that starts a group of content, with an optional count and a meta text or action on the right.",
  "contract": 2,
  "parts": [],
  "min": ["layout:card-min-w", "layout:section-h"],
  "audit": {"snaps": {"count": "the Count badge's count changes like text"}},
  "frame": [480, 96],
  "props": [
    {"name": "title", "about": "The caption that starts the group.", "type": "text", "default": "Needs you"},
    {"name": "badge", "about": "Shows a count badge after the caption.", "type": "bool", "default": true},
    {"name": "count", "about": "The number in the badge.", "type": "range", "min": 0, "max": 150, "step": 1, "default": 3},
    {"name": "tone", "about": "The badge colour, by what the number means: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "warn"},
    {"name": "zero", "about": "What happens when the count is zero: muted shows a quiet 0; hide removes the badge.", "type": "select", "options": ["muted", "hide"], "default": "muted"},
    {"name": "right", "about": "What sits at the right end: none, meta for a quiet line of text, or action for a tappable link.", "type": "select", "options": ["none", "meta", "action"], "default": "meta"},
    {"name": "meta", "about": "The quiet text at the right end, such as when it was last updated.", "type": "text", "default": "updated 2m"},
    {"name": "action", "about": "The words of the link at the right end, such as See all.", "type": "text", "default": "See all"},
    {"name": "state", "debug": true, "about": "How the link looks: default ready to tap, pressed while held, or disabled.", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"},
    {"name": "size", "about": "sm is the small capitals caption; label is the larger screen label style.", "type": "select", "options": ["sm", "label"], "default": "sm"}
  ],
  "variants": [
    {"label": "count + meta", "props": {}},
    {"label": "title only", "props": {"title": "Today", "badge": false, "right": "none"}},
    {"label": "count 0 · muted", "props": {"title": "Reviews", "count": 0, "right": "none"}},
    {"label": "count 0 · hidden", "props": {"title": "Reviews", "count": 0, "zero": "hide", "right": "none"}},
    {"label": "99+ · working", "props": {"title": "Running", "count": 128, "tone": "working", "right": "none"}},
    {"label": "action", "props": {"title": "Your pull requests", "count": 2, "tone": "info", "right": "action"}},
    {"label": "action · pressed", "props": {"title": "Notifications", "count": 5, "tone": "error", "right": "action", "action": "Clear", "state": "pressed"}},
    {"label": "action · disabled", "props": {"title": "Notifications", "badge": false, "right": "action", "action": "Clear", "state": "disabled"}}
  ]
})") {
  lv_obj_t *h = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(h);
  lv_obj_remove_flag(h, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fill(h, p);
  fw::st::built(h, p);
}

// Update path: the action's default <-> pressed = its press fade in place; any other prop builds the header again in
// the same root (fw::refill, snap: badge, right side, texts), the action going disabled / back fades in. Silent.
FW_UPDATE(section_header) {
  const bool pr = !strcmp(p["state"] | "default", "pressed");
  const bool was_off = !strcmp(p["right"] | "meta", "action") && lv_obj_has_state(lv_obj_get_child(root, -1), ST_OFF);
  if (fw::shape(root, [&] {  // the props with pressed read as default (fw::st::built stored the same)
        ArduinoJson::JsonDocument r;
        r.set(p);
        if (pr) r["state"] = "default";
        std::string s;
        serializeJson(r, s);
        return s;
      }())) {
    lv_obj_t *a = lv_obj_get_child(root, -1);
    if (a && !strcmp(p["right"] | "meta", "action") && pr != lv_obj_has_state(a, LV_STATE_PRESSED))
      lv_obj_set_state(a, LV_STATE_PRESSED, pr);
    return true;
  }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // a new tone fades like a tap (shared styles too)
  const bool off = !strcmp(p["right"] | "meta", "action") && !strcmp(p["state"] | "default", "disabled");
  if (off != was_off) fw::st::fade_in_children(lv_obj_get_child(root, -1));
  return true;
}
