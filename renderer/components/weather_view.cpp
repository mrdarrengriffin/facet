// Weather view: the Weather screen's body (docs screens/weather.ts weatherScreen; the factory spec renderer/specs/
// weather.json adds the header): one screen for the weather, the sun, the moon and lightning. A Segmented under the
// header picks the view: Weather · Sun · Moon · Lightning.
//   Weather    the temperature (display, text) with the animated weather icon (ic::hero) on the right, the
//              condition (headline, text), one line (body, text-dim: "Feels 9° · high 16°, low 8°"), the rain line
//              (a Status dot info / idle + body: "Rain from 14:15" / "Dry for the next hour · rain at 16:00"), the
//              next eight hours (lay::hour_col columns: hour label text-dim, "now" accent-text; icon ic::glyph; body
//              temperature), and a "next 5 days" More row at the bottom (opens the Weather days screen)
//   Sun        the next sunrise or sunset as the hero (label kind, hero time, body "in 2 h 14 min"), the day's arc
//              (fw::value_arc lay::sun_arc, 180 degrees, track line, accent, a knob = the sun) over a horizon
//              hairline with the rise and set times under its ends, and four Stats (day length, vs yesterday, first
//              light, last light). The maths = core/sun.h (no API)
//   Moon       the Moon view (fw::find, size "weather") and a "details" More row (opens Moon details)
//   Lightning  the Lightning view (fw::find; its demo runs while the tab is open)
// Feed (core/weather.h): props condition / temp / feels (bindable: an HA weather.* state and its attributes), the
// hours and days from the sample adapter until a panel fetches forecasts; lat / lon for the sun and moon (the docs
// sample); now = a fixed time (ms, tests), 0 = live. Taps: the Segmented switches the view (instant, like the docs);
// a More row emits "open" "weather_days" / "moon_details" (the spec's nav.open). Not ported yet (noted for Darren):
// the Lightning tab's bolt + distance when a storm is near, the header's place name, landing on Lightning in a storm.
// Update path: the weather texts, icon and hours in place; view = a prop for the workbench (switches in place).
// Contrast: "weather" (+ stat, status_dot, more_row, radar, moon, text).
#include <strings.h>
#include <ctime>
// audit deps: lightning_view, moon_view, more_row, segmented, stat, status_dot
#include "fw.h"
#include "sun.h"
#include "weather.h"

