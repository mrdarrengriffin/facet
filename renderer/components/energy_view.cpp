// Energy view: the Energy screen's summary body (docs screens/energy.ts sumView; the factory spec renderer/specs/
// energy.json adds the header and the "last 7 days" link): today's cost so far as the hero, use against yesterday,
// the current price band and when it changes, and one chart: today's half-hours coloured by price band, with the band
// strip and an hour axis.
//   top      "today so far" (type label, text-dim; a tapped bar's reading in its place: "14:30 · 0.42 kWh · 26.2p"),
//            the cost (hero, text: "£2.42"), "4.2 kWh · 12% less than yesterday by now" (body, text-dim), the rate
//            line (a Status dot: peak working, cheap info, normal idle + body: "Normal 26.2p until 16:00, then 39.4p")
//   chart    a real Bar chart (48 half-hours, bands colours, the now bar, scrub = tap a bar) the content width x
//            lay::chart_h, a real Rate strip under it, the hour axis (00 06 12 18 24) under that
//   more     "last 7 days" (a More row at the foot: emits "open" energy_week, the spec's nav.open)
// Feed (core/energy.h): the sample tariff day until a panel reads its energy sensors; now = a fixed time (ms, tests),
// 0 = live (re-renders every half hour).
//   header   on a spec screen: "420 w now" (type label, text-dim, its width on the grid) in the screen's Header before
//            the time (docs header right [now]; fw::spec::building_header)
// Update path: the texts in place, the chart through its own update (bars move), the strip likewise. Contrast:
// "energy" (+ chart, rate_strip, status_dot, more_row, text).
#include <ctime>
#include "chart.h"
#include "energy.h"
#include "spec.h"
// audit deps: bar_chart, more_row, rate_strip, status_dot
#include "fw.h"

