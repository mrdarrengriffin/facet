// GitHub PR: the top of the GitHub screen's pull request view (docs @facet/ui screens/github.ts detailView, CSS
// .fs-d-*), on the contract (renderer/CONTRACT.md, "contract": 2): a left column of text and, at the right edge, a QR
// code to carry on on your phone. The checks under it are the spec's (a Section header + a Grid of Status lines). What
// nothing else draws: this column beside the code. Data: the github feed (core/github_source.cpp) through the spec.
//   box      fills its parent's width (C5), a row, tops aligned
//   column   w:pr-col-w (400 / 268), gap:5 between its lines (docs 16):
//            meta     type:label caps, text-dim, one line "..." ("lantern · quiet-hours → main"; docs text-mute: C21)
//            title    type:headline, text, h:pr-title-h exactly: two lines, then "..." (docs line-clamp 2)
//            by       type:body, text-dim, one line "..." ("sam-okafor · 26h · 6 files · +120 −14")
//            status   a row h:gh-row-top exactly (docs 24), gap:7 (docs space-7): each a real Status chip at size dot (C8b) in its tone, space 2, its
//                     word (type:label caps, fg:text; the docs colour the word in the tone, under 4.5:1 in light)
//            body     above:2 more (docs 248 = 200 + 24 + 24), type:body, text-dim, h:pr-body-h exactly: three lines
//   code     a real QR block (C8b; still legacy inside) of the pull request's link, "scan to open", at the right edge
// Steps: none (the column is a fixed token, the code its own). Bleed: none. Touch: none (static: no events, no sound).
// States: n/a (the feed's empty / error = the screen's words). Update path: the words and the code in place; another
// set of statuses = made again, faded like a tap. Silent. Contrast: colour.json "github_pr" + Status chip + QR block.
#include "fw.h"
#include "fx.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
std::string statuses(fw::Props p) {  // the statuses as JSON text: [{"tone": "warn", "text": "review"}, ...]
  if (p["statuses"].is<const char *>()) return p["statuses"].as<std::string>();
  std::string s;
  serializeJson(p["statuses"], s);
  return s;
}
const char *tone(const char *t) {  // feed data never aborts the chip: a closed list
  static const char *const OK[] = {"ok", "warn", "error", "working", "info", "idle"};
  for (const char *k : OK) if (!strcmp(t, k)) return k;
  return "idle";
}
void qr_props(fw::Props p, Doc &d) { d["text"] = p["url"] | ""; d["caption"] = "scan to open"; }
lv_obj_t *fixed_text(lv_obj_t *col, const char *s, const char *type, const char *fg, const char *h, const char *max) {
  lv_obj_t *l = fx::text(col, s, type, fg);
  fx::use(l, "fill-w");
  if (h) for (const char *t : {h, max}) fx::use(l, t);  // a fixed box of lines (else one line)
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);  // its lines, then "..."
  return l;
}
void fill(lv_obj_t *root, fw::Props p) {
  const std::string st = statuses(p);
  fw::shape(root, st.c_str());
  for (const char *t : {"box", "row-top"}) fx::use(root, t);
  lv_obj_t *col = fx::box(root, "col");
  for (const char *t : {"w:pr-col-w", "gap:5"}) fx::use(col, t);
  fixed_text(col, p["meta"] | "", "type:label", "fg:text-dim", nullptr, nullptr);
  fixed_text(col, p["title"] | "", "type:headline", "fg:text", "h:pr-title-h", "max-h:pr-title-h");
  fixed_text(col, p["by"] | "", "type:body", "fg:text-dim", nullptr, nullptr);
  lv_obj_t *row = fx::box(col, "row-mid");
  for (const char *t : {"gap:7", "h:gh-row-top", "max-h:gh-row-top"}) fx::use(row, t);  // docs .fs-d-status: 24 tall
  Doc sd;
  deserializeJson(sd, st);
  for (ArduinoJson::JsonVariantConst s : sd.as<ArduinoJson::JsonArrayConst>()) {
    lv_obj_t *one = fx::box(row, "row-mid");
    for (const char *t : {"hug", "gap:2"}) fx::use(one, t);
    Doc d;
    d["tone"] = tone(s["tone"] | "idle"), d["size"] = "dot";
    fw::find("status_chip")->build(one, d.as<fw::Props>());
    fx::text(one, s["text"] | "", "type:label", "fg:text");
  }
  lv_obj_t *body = fixed_text(col, p["body"] | "", "type:body", "fg:text-dim", "h:pr-body-h", "max-h:pr-body-h");
  fx::use(body, "above:2");
  fx::use(fx::box(root, "free"), "grow");
  Doc q;
  qr_props(p, q);
  fw::find("qr_block")->build(root, q.as<fw::Props>());
}
}  // namespace

FW_COMPONENT(github_pr, "GitHub PR", "components", R"({
  "about": "The top of a pull request on the GitHub screen: its words on the left and a QR code to carry on on your phone.",
  "contract": 2,
  "parts": [],
  "frame": [656, 0],
  "props": [
    {"name": "meta", "about": "The small line at the top: the repository and the branches.", "type": "text", "default": "lantern · quiet-hours → main"},
    {"name": "title", "about": "The pull request's title, up to two lines.", "type": "text", "default": "Add a quiet hours schedule to the notification router"},
    {"name": "by", "about": "Who opened it, how long ago and how big it is.", "type": "text", "default": "sam-okafor · 26h · 6 files · +120 −14"},
    {"name": "statuses", "about": "Why it needs you, draft and CI: a JSON list of {tone, text}, each shown as a dot and a word.", "type": "text", "default": "[{\"tone\": \"warn\", \"text\": \"review\"}, {\"tone\": \"working\", \"text\": \"ci running\"}]"},
    {"name": "body", "about": "The start of the description, up to three lines.", "type": "text", "default": "Holds non-urgent notifications between 22:00 and 07:00. Urgent ones still come through. Adds a settings screen and tests."},
    {"name": "url", "about": "The pull request's web address, for the QR code.", "type": "text", "default": "https://example.com/orchard-labs/lantern/pull/412"}
  ],
  "variants": [
    {"label": "needs a review · checks running", "props": {}},
    {"label": "your own · approved", "props": {"meta": "lantern · pi-docs → main", "title": "Docs: getting started on a Raspberry Pi", "by": "you · 2d · 6 files · +120 −14", "statuses": "[{\"tone\": \"ok\", \"text\": \"approved\"}, {\"tone\": \"ok\", \"text\": \"ci pass\"}]", "body": "No description."}},
    {"label": "long title", "props": {"title": "A very long pull request title that needs two lines and then some more words to cut"}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "row-top");
  fx::name(root, "github_pr");
  fill(root, p);
}

FW_UPDATE(github_pr) {
  if (!fw::shape(root, statuses(p).c_str())) {  // other statuses: made again, faded like a tap
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  lv_obj_t *col = lv_obj_get_child(root, 0);
  fx::set_text(lv_obj_get_child(col, 0), p["meta"] | "", "type:label");
  fx::set_text(lv_obj_get_child(col, 1), p["title"] | "", "type:headline");
  fx::set_text(lv_obj_get_child(col, 2), p["by"] | "", "type:body");
  fx::set_text(lv_obj_get_child(col, 4), p["body"] | "", "type:body");
  Doc q;
  qr_props(p, q);
  fw::update_child(fw::find("qr_block"), lv_obj_get_child(root, 2), q.as<fw::Props>());
  return true;
}