namespace {
constexpr const char *VIEWS[4] = {"weather", "sun", "moon", "lightning"};
struct HourCol { lv_obj_t *hr, *ic, *t; };
struct WV {
  lv_obj_t *views[4] = {};
  lv_obj_t *temp = nullptr, *icon = nullptr, *cond = nullptr, *line = nullptr, *dot = nullptr, *rain = nullptr;
  HourCol hours[8] = {};
  lv_obj_t *s_kind = nullptr, *s_time = nullptr, *s_in = nullptr, *s_arc = nullptr, *s_rise = nullptr, *s_set = nullptr, *s_stats[4] = {};
  lv_obj_t *lx = nullptr;
  int view = 0;
  double fixed = 0, lat = 52.2, lon = -1.2;
  std::string condition, icon_name = "partly-day";
  double temp_v = NAN, feels_v = NAN;
  lv_timer_t *tick = nullptr;
};
WV *of(lv_obj_t *o) { return (WV *) lv_obj_get_user_data(o); }
double now_of(const WV *w) { return w->fixed > 0 ? w->fixed : (double) time(nullptr) * 1000; }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int w, int gap) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
std::string hm(double ms) {
  const time_t t = (time_t) (ms / 1000);
  struct tm tm;
  localtime_r(&t, &tm);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
bool is_day(const WV *w, double t) {
  const fw::sun::Times s = fw::sun::times(t, w->lat, w->lon);
  return !std::isnan(s.rise) && t >= s.rise && t < s.set;
}
// a More row inside a listened box: its tap emits "open" <screen> from the view's root (the spec's nav.open; the emit
// has to come from outside the listened box, the nearest scope takes it)
struct MoreLink { lv_obj_t *root; const char *screen; };
void more(lv_obj_t *root, lv_obj_t *parent, const char *text, const char *screen) {
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

// ---- Weather
void render_weather(lv_obj_t *root) {
  WV *w = of(root);
  const fw::Theme &t = *fw::T;
  const double now = now_of(w);
  fw::wx::Data d = fw::wx::sample(now);
  if (!w->condition.empty()) d.condition = w->condition.c_str();  // the bound HA state wins over the sample
  if (!std::isnan(w->temp_v)) d.temp = w->temp_v, d.feels = std::isnan(w->feels_v) ? d.temp : w->feels_v;
  fw::set_text(w->temp, fw::ty::display, fw::wx::deg(d.temp).c_str());
  w->icon_name = fw::wx::icon(d.condition, is_day(w, now));
  fw::weather_icon_set(w->icon, w->icon_name.c_str(), w->view == 0);
  fw::set_text(w->cond, fw::ty::headline, *fw::wx::text(d.condition) ? fw::wx::text(d.condition) : d.condition);
  char b[96];
  snprintf(b, sizeof b, "Feels %s \xC2\xB7 high %s, low %s", fw::wx::deg(d.feels).c_str(), fw::wx::deg(d.daily[0].hi).c_str(), fw::wx::deg(d.daily[0].lo).c_str());
  fw::set_text(w->line, fw::ty::body, b);
  if (!std::isnan(d.rain_from)) snprintf(b, sizeof b, "Rain from %s", hm(d.rain_from).c_str());
  else if (!std::isnan(d.next_rain)) snprintf(b, sizeof b, "Dry for the next hour \xC2\xB7 rain at %s", hm(d.next_rain).c_str());
  else snprintf(b, sizeof b, "Dry for the next hour");
  fw::set_text(w->rain, fw::ty::body, b);
  ArduinoJson::JsonDocument dp;
  dp["tone"] = std::isnan(d.rain_from) ? "idle" : "info";
  fw::update_child(fw::find("status_dot"), w->dot, dp.as<fw::Props>());
  for (int i = 0; i < 8; i++) {
    const fw::wx::Hour &h = d.hourly[i];
    fw::set_text(w->hours[i].hr, fw::ty::label, i == 0 ? "now" : hm(h.t).substr(0, 2).c_str());
    lv_obj_set_style_text_color(w->hours[i].hr, i == 0 ? t.accent_text : t.text_dim, 0);
    fw::weather_icon_set(w->hours[i].ic, fw::wx::icon(h.condition, is_day(w, h.t), h.precip), false);
    fw::set_text(w->hours[i].t, fw::ty::body, fw::wx::deg(h.temp).c_str());
  }
}
void build_weather(lv_obj_t *root, lv_obj_t *v) {
  WV *w = of(root);
  const fw::Theme &t = *fw::T;
  lv_obj_t *col = column(v, fw::lay::content_w, fw::sp::s3);
  lv_obj_t *row = box(col);
  lv_obj_set_size(row, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  w->temp = fw::text(row, fw::ty::display, "--\xC2\xB0", t.text);
  w->icon = fw::weather_icon(row, "partly-day", fw::ic::hero, true, t.text, t.bg);
  w->cond = fw::text(col, fw::ty::headline, "Loading", t.text);
  w->line = fw::text(col, fw::ty::body, "", t.text_dim, fw::lay::content_w);
  lv_obj_t *rr = box(col);
  lv_obj_set_size(rr, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(rr, fw::sp::s3, 0);
  ArduinoJson::JsonDocument dp;
  dp["tone"] = "idle";
  w->dot = build("status_dot", rr, dp);
  w->rain = fw::text(rr, fw::ty::body, "", t.text);
  lv_obj_t *strip = box(col);
  lv_obj_set_size(strip, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_margin_top(strip, fw::sp::s5, 0);
  for (HourCol &h : w->hours) {
    lv_obj_t *c = column(strip, fw::lay::hour_col, fw::sp::s2);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    h.hr = fw::text(c, fw::ty::label, "--", t.text_dim);
    h.ic = fw::weather_icon(c, "cloudy", fw::ic::glyph, false, t.text, t.bg);
    h.t = fw::text(c, fw::ty::body, "--", t.text);
  }
  lv_obj_t *grow = box(v);
  lv_obj_set_flex_grow(grow, 1);
  more(root, v, "next 5 days", "weather_days");
}

// ---- Sun
void render_sun(lv_obj_t *root) {
  WV *w = of(root);
  const double now = now_of(w);
  const fw::sun::Data d = fw::sun::data(now, w->lat, w->lon);
  const fw::sun::Now n = fw::sun::now_of(d, now);
  fw::set_text(w->s_kind, fw::ty::label, n.kind == 1 ? "sunrise" : n.kind == 2 ? "sunset" : "sun");
  fw::set_text(w->s_time, fw::ty::hero, n.kind ? hm(n.t).c_str() : "--:--");
  fw::set_text(w->s_in, fw::ty::body, n.kind ? fw::sun::in_time(n.t - now).c_str() : "the sun stays down today");
  lv_arc_set_value(w->s_arc, std::isnan(n.frac) ? 0 : (int32_t) std::lround(n.frac * 1000));
  // the knob = the sun: shown only while it is up
  lv_obj_set_style_opa(w->s_arc, std::isnan(n.frac) ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_KNOB);
  fw::set_text(w->s_rise, fw::ty::label, std::isnan(d.rise) ? "--:--" : hm(d.rise).c_str());
  fw::set_text(w->s_set, fw::ty::label, std::isnan(d.set) ? "--:--" : hm(d.set).c_str());
  const std::string v[4] = {std::isnan(d.length) ? "--" : fw::sun::day_length(d.length), std::isnan(d.delta) ? "--" : fw::sun::day_delta(d.delta),
                            std::isnan(d.dawn) ? "--:--" : hm(d.dawn), std::isnan(d.dusk) ? "--:--" : hm(d.dusk)};
  const char *L[4] = {"day length", "vs yesterday", "first light", "last light"};
  for (int i = 0; i < 4; i++) {
    ArduinoJson::JsonDocument p;
    p["label"] = L[i], p["value"] = v[i], p["tone"] = i == 1 ? "dim" : "text";
    fw::update_child(fw::find("stat"), w->s_stats[i], p.as<fw::Props>());
  }
}
void build_sun(lv_obj_t *root, lv_obj_t *v) {
  WV *w = of(root);
  const fw::Theme &t = *fw::T;
  const int A = fw::lay::sun_arc;
  lv_obj_t *row = box(v);
  lv_obj_set_size(row, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *left = column(row, fw::lay::content_w - A - fw::lay::gutter, fw::sp::s2);
  w->s_kind = fw::text(left, fw::ty::label, "sun", t.text_dim);
  w->s_time = fw::text(left, fw::ty::hero, "--:--", t.text);
  w->s_in = fw::text(left, fw::ty::body, "", t.text_dim);
  lv_obj_t *right = box(row);
  lv_obj_set_size(right, A, A / 2 + fw::sp::s2 + fw::style(fw::ty::label).box);
  fw::ArcLook l;
  l.start = 180.0f, l.range = 180.0f, l.steps = 1000;
  l.track_w = fw::stroke::arc, l.track_c = t.line;
  l.ind_w = fw::stroke::indicator, l.ind_c = t.accent;
  l.knob = fw::lay::sun_knob, l.knob_ring = fw::stroke::indicator, l.knob_c = t.accent, l.knob_ring_c = t.bg;
  w->s_arc = fw::value_arc(right, A, l);
  lv_obj_set_pos(w->s_arc, 0, 0);
  fw::bleed(w->s_arc, fw::lay::sun_knob / 2);  // the sun rides the arc's line: half of it outside the box
  lv_obj_t *hz = box(right);  // the horizon
  lv_obj_set_size(hz, A, fw::stroke::hair);
  lv_obj_set_pos(hz, 0, A / 2);
  lv_obj_set_style_bg_color(hz, t.line, 0);
  lv_obj_set_style_bg_opa(hz, LV_OPA_COVER, 0);
  w->s_rise = fw::text(right, fw::ty::label, "--:--", t.text_dim);
  lv_obj_set_pos(w->s_rise, 0, A / 2 + fw::sp::s2);
  w->s_set = fw::text(right, fw::ty::label, "--:--", t.text_dim);
  lv_obj_align(w->s_set, LV_ALIGN_TOP_RIGHT, 0, A / 2 + fw::sp::s2);
  lv_obj_t *grid = box(v);
  lv_obj_set_size(grid, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, fw::sp::s7, 0);
  lv_obj_set_style_pad_column(grid, fw::lay::gutter, 0);
  lv_obj_set_style_margin_top(grid, fw::sp::s9, 0);
  for (lv_obj_t *&s : w->s_stats) {
    ArduinoJson::JsonDocument p;
    p["label"] = "--", p["value"] = "--", p["tone"] = "text";
    s = build("stat", grid, p);
  }
}

void pick(lv_obj_t *root, int v) {
  WV *w = of(root);
  w->view = v;
  for (int i = 0; i < 4; i++) {
    if (i == v) lv_obj_remove_flag(w->views[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(w->views[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (v == 1) render_sun(root);
  // the big weather icon moves only while its view shows (a hidden animation is a panel's CPU for nothing)
  if (w->icon) fw::weather_icon_set(w->icon, w->icon_name.c_str(), v == 0);
}
void take(lv_obj_t *root, fw::Props p) {
  WV *w = of(root);
  w->fixed = p["now"] | 0.0;
  w->lat = p["lat"] | 52.2;
  w->lon = p["lon"] | -1.2;
  w->condition = p["condition"] | "";
  const char *tv = p["temp"] | "", *fv = p["feels"] | "";
  w->temp_v = *tv ? strtod(tv, nullptr) : NAN;
  w->feels_v = *fv ? strtod(fv, nullptr) : NAN;
  render_weather(root);
  render_sun(root);
}
}  // namespace

FW_COMPONENT(weather_view, "Weather view", "components", R"({
  "about": "The Weather screen's body: Weather, Sun, Moon and Lightning views under one Segmented. The weather with the next eight hours, the day's sun arc, the moon, the storm.",
  "frame": [480, 400],
  "props": [
    {"name": "view", "type": "select", "options": ["weather", "sun", "moon", "lightning"], "default": "weather"},
    {"name": "condition", "type": "text", "default": ""},
    {"name": "temp", "type": "text", "default": ""},
    {"name": "feels", "type": "text", "default": ""},
    {"name": "lat", "type": "range", "min": -60, "max": 65, "step": 0.1, "default": 52.2},
    {"name": "lon", "type": "range", "min": -180, "max": 180, "step": 0.1, "default": -1.2}
  ],
  "variants": [
    {"label": "weather", "props": {"now": 1792926000000}},
    {"label": "weather · bound: sunny 21", "props": {"now": 1792926000000, "condition": "sunny", "temp": "21.3", "feels": "20"}},
    {"label": "sun", "props": {"view": "sun", "now": 1792926000000}},
    {"label": "moon", "props": {"view": "moon", "now": 1792926000000}},
    {"label": "lightning", "props": {"view": "lightning", "now": 1792926000000}, "audit": {"skip": {"radial": "strikes on the radar's west edge sit over the 9 o'clock tick: data, not the ring's geometry"}}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  (void) t;
  auto *w = new WV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, w);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *w = (WV *) lv_event_get_user_data(e);
    if (w->tick) lv_timer_delete(w->tick);
    delete w;
  }, LV_EVENT_DELETE, w);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s7, 0);
  w->fixed = p["now"] | 0.0;
  lv_obj_t *tabs = box(root);  // the Segmented, listened to here (its change switches the view)
  lv_obj_set_size(tabs, fw::lay::content_w, LV_SIZE_CONTENT);
  ArduinoJson::JsonDocument sd;
  sd["options"] = "Weather, Sun, Moon, Lightning";
  sd["selected"] = p["view"] | "weather";
  sd["width"] = "fill";
  build("segmented", tabs, sd);
  fw::listen(tabs, [](lv_obj_t *, const char *name, const char *detail, void *user) {
    if (strcmp(name, "change")) return;
    for (int i = 0; i < 4; i++) if (!strcasecmp(detail, VIEWS[i])) pick((lv_obj_t *) user, i);
  }, root);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *v = w->views[i] = box(root);
    lv_obj_set_width(v, fw::lay::content_w);
    lv_obj_set_flex_grow(v, 1);
    lv_obj_set_flex_flow(v, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(v, LV_OBJ_FLAG_IGNORE_LAYOUT);
  }
  lv_obj_update_layout(root);
  const int top = lv_obj_get_height(tabs) + fw::sp::s7, vh = fw::lay::content_h - top;
  for (lv_obj_t *v : w->views) { lv_obj_set_pos(v, 0, top); lv_obj_set_height(v, vh); }
  build_weather(root, w->views[0]);
  build_sun(root, w->views[1]);
  {
    ArduinoJson::JsonDocument mp;
    mp["size"] = "weather";
    if (w->fixed > 0) mp["now"] = w->fixed;
    build("moon_view", w->views[2], mp);
    lv_obj_t *grow = box(w->views[2]);
    lv_obj_set_flex_grow(grow, 1);
    more(root, w->views[2], "details", "moon_details");
  }
  {
    ArduinoJson::JsonDocument lp;
    lp["size"] = "weather";
    if (w->fixed > 0) { lp["demo"] = false; lp["now"] = w->fixed; }
    w->lx = build("lightning_view", w->views[3], lp);
  }
  take(root, p);
  const char *v0 = p["view"] | "weather";
  int vi = 0;
  for (int i = 0; i < 4; i++) if (!strcmp(v0, VIEWS[i])) vi = i;
  pick(root, vi);
  if (w->fixed <= 0)
    w->tick = lv_timer_create([](lv_timer_t *tm) {
      auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
      if (of(root)->view == 1) render_sun(root);
    }, fw::mo::ms::clock_tick * 60, root);
}

FW_UPDATE(weather_view) {
  WV *w = of(root);
  if (!w) return false;
  take(root, p);
  const char *v0 = p["view"] | "weather";
  for (int i = 0; i < 4; i++) if (!strcmp(v0, VIEWS[i]) && i != w->view) pick(root, i);
  return true;
}
