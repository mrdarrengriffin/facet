// Agents view: the Agents screen's body (docs screens/agents.ts agentsScreen; the factory spec renderer/specs/agents.json
// adds the header "Agents"): how many agents need you (a real Hero block: the number, "need you", "2 working · 5 done
// or idle"; accent while any need you), then one real Tile per agent, three across (lay::tile_w x 3 + 2 gutters = the
// content width at both densities), sorted needs > working > done > idle (docs ORDER), a gutter apart both ways, the
// tiles lay::group under the hero. No agents = a real Empty state ("No agents yet") in place of the tiles.
// Feed: props sample (mixed = the docs story, nine agents; calm = nobody needs you; empty = none). Every agent host is
// different (a coding agent hub, a task runner), so the live source is a JSON feed the user points the screen at
// (sources.json "agents"); until a host feeds it, the sample. A tile tap = the Tile's own (event tap + sound open).
// Update path: refilled in place (tiles change shape / order). Contrast: hero_block, tile, empty_state.
// audit deps: hero_block, tile, empty_state
#include "fw.h"

namespace {
struct Agent { const char *title, *kind, *status; };
// the docs sample (@facet/demo-data AGENTS)
const Agent MIXED[] = {
    {"Photo backup", "working", "working \xC2\xB7 42% ctx"},     {"Recipe importer", "needs", "needs you \xC2\xB7 12m"},
    {"Garden sensors", "done", "done \xC2\xB7 1h ago"},          {"Firmware build", "working", "working \xC2\xB7 18% ctx"},
    {"Calendar tidy-up", "done", "done \xC2\xB7 5h ago"},        {"Invoice sorter", "needs", "needs you \xC2\xB7 2h"},
    {"Bird feeder camera", "done", "done \xC2\xB7 1d ago"},      {"Plant watering plan", "idle", "idle \xC2\xB7 1w ago"},
    {"Docs spellcheck", "done", "done \xC2\xB7 3d ago"},
};
const Agent CALM[] = {
    {"Photo backup", "working", "working \xC2\xB7 42% ctx"},
    {"Garden sensors", "done", "done \xC2\xB7 1h ago"},
    {"Plant watering plan", "idle", "idle \xC2\xB7 1w ago"},
};
int order(const char *k) { return !strcmp(k, "needs") ? 0 : !strcmp(k, "working") ? 1 : !strcmp(k, "done") ? 2 : 3; }

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void fill(lv_obj_t *root, fw::Props p) {
  lv_obj_clean(root);
  const std::string sample = p["sample"] | "mixed";
  std::vector<Agent> list;
  if (sample == "mixed") list.assign(std::begin(MIXED), std::end(MIXED));
  else if (sample == "calm") list.assign(std::begin(CALM), std::end(CALM));
  std::stable_sort(list.begin(), list.end(), [](const Agent &a, const Agent &b) { return order(a.kind) < order(b.kind); });
  int needs = 0, working = 0;
  for (const Agent &a : list) needs += !strcmp(a.kind, "needs"), working += !strcmp(a.kind, "working");

  ArduinoJson::JsonDocument hp;
  hp["n"] = std::to_string(needs);
  hp["label"] = needs == 1 ? "needs you" : "need you";
  char b[64];
  snprintf(b, sizeof b, "%d working \xC2\xB7 %d done or idle", working, (int) list.size() - needs - working);
  hp["line"] = list.empty() ? "no agents yet" : b;
  hp["accent"] = needs > 0;
  build("hero_block", root, hp);

  lv_obj_t *tiles = lv_obj_create(root);
  lv_obj_remove_style_all(tiles);
  lv_obj_remove_flag(tiles, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(tiles, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_margin_top(tiles, fw::lay::group, 0);
  if (list.empty()) {
    ArduinoJson::JsonDocument ep;
    ep["icon"] = "done", ep["title"] = "No agents yet", ep["body"] = "Agents show here once a feed is set", ep["tone"] = "idle";
    build("empty_state", tiles, ep);
    return;
  }
  lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(tiles, fw::lay::gutter, 0);
  lv_obj_set_style_pad_row(tiles, fw::lay::gutter, 0);
  for (const Agent &a : list) {
    ArduinoJson::JsonDocument tp;
    tp["title"] = a.title, tp["kind"] = a.kind, tp["status"] = a.status;
    build("tile", tiles, tp);
  }
}
}  // namespace

FW_COMPONENT(agents_view, "Agents view", "components", R"({
  "about": "The Agents screen's body: how many agents need you, then a tile per agent, three across, the ones that need you first.",
  "frame": [480, 0],
  "props": [
    {"name": "sample", "about": "For previews only: which set of made-up agents to show. mixed has some needing you, calm has none needing you, empty has no agents at all.", "type": "select", "options": ["mixed", "calm", "empty"], "default": "mixed"}
  ],
  "variants": [
    {"label": "two need you", "props": {}},
    {"label": "nobody needs you", "props": {"sample": "calm"}},
    {"label": "no agents", "props": {"sample": "empty"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  fill(root, p);
}

FW_UPDATE(agents_view) {
  fill(root, p);
  return true;
}
