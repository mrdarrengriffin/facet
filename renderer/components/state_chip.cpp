// State chip: the Facet state chips for pull requests (docs: Components > Lists > State chip, @facet/ui lists.ts
// stateChip() + CHIP): review state, CI and why it needs you, each a Status chip (fw::find("status_chip")) with the docs
// tone and words: approved ok, changes requested error, review pending idle, draft idle, ci pass ok, ci fail error,
// ci running working, review warn, assigned info. Static. Update path: the Status chip's own (tone + text in place).
// Contrast pairs: colour.json section "status_chip".
#include "fw.h"

namespace {
struct Kind { const char *id, *tone, *text; };
constexpr Kind KINDS[] = {{"approved", "ok", "approved"},       {"changes", "error", "changes requested"},
                          {"pending", "idle", "review pending"}, {"draft", "idle", "draft"},
                          {"ci-pass", "ok", "ci pass"},          {"ci-fail", "error", "ci fail"},
                          {"ci-pending", "working", "ci running"}, {"review", "warn", "review"},
                          {"assigned", "info", "assigned"}};
void chip_props(fw::Props p, ArduinoJson::JsonDocument &cp) {
  const char *k = p["kind"] | "review";
  const Kind *m = &KINDS[7];
  for (const Kind &x : KINDS) if (!strcmp(x.id, k)) m = &x;
  cp["tone"] = m->tone;
  cp["text"] = m->text;
}
}  // namespace

FW_COMPONENT(state_chip, "State chip", "components", R"({
  "about": "Chips for pull requests: review state, CI and why it needs you.",
  "frame": [256, 64],
  "props": [
    {"name": "kind", "type": "select", "options": ["approved", "changes", "pending", "draft", "ci-pass", "ci-fail", "ci-pending", "review", "assigned"], "default": "review"}
  ],
  "variants": [
    {"label": "approved", "props": {"kind": "approved"}},
    {"label": "changes", "props": {"kind": "changes"}},
    {"label": "pending", "props": {"kind": "pending"}},
    {"label": "draft", "props": {"kind": "draft"}},
    {"label": "ci-pass", "props": {"kind": "ci-pass"}},
    {"label": "ci-fail", "props": {"kind": "ci-fail"}},
    {"label": "ci-pending", "props": {"kind": "ci-pending"}},
    {"label": "review", "props": {"kind": "review"}},
    {"label": "assigned", "props": {"kind": "assigned"}}
  ]
})") {
  const fw::Component *c = fw::find("status_chip");
  if (!c) return;
  ArduinoJson::JsonDocument cp;
  chip_props(p, cp);
  c->build(parent, cp.as<fw::Props>());
}

FW_UPDATE(state_chip) {
  ArduinoJson::JsonDocument cp;
  chip_props(p, cp);
  return fw::update_child(fw::find("status_chip"), root, cp.as<fw::Props>());
}
