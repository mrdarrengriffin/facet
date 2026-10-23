// GitHub list: the GitHub screen's "all pull requests" body (docs screens/github.ts listView; the factory spec
// renderer/specs/github_list.json adds the header "Pull requests" and scrolls it): two sections, "Need you" and "Your
// PRs" (real Section headers with the count, warn / idle), each pull request a real PR row; a row opens it (emits
// "open" github_pr?{"pr": id}: the spec's nav.open). An empty section says so (body, text-dim). Feed: core/github.h.
// Update path: rebuilt in place. Contrast: section_header, pr_row, text.
// audit deps: pr_row, section_header
#include "fw.h"
#include "github.h"
#include "links.h"

namespace {
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void section(lv_obj_t *root, const char *title, const std::vector<fw::gh::PR> &prs, const char *tone, const char *none) {
  ArduinoJson::JsonDocument sp;
  sp["title"] = title, sp["badge"] = true, sp["count"] = (int) prs.size(), sp["tone"] = tone, sp["right"] = "none";
  lv_obj_t *h = build("section_header", root, sp);
  if (h) lv_obj_set_style_margin_top(h, lv_obj_get_index(h) ? fw::lay::group : 0, 0);
  if (prs.empty()) { fw::text(root, fw::ty::body, none, fw::T->text_dim); return; }
  for (const fw::gh::PR &p : prs) {
    lv_obj_t *l = fw::link_box(root, root, std::string("github_pr?{\"pr\": \"") + p.id + "\"}");
    ArduinoJson::JsonDocument rp;
    rp["repo"] = p.repo, rp["number"] = p.number, rp["title"] = p.title, rp["age"] = fw::gh::age(p.age_h);
    rp["draft"] = p.draft, rp["ci"] = p.ci;
    build("pr_row", l, rp);
  }
}
void fill(lv_obj_t *root) {
  lv_obj_clean(root);
  const fw::gh::Data d = fw::gh::sample();
  section(root, "Need you", d.needs, "warn", "Nothing needs you.");
  section(root, "Your PRs", d.mine, "idle", "No open pull requests.");
}
}  // namespace

FW_COMPONENT(github_list, "GitHub list", "components", R"({
  "about": "The GitHub screen's full list: the pull requests that need you and your own, each a row that opens it.",
  "frame": [480, 0],
  "props": [],
  "variants": [
    {"label": "three and three", "props": {}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  fill(root);
}

FW_UPDATE(github_list) {
  fill(root);
  return true;
}