namespace {
struct EV { double fixed = 0; lv_obj_t *now = nullptr, *lbl = nullptr, *big = nullptr, *line = nullptr, *dot = nullptr, *rate = nullptr, *bars = nullptr, *strip = nullptr; lv_timer_t *tick = nullptr; int slot = -2; };
EV *of(lv_obj_t *o) { return (EV *) lv_obj_get_user_data(o); }
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
// a More row inside a listened box: its tap emits "open" <screen> from the view's root (the spec's nav.open)
struct MoreLink { lv_obj_t *root; const char *screen; };
void more(lv_obj_t *root, lv_obj_t *parent, const char *text, const char *screen) {
  if (!fw::spec::screen_known(screen)) return;  // the screen is not in this build (its feature is off): no row
  lv_obj_t *holder = box(parent);
  lv_obj_set_size(holder, fw::lay::content_w, LV_SIZE_CONTENT);
  ArduinoJson::JsonDocument d;
  d["text"] = text;
  build("more_row", holder, d);
  auto *link = new MoreLink{root, screen};
  lv_obj_add_event_cb(holder, [](lv_event_t *e) { delete (MoreLink *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, link);
  fw::listen(holder, [](lv_obj_t *, const char *name, const char *, void *user) {
    if (strcmp(name, fw::st::TAP_EVENT)) return;
    auto *l = (MoreLink *) user;
    fw::emit(l->root, "open", l->screen);
  }, link);
}
int chart_w_dp() { return (int) std::lround(fw::lay::content_w / fw::D) / 8 * 8; }
void chart_props(const fw::energy::Data &d, ArduinoJson::JsonDocument &bp, ArduinoJson::JsonDocument &sp) {
  std::string v, b;
  for (int i = 0; i < 48; i++) {
    char x[16];
    if (std::isnan(d.slots[i].kwh)) snprintf(x, sizeof x, "-");
    else snprintf(x, sizeof x, "%.2f", d.slots[i].kwh);
    v += (i ? "," : "") + std::string(x);
    b += (i ? "," : "") + std::string(fw::energy::BAND_ID[d.slots[i].band]);
  }
  bp["values"] = v, bp["bands"] = b, bp["colour"] = "bands", bp["now"] = d.now_slot;
  bp["width"] = chart_w_dp(), bp["height"] = (int) (fw::ch::chart_h / fw::D), bp["scrub"] = true;  // dp: chart-h
  sp["bands"] = b, sp["now"] = d.now_slot, sp["width"] = chart_w_dp();
}
std::string hm(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
void render(lv_obj_t *root) {
  namespace en = fw::energy;
  EV *e = of(root);
  const double now = e->fixed > 0 ? e->fixed : (double) time(nullptr) * 1000;
  const en::Data d = en::sample(now);
  fw::set_text(e->big, fw::ty::hero, en::money(d.cost).c_str());
  if (e->now) {
    char w[24];
    if (std::isnan(d.watts)) snprintf(w, sizeof w, "-- w now");
    else snprintf(w, sizeof w, "%ld w now", std::lround(d.watts));
    if (strcmp(lv_label_get_text(e->now), w)) { fw::set_text(e->now, fw::ty::label, w); fw::spec::header_fit(e->now); }
  }
  char b[96];
  const double y = d.yesterday_to_now;
  if (!std::isnan(y) && y > 0) {
    const long delta = std::lround((d.kwh - y) / y * 100);
    snprintf(b, sizeof b, "%.1f kWh \xC2\xB7 %ld%% %s than yesterday by now", d.kwh, std::labs(delta), delta <= 0 ? "less" : "more");
  } else snprintf(b, sizeof b, "%.1f kWh", d.kwh);
  fw::set_text(e->line, fw::ty::body, b);
  ArduinoJson::JsonDocument dp;
  dp["tone"] = d.band == en::PEAK ? "working" : d.band == en::CHEAP ? "info" : "idle";
  fw::update_child(fw::find("status_dot"), e->dot, dp.as<fw::Props>());
  std::string name = en::BAND_ID[d.band];
  name[0] = (char) toupper((unsigned char) name[0]);
  snprintf(b, sizeof b, "%s %s until %s, then %s", name.c_str(), en::pence(en::RATE[d.band]).c_str(), hm(d.until).c_str(), en::pence(en::RATE[d.next_band]).c_str());
  fw::set_text(e->rate, fw::ty::body, b);
  if (d.now_slot != e->slot) {  // the chart moves only on a new half hour
    e->slot = d.now_slot;
    ArduinoJson::JsonDocument bp, sp;
    chart_props(d, bp, sp);
    fw::update_child(fw::find("bar_chart"), e->bars, bp.as<fw::Props>());
    fw::update_child(fw::find("rate_strip"), e->strip, sp.as<fw::Props>());
  }
}
}  // namespace

FW_COMPONENT(energy_view, "Energy view", "components", R"({
  "about": "The Energy screen's summary: today's cost so far, use against yesterday, the price now and when it changes, and today's half-hours by price band.",
  "frame": [480, 400],
  "props": [],
  "variants": [
    {"label": "a Thursday afternoon", "props": {"now": 1792926000000}},
    {"label": "evening peak", "props": {"now": 1792947600000}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *e = new EV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, e);
  lv_obj_add_event_cb(root, [](lv_event_t *ev) {
    auto *e = (EV *) lv_event_get_user_data(ev);
    if (e->tick) lv_timer_delete(e->tick);
    delete e;
  }, LV_EVENT_DELETE, e);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s2, 0);
  e->fixed = p["now"] | 0.0;
  const double now = e->fixed > 0 ? e->fixed : (double) time(nullptr) * 1000;
  if (lv_obj_t *hdr = fw::spec::building_header()) {  // the live power in the screen's header
    e->now = fw::text(hdr, fw::ty::label, "", t.text_dim);
    fw::spec::header_place(e->now);
  }
  e->lbl = fw::text(root, fw::ty::label, "today so far", t.text_dim, fw::lay::content_w);
  e->big = fw::text(root, fw::ty::hero, "\xC2\xA3--", t.text);
  e->line = fw::text(root, fw::ty::body, "", t.text_dim, fw::lay::content_w);
  lv_obj_t *rr = box(root);
  lv_obj_set_size(rr, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(rr, fw::sp::s3, 0);
  ArduinoJson::JsonDocument dp;
  dp["tone"] = "idle";
  e->dot = build("status_dot", rr, dp);
  e->rate = fw::text(rr, fw::ty::body, "", t.text);
  lv_obj_t *chart = box(root);  // the chart, its strip and the hour axis
  lv_obj_set_size(chart, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(chart, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(chart, fw::sp::s2, 0);
  lv_obj_set_style_margin_top(chart, fw::sp::s7, 0);
  const fw::energy::Data d = fw::energy::sample(now);
  ArduinoJson::JsonDocument bp, sp;
  chart_props(d, bp, sp);
  e->slot = d.now_slot;
  e->bars = build("bar_chart", chart, bp);
  e->strip = build("rate_strip", chart, sp);
  {
    static const char *const L[5] = {"00", "06", "12", "18", "24"};
    const float at[5] = {0, 0.25f, 0.5f, 0.75f, 1};
    fw::chart::axis(chart, t, (int) std::lround(chart_w_dp() * fw::D), L, at, 5);
  }
  fw::listen(chart, [](lv_obj_t *, const char *name, const char *detail, void *user) {  // a scrubbed bar: its reading
    if (strcmp(name, "scrub")) return;
    auto *root = (lv_obj_t *) user;
    EV *e = of(root);
    const int i = atoi(detail);
    const fw::energy::Data d = fw::energy::sample(e->fixed > 0 ? e->fixed : (double) time(nullptr) * 1000);
    if (i < 0 || i >= 48) { fw::set_text(e->lbl, fw::ty::label, "today so far"); return; }
    const fw::energy::Slot &x = d.slots[i];
    char b[64], k[24];
    if (std::isnan(x.kwh)) snprintf(k, sizeof k, "no data");
    else snprintf(k, sizeof k, "%.2f kWh", x.kwh);
    snprintf(b, sizeof b, "%s \xC2\xB7 %s \xC2\xB7 %s", hm(x.start).c_str(), k, fw::energy::pence(fw::energy::RATE[x.band]).c_str());
    fw::set_text(e->lbl, fw::ty::label, b);
  }, root);
  lv_obj_t *grow = box(root);
  lv_obj_set_flex_grow(grow, 1);
  more(root, root, "last 7 days", "energy_week");
  render(root);
  if (e->fixed <= 0) e->tick = lv_timer_create([](lv_timer_t *tm) { render((lv_obj_t *) lv_timer_get_user_data(tm)); }, fw::mo::ms::clock_tick * 60, root);
}

FW_UPDATE(energy_view) {
  EV *e = of(root);
  if (!e) return false;
  e->fixed = p["now"] | 0.0;
  render(root);
  return true;
}
