// GitHub PR: the GitHub screen's detail body (docs screens/github.ts detailView; the factory spec renderer/specs/
// github_pr.json adds the header "Pull request" and takes the pull request's id as its "pr" parameter): a left column
// of text, a QR code to carry on on your phone, and the checks in two columns.
//   left     "lantern · #412" (type label, text-dim), the title (title, text, up to two lines), "sam-okafor · 26h ·
//            6 files · +120 −14" (body, text-dim), the statuses (real Status chips: review / assigned warn, ci pass ok,
//            ci fail error, ci running working, draft idle), the description (body, text, up to three lines)
//   right    a real QR block (lay::qr_block_w) of the pull request's link, "scan to open"
//   checks   "checks · 4 · 1 failing" (label, text-dim), then real Check rows in two columns (failing first)
// Feed: core/github.h (pr = the id; an unknown one shows "Pull request not found"). Static to touch; update path:
// rebuilt in place. Contrast: status_chip, qr_block, check_row, text.
// audit deps: check_row, qr_block, status_chip
#include "fw.h"
#include "github.h"

namespace {
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void fill(lv_obj_t *root, const char *id) {
  const fw::Theme &t = *fw::T;
  lv_obj_clean(root);
  const fw::gh::Data d = fw::gh::sample();
  bool mine = false;
  const fw::gh::PR *p = fw::gh::find(d, id, &mine);
  if (!p) { fw::text(root, fw::ty::body, "Pull request not found", t.text_dim); return; }
  const int QW = fw::lay::qr_block_w, PW = fw::lay::content_w - QW - fw::lay::gutter;
  lv_obj_t *top = box(root);
  lv_obj_set_size(top, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *left = box(top);
  lv_obj_set_size(left, PW, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(left, fw::sp::s3, 0);
  char b[128];
  snprintf(b, sizeof b, "%s \xC2\xB7 #%d", p->repo, p->number);
  fw::text(left, fw::ty::label, b, t.text_dim, PW);
  lv_obj_t *ti = fw::text(left, fw::ty::title, p->title, t.text, PW);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_max_height(ti, 2 * fw::style(fw::ty::title).box, 0);
  snprintf(b, sizeof b, "%s \xC2\xB7 %s \xC2\xB7 %d files \xC2\xB7 +%d \xE2\x88\x92%d", p->author, fw::gh::age(p->age_h).c_str(), p->files, p->additions, p->deletions);
  lv_obj_t *by = fw::text(left, fw::ty::body, b, t.text_dim, PW);
  lv_label_set_long_mode(by, LV_LABEL_LONG_DOT);
  fw::bleed(by, fw::sp::s1);  // a first glyph may overhang left of the box (the j of "jo-marsh")
  lv_obj_t *st = box(left);
  lv_obj_set_size(st, PW, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(st, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(st, fw::sp::s3, 0);
  lv_obj_set_style_pad_row(st, fw::sp::s3, 0);
  for (const fw::gh::Status &s : fw::gh::statuses(*p, mine)) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = s.tone, cp["text"] = s.text;
    build("status_chip", st, cp);
  }
  lv_obj_t *body = fw::text(left, fw::ty::body, *p->body ? p->body : "No description.", t.text, PW);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_max_height(body, 3 * fw::style(fw::ty::body).box, 0);
  ArduinoJson::JsonDocument qp;
  qp["text"] = p->url(), qp["caption"] = "scan to open";
  build("qr_block", top, qp);
  int fails = 0;
  for (const fw::gh::Check &c : p->checks) fails += !strcmp(c.state, "fail");
  if (p->checks.empty()) snprintf(b, sizeof b, "checks");
  else if (fails) snprintf(b, sizeof b, "checks \xC2\xB7 %zu \xC2\xB7 %d failing", p->checks.size(), fails);
  else snprintf(b, sizeof b, "checks \xC2\xB7 %zu", p->checks.size());
  lv_obj_t *k = fw::text(root, fw::ty::label, b, t.text_dim);
  lv_obj_set_style_margin_top(k, fw::lay::group, 0);
  if (p->checks.empty()) { fw::text(root, fw::ty::body, "No checks on the head commit.", t.text_dim); return; }
  lv_obj_t *grid = box(root);
  lv_obj_set_size(grid, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(grid, fw::lay::gutter, 0);
  const int CW2 = (fw::lay::content_w - fw::lay::gutter) / 2;
  std::vector<fw::gh::Check> sorted = p->checks;  // failing first, then running, then the rest (docs)
  auto rank = [](const char *s) { return !strcmp(s, "fail") ? 0 : !strcmp(s, "pending") ? 1 : 2; };
  for (size_t i = 1; i < sorted.size(); i++)
    for (size_t j = i; j > 0 && rank(sorted[j].state) < rank(sorted[j - 1].state); j--) std::swap(sorted[j], sorted[j - 1]);
  for (const fw::gh::Check &c : sorted) {
    ArduinoJson::JsonDocument cp;
    cp["name"] = c.name, cp["state"] = c.state, cp["width"] = (int) std::lround(CW2 / fw::D);
    build("check_row", grid, cp);
  }
}
}  // namespace

FW_COMPONENT(github_pr, "GitHub PR", "components", R"({
  "about": "The GitHub screen's pull request: the text on the left, a QR code to carry on on your phone, and the checks.",
  "frame": [480, 0],
  "props": [
    {"name": "pr", "about": "For previews only: which made-up pull request to show. n1 to n3 are ones that need your review; m1 to m3 are your own.", "type": "select", "options": ["n1", "n2", "n3", "m1", "m2", "m3"], "default": "n1"}
  ],
  "variants": [
    {"label": "needs a review · checks running", "props": {"pr": "n1"}},
    {"label": "both · a failing check", "props": {"pr": "n3"}},
    {"label": "your draft · no checks", "props": {"pr": "m3"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s3, 0);
  fill(root, p["pr"] | "n1");
}

FW_UPDATE(github_pr) {
  fill(root, p["pr"] | "n1");
  return true;
}
