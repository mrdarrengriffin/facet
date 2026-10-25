// Mascot mood: the Facet mascot in a mood (docs: Atoms > Mascot, @facet/ui mascot.ts mascotMood() + moodFor()): the
// core mascot (fw::mascot, tokens/mascot.json: body, eyes, brows, "!", z, dots, each mood's own motion) with its mood
// either picked directly or worked out from what an agent is doing (core fw::mascot_mood_for = docs moodFor: error /
// failed in the status = error, working, needs, idle = sleeping, done within 2 h = done else idle), the way the
// Agents tiles pick it. Under it the mood's name (c-caps-sm, text-dim) and what it means (c-meta, text-dim).
//   colour  the mood's own token (docs MOOD_INFO) or a tile's own colour (accent / steel / warn)
// Static to touch. Update path: a new mood moves in place (fw::mascot_set: its own motion; becoming done hops) and the
// words change, silent; a new colour refills. Contrast pairs: colour.json section "mascot" (+ "mascot_mood").
#include "fw.h"

namespace {
fw::mas::Mood mood_of(fw::Props p) {
  if (!strcmp(p["from"] | "mood", "agent")) return fw::mascot_mood_for(p["kind"] | "working", p["status"] | "");
  return fw::mascot_mood(p["mood"] | "working");
}
const char *colour_of(fw::Props p) {
  const char *c = p["colour"] | "mood";
  return strcmp(c, "mood") ? c : nullptr;
}
void words(lv_obj_t *col, fw::mas::Mood m) {
  const fw::mas::MoodDef &d = fw::mas::MOODS[m];
  lv_obj_t *n = lv_obj_get_child(col, 1), *w = lv_obj_get_child(col, 2);
  fw::set_text(n, fw::ty::c_caps_sm, d.label);
  fw::set_text(w, fw::ty::c_meta, d.what);
}
void fill(lv_obj_t *col, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(col);
  lv_obj_remove_flag(col, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(col, fw::lay::card_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, fw::sp::s5, 0);
  fw::MascotOpts o;
  o.colour = colour_of(p);
  o.scale = std::max(1, std::min(4, (int) (p["scale"] | 2)));
  fw::mascot(col, mood_of(p), o);
  lv_obj_t *n = fw::text(col, fw::ty::c_caps_sm, "", t.text_dim);
  lv_obj_set_height(n, fw::style(fw::ty::c_caps_sm).box);
  lv_obj_t *w = fw::text(col, fw::ty::c_meta, "", t.text_dim, fw::lay::card_w);
  lv_obj_set_style_text_align(w, LV_TEXT_ALIGN_CENTER, 0);
  words(col, mood_of(p));
  fw::shape(col, std::string(p["colour"] | "mood") + "|" + std::to_string((int) (p["scale"] | 2)));
}
}  // namespace

FW_COMPONENT(mascot_mood, "Mascot mood", "components", R"({
  "about": "The mascot in a mood, picked directly or from what an agent is doing: working, needs you, done, error, idle or asleep.",
  "frame": [352, 224],
  "props": [
    {"name": "from", "about": "How the mood is chosen: mood picks it directly; agent works it out from what an agent is doing.", "type": "select", "options": ["mood", "agent"], "default": "mood"},
    {"name": "mood", "about": "The mascot's mood when picked directly: idle, working, done, needs you, error or sleeping.", "type": "select", "options": ["idle", "working", "done", "needs", "error", "sleeping"], "default": "working"},
    {"name": "kind", "about": "When the mood comes from an agent: what the agent is doing, working, needs you, done or idle.", "type": "select", "options": ["working", "needs", "done", "idle"], "default": "done"},
    {"name": "status", "about": "When the mood comes from an agent: its status line, such as done 40m ago. A task done long ago turns the mascot idle.", "type": "text", "default": "done · 40m ago"},
    {"name": "colour", "about": "The mascot's colour: mood uses the mood's own colour, or accent, steel or warn for one fixed colour.", "type": "select", "options": ["mood", "accent", "steel", "warn"], "default": "mood"},
    {"name": "scale", "about": "How big the mascot is drawn, from 1 (screen size) to 4 times.", "type": "range", "min": 1, "max": 4, "step": 1, "default": 2}
  ],
  "variants": [
    {"label": "working", "props": {"mood": "working"}},
    {"label": "needs you", "props": {"mood": "needs"}},
    {"label": "done", "props": {"mood": "done"}},
    {"label": "error", "props": {"mood": "error"}},
    {"label": "asleep", "props": {"mood": "sleeping"}},
    {"label": "agent · done 40m ago", "props": {"from": "agent", "kind": "done", "status": "done · 40m ago"}},
    {"label": "agent · done 3h ago = idle", "props": {"from": "agent", "kind": "done", "status": "done · 3h ago"}},
    {"label": "agent · build failed", "props": {"from": "agent", "kind": "working", "status": "build failed"}},
    {"label": "screen size", "props": {"scale": 1}}
  ]
})") {
  lv_obj_t *col = lv_obj_create(parent);
  fill(col, p);
}

FW_UPDATE(mascot_mood) {
  if (!fw::shape(root, std::string(p["colour"] | "mood") + "|" + std::to_string((int) (p["scale"] | 2)))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  fw::mascot_set(lv_obj_get_child(root, 0), mood_of(p), colour_of(p));
  words(root, mood_of(p));
  return true;
}
