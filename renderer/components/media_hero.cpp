// Media hero: the Media screen's hero (docs: @facet/ui screens/media.ts: the disc slot + .fs-m-col; CSS .fs-m-*), on
// the contract (renderer/CONTRACT.md, "contract": 2). Specific to the Media screen.
//   row      the picture slot (w:mv-h, the disc's slot) and the words column (grow), cross-centred, gap:9 apart
//   picture  a real Media visual (fw::find("media_visual"), C8b): disc / cassette / cd / vinyl, size screen, in a slot
//            wrap-end: one wider than its slot (the cassette) keeps its right edge on the slot's and runs off the
//            screen's left edge (docs .f-mv-layer, cropped left; fx::overflow draws the overhang)
//   column   gap:2 between its lines: the state (type:label caps; fg:accent-text while playing = docs data-tone
//            working, else fg:text-dim), the title (type:headline, fg:text, two lines then "...", h:m-title-h: the artist
//            never moves; docs line-clamp 2, where the artist follows a one-line title), the artist (type:body, fg:text-dim, one line, "...")
// Feed: bind every prop to panel.media (core/media_source.cpp). A read-out (docs: no touch): no events, no sound.
// Update path: the words and the picture in place (the visual's own update: spin, packs, arm, arc), silent; another
// visual or a new state ink rebuilds with a fade (fw::st::refill_fade). Contrast: text, media_visual.
#include "fw.h"
#include "fx.h"

namespace {
bool playing_of(fw::Props p) {
  if (p["playing"].is<bool>()) return p["playing"].as<bool>();
  const char *v = p["playing"] | "true";
  return !strcmp(v, "true") || !strcmp(v, "1");
}
const char *ink(fw::Props p) { return !strcmp(p["tone"] | "text-dim", "accent-text") ? "fg:accent-text" : "fg:text-dim"; }
std::string sig(fw::Props p) { return std::string(p["visual"] | "cd") + ink(p); }
void visual_props(fw::Props p, ArduinoJson::JsonDocument &d) {
  d["visual"] = p["visual"] | "cd", d["size"] = "screen", d["progress"] = p["progress"] | 38, d["playing"] = playing_of(p);
  d["title"] = p["title"] | "", d["artist"] = p["artist"] | "";
}
void fill(lv_obj_t *root, fw::Props p) {
  for (const char *t : {"box", "row-top", "fill-w", "gap:m-gap"}) fx::use(root, t);
  fw::shape(root, sig(p), "hero");
  fx::overflow(root);  // the cassette's overhang past the inset, off the screen's left edge (docs: cropped left)
  lv_obj_t *slot = fx::box(root, "free");
  for (const char *t : {"wrap-end", "w:mv-h"}) fx::use(slot, t);  // its one child at the end: a wider one runs off to the left
  fx::overflow(slot);
  ArduinoJson::JsonDocument d;
  visual_props(p, d);
  fw::find("media_visual")->build(slot, d.as<fw::Props>());
  lv_obj_t *col = fx::box(root, "col");
  for (const char *t : {"grow", "gap:2"}) fx::use(col, t);
  lv_obj_t *st = fx::text(col, p["state"] | "idle", "type:label", ink(p));
  for (const char *t : {"fill-w", "above:m-col-top"}) fx::use(st, t);
  lv_obj_t *ti = fx::text(col, p["title"] | "Nothing playing", "type:headline", "fg:text");
  for (const char *t : {"fill-w", "h:m-title-h", "max-h:m-title-h"}) fx::use(ti, t);  // two lines, then "..."
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_t *ar = fx::text(col, p["artist"] | "", "type:body", "fg:text-dim");
  fx::use(ar, "fill-w");
}
}  // namespace

FW_COMPONENT(media_hero, "Media hero", "components", R"({
  "about": "The top of the Media screen: the record or tape that is playing beside what it is, the track and the artist.",
  "contract": 2,
  "parts": [],
  "frame": [656, 280],
  "props": [
    {"name": "visual", "about": "The picture: the plain disc with a progress ring, a cassette, a CD or a vinyl record.", "type": "select", "options": ["disc", "cassette", "cd", "vinyl"], "default": "cd"},
    {"name": "state", "about": "What the player is doing, in small capitals: now playing, paused or idle.", "type": "text", "default": "now playing"},
    {"name": "tone", "about": "accent-text while it plays, text-dim otherwise.", "type": "select", "options": ["text-dim", "accent-text"], "default": "accent-text"},
    {"name": "title", "about": "The track, up to two lines.", "type": "text", "default": "Harbour Lights"},
    {"name": "artist", "about": "The artist, one line.", "type": "text", "default": "The Lanterns"},
    {"name": "progress", "about": "How far through the track it is, from 0 to 100.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 38},
    {"name": "playing", "about": "Whether it is playing: the picture turns.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "CD · playing", "props": {}},
    {"label": "disc · paused", "props": {"visual": "disc", "state": "paused", "tone": "text-dim", "playing": false, "progress": 72}},
    {"label": "cassette · long title", "props": {"visual": "cassette", "title": "A very long track title that needs two lines and more", "artist": "Somebody with a long name and a band"}},
    {"label": "idle", "props": {"visual": "disc", "state": "idle", "tone": "text-dim", "title": "Nothing playing", "artist": "", "playing": false, "progress": 0}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  fx::name(root, "media_hero");
  fill(root, p);
}

FW_UPDATE(media_hero) {
  lv_obj_t *slot = lv_obj_get_child(root, 0), *col = lv_obj_get_child(root, 1);
  if (!slot || !col || !fw::shape(root, sig(p), "hero")) {
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  ArduinoJson::JsonDocument d;
  visual_props(p, d);
  if (lv_obj_t *v = lv_obj_get_child(slot, 0)) fw::update_child(fw::find("media_visual"), v, d.as<fw::Props>());
  fx::set_text(lv_obj_get_child(col, 0), p["state"] | "idle", "type:label");
  lv_obj_t *ti = lv_obj_get_child(col, 1);
  fx::set_text(ti, p["title"] | "Nothing playing", "type:headline");
  fx::set_text(lv_obj_get_child(col, 2), p["artist"] | "", "type:body");
  return true;
}
