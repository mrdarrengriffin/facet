// Big number: the Facet big number (docs: Components > Charts > Big number, @facet/ui charts.ts bigNumber(), CSS
// .fch-big): lay::card_w wide (docs width 320), a caps label over a big value and its unit; it counts up when shown. Built with the Charts foundation's
// big number (core/chart.h fw::chart::big_number, the one the Charts page shows): label type c-caps-sm text-dim,
// sp::s2, the value (type c-number, text) with the unit beside it (c-subtitle, text-dim) on the baseline.
//   count   showing it counts 0 -> value (docs play(): 700 ms ease-out; here Motion value, 400 ms ease-out, the
//           gauge / count chart motion), the label's box only repaints; reduced motion = the value at once
//   unavailable  the value is the placeholder em dash (st::PLACEHOLDER), no count
// A read-out: no events, no sound. Update path: a new value counts from the shown one (silent), label / unit / prefix /
// decimals change the text at once; unavailable in / out refills. The value may come as text (a bound HA state). Contrast pairs: colour.json section "chart".
#include "fw.h"
#include "chart.h"
#include <cmath>
#include <cstdlib>

namespace {
struct Big {
  lv_obj_t *val = nullptr, *cap = nullptr, *unit = nullptr;
  double shown = 0, target = 0;
  int dec = 2;
  std::string prefix;
  bool unavailable = false;
};
Big *of(lv_obj_t *o) { return (Big *) lv_obj_get_user_data(o); }
// the value: a number, or a number as text (a bound HA state arrives as text: "23.4"); text that isn't a number (an
// empty bind, "unknown") = no value, shown as unavailable; no value prop at all = the demo 8.69
bool value_of(fw::Props p, double *v) {
  *v = 8.69;
  if (p["value"].is<const char *>()) {
    const char *s = p["value"];
    char *end = nullptr;
    const double x = strtod(s, &end);
    if (end == s || !std::isfinite(x)) return false;
    *v = x;
    return true;
  }
  *v = p["value"] | 8.69;
  return true;
}
std::string fmt(const Big *d, double v) {
  char b[32];
  snprintf(b, sizeof b, "%s%.*f", d->prefix.c_str(), d->dec, v);
  return b;
}
void show(void *o, int32_t k) {  // k = 0..1024 of the way from 0 (or the old value) to the target
  auto *d = of((lv_obj_t *) o);
  const double v = d->shown + (d->target - d->shown) * k / 1024.0;
  const std::string s = fmt(d, v);
  if (s != lv_label_get_text(d->val)) lv_label_set_text(d->val, s.c_str());
  if (k == 1024) d->shown = d->target;
}
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  auto *d = of(root);
  d->dec = std::max(0, std::min(3, (int) (p["decimals"] | 2)));
  d->prefix = p["prefix"] | "";
  d->unavailable = !value_of(p, &d->target) || !strcmp(p["state"] | "default", "unavailable");
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::card_w, LV_SIZE_CONTENT);  // docs width (the sample's 320 = a card), so a count never moves it
  d->val = fw::chart::big_number(root, t, p["label"] | "today · so far", d->unavailable ? fw::st::PLACEHOLDER : "0", p["unit"] | "kWh");
  lv_obj_t *col = lv_obj_get_child(root, 0);
  d->cap = lv_obj_get_child(col, 0);
  d->unit = lv_obj_get_child(lv_obj_get_child(col, 1), 1);
  if (d->unavailable) { lv_obj_set_style_text_color(d->val, fw::st::unavailable_ink(t), 0); return; }
  d->shown = 0;
  if (opening) fw::animate(root, fw::st::SWEEP, {show, 0, 1024, fw::mo::VALUE});
  else { d->shown = d->target; show(root, 1024); }
}
std::string shape_of(fw::Props p) {
  double v;
  return value_of(p, &v) ? std::string(p["state"] | "default") : std::string("unavailable");
}
}  // namespace

FW_COMPONENT(big_number, "Big number", "components", R"({
  "about": "A caps label over a big value and its unit. It counts up when shown.",
  "frame": [256, 96],
  "props": [
    {"name": "label", "about": "The small capitals line above the number that says what it is, such as today so far.", "type": "text", "default": "today · so far"},
    {"name": "value", "about": "The number itself. It counts up to this value when the number first appears.", "type": "range", "min": 0, "max": 40, "step": 0.01, "default": 8.69},
    {"name": "unit", "about": "The unit shown after the number, such as kWh or %. Leave empty for none.", "type": "text", "default": "kWh"},
    {"name": "decimals", "about": "How many digits are shown after the decimal point.", "type": "range", "min": 0, "max": 3, "step": 1, "default": 2},
    {"name": "prefix", "about": "Text shown just before the number, such as a currency sign. Leave empty for none.", "type": "text", "default": ""},
    {"name": "state", "debug": true, "about": "default shows the number; unavailable shows a dash in place of it when the value cannot be read.", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "opening": "Showing it counts up from 0 (Motion value).",
  "variants": [
    {"label": "kWh so far", "props": {}},
    {"label": "cost · £ prefix", "props": {"label": "cost today", "value": 2.43, "prefix": "£", "unit": ""}},
    {"label": "whole number", "props": {"label": "outside", "value": 17, "decimals": 0, "unit": "°C"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Big;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_set_user_data(root, d);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (Big *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fill(root, p, p["opening"] | true);
  fw::shape(root, shape_of(p));
}

FW_UPDATE(big_number) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, shape_of(p))) {
    fw::refill(root);
    fill(root, p, false);
    return true;
  }
  if (d->unavailable) return true;
  fw::set_text(d->cap, fw::ty::c_caps_sm, p["label"] | "today · so far");
  fw::set_text(d->unit, fw::ty::c_subtitle, p["unit"] | "kWh");
  d->dec = std::max(0, std::min(3, (int) (p["decimals"] | 2)));
  d->prefix = p["prefix"] | "";
  double v;
  value_of(p, &v);
  lv_anim_delete(root, (lv_anim_exec_xcb_t) show);
  d->target = v;
  fw::animate(root, fw::st::SWEEP, {show, 0, 1024, fw::mo::VALUE});  // counts from the shown value (silent: no sound)
  return true;
}
