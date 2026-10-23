// GitHub view: the GitHub screen's summary body (docs screens/github.ts sumView; the factory spec renderer/specs/
// github.json adds the header): how many pull requests need you (a real Hero block: the number, "need you", "2 reviews
// · 1 assigned"; accent while any need you), the top three of them as real PR rows, and "all pull requests" (a More
// row). A row opens that pull request (emits "open" github_pr?{"pr": id}), the More row the full list (github_list):
// the spec's nav.open. Feed: core/github.h (the docs story until a host feeds GitHub). Static to touch apart from the
// links; update path: rebuilt in place (rows change shape). Contrast: hero_block, pr_row, more_row.
// audit deps: hero_block, more_row, pr_row
#include "fw.h"
#include "github.h"
#include "links.h"

namespace {
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void fill(lv_obj_t *root) {
  lv_obj_clean(root);
  const fw::gh::Data d = fw::gh::sample();
  int reviews = 0, assigned = 0;
  for (const fw::gh::PR &p : d.needs) {
    const std::string r = p.reason;
    if (r == "review" || r == "both") reviews++;
    if (r == "assigned" || r == "both") assigned++;
  }
  ArduinoJson::JsonDocument hp;
  hp["n"] = std::to_string(d.needs.size());
  hp["label"] = "need you";
  char b[64];
  snprintf(b, sizeof b, "%d review%s \xC2\xB7 %d assigned", reviews, reviews == 1 ? "" : "s", assigned);
  hp["line"] = d.needs.empty() ? "nothing to review" : b;
  hp["accent"] = !d.needs.empty();
  build("hero_block", root, hp);
  lv_obj_t *rows = lv_obj_create(root);
  lv_obj_remove_style_all(rows);
  lv_obj_remove_flag(rows, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(rows, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_margin_top(rows, fw::lay::group, 0);
  for (size_t i = 0; i < d.needs.size() && i < 3; i++) {
    const fw::gh::PR &p = d.needs[i];
    lv_obj_t *l = fw::link_box(root, rows, std::string("github_pr?{\"pr\": \"") + p.id + "\"}");
    ArduinoJson::JsonDocument rp;
    rp["repo"] = p.repo, rp["number"] = p.number, rp["title"] = p.title, rp["age"] = fw::gh::age(p.age_h);
    rp["draft"] = p.draft, rp["ci"] = p.ci;
    build("pr_row", l, rp);
  }
  lv_obj_t *m = fw::link_box(root, rows, "github_list");
  ArduinoJson::JsonDocument mp;
  mp["text"] = "all pull requests";
  build("more_row", m, mp);
}
}  // namespace

FW_COMPONENT(github_view, "GitHub view", "components", R"({
  "about": "The GitHub screen's summary: how many pull requests need you, the top three, and the way to all of them.",
  "frame": [480, 0],
  "props": [],
  "variants": [
    {"label": "three need you", "props": {}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  fill(root);
}

FW_UPDATE(github_view) {
  fill(root);
  return true;
}
