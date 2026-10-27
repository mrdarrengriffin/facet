// Theme engine workbench + test hooks (WASM only; not in the panel bundle, firmware/bundle.mjs lists its core files):
// fw_lab(-1, "theme_<cmd>", arg) from core/main.cpp. Every answer is JSON.
//   theme_load <json>      {ok, id, label, bytes, checks, digest, peak, modes, sets, errors: [{path, msg, line}]}
//   theme_apply <json>     theme_load + {live, gen}: the live slot (fx::theme_apply; a refusal changes nothing)
//   theme_live             {id, digest, gen}        theme_reset   back to Facet (new generation)
//   theme_use <id>         a built-in theme (themes/<id>.json) made live, as theme_apply; "" = Facet
//   (theme_apply / theme_use / theme_reset repaint every look live: fx::restyle, then the default display's stage bg)
//   theme_break <partial|> the break-it switch (fx::theme_break_partial)
//   theme_dump <json>      the compiled Skin: each part's merged block per combination of states (inks by name, lengths in px at
//                          [1.5x, 1x]), look sizes, type, fonts, shapes, the palette of accent set 0 per mode
//   theme_contrast <json>  {ok, rows: [[where, fg, bg, kind, mode, set, exact, panel, min, pass]], errors}
//   theme_builtins         [{id, ok, digest, errors}] every embedded theme     theme_facet  {digest, checks}
//   theme_info             {schema, max_bytes, looks: [[name, lo 1.5x, hi 1.5x, lo 1x, hi 1x]], skin_bytes}
#include <string>
#include <vector>
#include "fw.h"
#include "fx.h"
#include "theme_file.h"
#include "theme_gen.h"
#include "theme_slot.h"

namespace fx {
namespace {
namespace G = fx::tgen;
using namespace fx::skin;
void relook() {  // a live theme switch: the looks, then the stage's canvas (a host stage paints its own bg)
  fx::restyle();
  if (lv_display_t *d = lv_display_get_default())
    if (lv_obj_t *scr = lv_display_get_screen_active(d)) { lv_obj_set_style_bg_color(scr, fx::bg(d), 0); lv_obj_invalidate(scr); }
}
std::string hexs(uint64_t v, int w) { char b[24]; snprintf(b, sizeof b, "%0*llx", w, (unsigned long long) v); return b; }
std::string rgbs(uint32_t c) { char b[12]; snprintf(b, sizeof b, "#%06X", (unsigned) c); return b; }
std::string ink_name(const Ink &k) {
  if (k.kind == INK_TOKEN) return G::COLOURS[k.value];
  if (k.kind == INK_ROLE) return G::ROLES[k.value];
  if (k.kind == INK_HEX) return rgbs(k.value);
  return "";
}
void len(ArduinoJson::JsonArray a, const Len &l) { a.add(l.px[0]); a.add(l.px[1]); }
void face(ArduinoJson::JsonObject v, const Face &f) {
  static const char *const FILL[] = {"solid", "gradient", "checker", "blocks"};
  v["fill"] = FILL[f.fill];
  v["a"] = ink_name(f.a);
  if (f.fill != FILL_SOLID) { v["b"] = ink_name(f.b); v["down"] = (bool) f.down; }
  if (f.fill == FILL_BLOCKS) { len(v["run"].to<ArduinoJson::JsonArray>(), f.run); len(v["gap"].to<ArduinoJson::JsonArray>(), f.gap); }
}
void errors(ArduinoJson::JsonArray a, const std::vector<ThemeError> &err) {
  for (const auto &e : err) { auto o = a.add<ArduinoJson::JsonObject>(); o["path"] = e.path; o["msg"] = e.msg; o["line"] = e.line; }
}
void summary(ArduinoJson::JsonObject o, const std::shared_ptr<const Skin> &s, const std::vector<ThemeError> &err) {
  o["ok"] = (bool) s;
  errors(o["errors"].to<ArduinoJson::JsonArray>(), err);
  o["peak"] = theme_peak_bytes();
  if (!s) return;
  o["id"] = std::string(s->id); o["label"] = std::string(s->label); o["bytes"] = sizeof(Skin); o["checks"] = s->checks; o["digest"] = hexs(s->digest(), 16);
  auto m = o["modes"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < 2; i++) if (s->modes >> i & 1) m.add(G::MODES[i]);
  auto st = o["sets"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < s->n_sets; i++) { auto x = st.add<ArduinoJson::JsonObject>(); x["id"] = std::string(s->set_id[i]); x["label"] = std::string(s->set_label[i]); }
}
void dump(ArduinoJson::JsonObject o, const Skin &s) {
  static const char *const SIDE[] = {"", "bottom", "top", "", "left", "", "", "", "right"};
  auto parts = o["parts"].to<ArduinoJson::JsonObject>();
  for (int p = 0; p < N_PARTS; p++) {
    auto po = parts[G::PARTS[p].name].to<ArduinoJson::JsonObject>();
    for (int mask = 0; mask < N_COMBOS; mask++) {  // every combination of states with something set: "base", "on", "on+pressed", ...
      const Block b = s.merged(p, mask);
      if (!b.has) continue;
      std::string name;
      for (int i = 0; i < N_STATES; i++) if (mask >> i & 1) name += std::string(name.empty() ? "" : "+") + G::STATES[i];
      auto bo = po[mask ? name : std::string("base")].to<ArduinoJson::JsonObject>();
      auto keys = bo["keys"].to<ArduinoJson::JsonArray>();
      for (int k = 0; k < G::N_KEYS; k++) if (b.has >> k & 1) keys.add(G::KEYS[k]);
      if (b.has & K_FACE) face(bo["face"].to<ArduinoJson::JsonObject>(), b.face);
      if (b.has & K_RADIUS) len(bo["radius"].to<ArduinoJson::JsonArray>(), b.radius);
      if (b.has & K_PAD) len(bo["pad"].to<ArduinoJson::JsonArray>(), b.pad);
      if (b.has & K_TEXT) bo["text"] = ink_name(b.text);
      if (b.has & K_INK) bo["ink"] = ink_name(b.ink);
      if (b.has & K_WEIGHT) bo["weight"] = G::WEIGHTS[b.bold];
      if (b.has & K_SHAPE) bo["shape"] = !strcmp(G::PARTS[p].kind, "knob") ? G::KNOB_SHAPES[b.shape] : G::TICK_SHAPES[b.shape];
      if (b.has & K_SHADOW) { auto sh = bo["shadow"].to<ArduinoJson::JsonObject>(); len(sh["dx"].to<ArduinoJson::JsonArray>(), b.shadow.dx); len(sh["dy"].to<ArduinoJson::JsonArray>(), b.shadow.dy); sh["ink"] = ink_name(b.shadow.ink); }
      if (b.has & K_EDGES) {
        auto ea = bo["edges"].to<ArduinoJson::JsonArray>();
        for (int k = 0; k < b.n_edges; k++) {
          const Edge &e = s.edges[b.edge0 + k];
          auto eo = ea.add<ArduinoJson::JsonObject>();
          eo["side"] = e.side == SIDE_ALL ? "all" : e.side == SIDE_TL ? "top-left" : e.side == SIDE_BR ? "bottom-right" : e.side < 9 ? SIDE[e.side] : "?";
          len(eo["w"].to<ArduinoJson::JsonArray>(), e.w); len(eo["inset"].to<ArduinoJson::JsonArray>(), e.inset);
          eo["ink"] = ink_name(e.ink);
          eo["style"] = G::EDGE_STYLES[e.dotted];
        }
      }
    }
  }
  auto sz = o["size"].to<ArduinoJson::JsonObject>();
  for (int i = 0; i < N_SIZES; i++) if (G::SIZES[i].look) { auto a = sz[G::SIZES[i].name].to<ArduinoJson::JsonArray>(); a.add(s.size[0][i]); a.add(s.size[1][i]); }
  auto ty = o["type"].to<ArduinoJson::JsonObject>();
  for (int i = 0; i < N_TYPES; i++) { auto a = ty[G::TYPES[i].name].to<ArduinoJson::JsonArray>(); a.add(s.type[0][i]); a.add(s.type[1][i]); }
  auto fo = o["font"].to<ArduinoJson::JsonObject>();
  for (int i = 0; i < N_FONT_ROLES; i++) fo[G::FONT_ROLES[i]] = G::FAMILIES[s.font[i]];
  auto sh = o["shape"].to<ArduinoJson::JsonObject>();
  sh["corner"] = G::SHAPE_CORNER[s.corner]; sh["cap"] = G::SHAPE_CAP[s.cap]; sh["knob"] = G::SHAPE_KNOB[s.knob]; sh["ticks"] = G::SHAPE_TICKS[s.ticks];
  sh["card"] = G::SHAPE_CARD[s.card]; sh["indicator"] = G::SHAPE_INDICATOR[s.indicator]; sh["icons"] = G::SHAPE_ICONS[s.icons]; sh["snap"] = (bool) s.snap;
  if (s.buttons.n) {
    auto bt = o["buttons"].to<ArduinoJson::JsonObject>();
    auto nm = bt["names"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < s.buttons.n; i++) nm.add(G::GLYPHS[s.buttons.names[i]]);
    len(bt["w"].to<ArduinoJson::JsonArray>(), s.buttons.w); len(bt["h"].to<ArduinoJson::JsonArray>(), s.buttons.h); len(bt["gap"].to<ArduinoJson::JsonArray>(), s.buttons.gap);
  }
  auto sb = o["scrollbar"].to<ArduinoJson::JsonObject>();
  len(sb["w"].to<ArduinoJson::JsonArray>(), s.scrollbar.w); sb["arrows"] = (bool) s.scrollbar.arrows;
  if (s.scrollbar.has_lane) face(sb["lane"].to<ArduinoJson::JsonObject>(), s.scrollbar.lane);
  auto pal = o["palette"].to<ArduinoJson::JsonObject>();
  for (int m = 0; m < 2; m++) {
    if (!(s.modes >> m & 1)) continue;
    auto pm = pal[G::MODES[m]].to<ArduinoJson::JsonObject>();
    for (int c = 0; c < N_COLOURS; c++) pm[G::COLOURS[c]] = rgbs(s.palette[m][0][c]);
  }
}
}  // namespace

std::string theme_lab(const std::string &c, const std::string &a) {
  if (!c.compare(0, 11, "theme_slot_")) return theme_lab_slot(c, a);  // the sent-theme slot (core/theme_slot.cpp, T4)
  ArduinoJson::JsonDocument d;
  std::vector<ThemeError> err;
  if (c == "theme_load" || c == "theme_dump" || c == "theme_contrast") {
    auto s = theme_load(a.data(), a.size(), err);
    auto o = d.to<ArduinoJson::JsonObject>();
    summary(o, s, err);
    if (s && c == "theme_dump") dump(o, *s);
    if (s && c == "theme_contrast") {
      std::vector<ContrastRow> rows;
      theme_contrast(*s, rows);
      auto ra = o["rows"].to<ArduinoJson::JsonArray>();
      for (const auto &r : rows) {
        auto x = ra.add<ArduinoJson::JsonArray>();
        x.add(r.where); x.add(r.fg); x.add(r.bg); x.add(r.kind); x.add(G::MODES[r.light]); x.add(std::string(s->set_id[r.set]));
        x.add(r.exact); x.add(r.panel); x.add(r.min); x.add(r.pass);
      }
    }
  } else if (c == "theme_apply" || c == "theme_use") {
    if (c == "theme_apply") theme_apply(a.data(), a.size(), err);
    else {
      int f = -1;
      for (int i = 0; i < theme_builtins(); i++) if (a == theme_builtin_id(i)) f = i;
      if (a.empty() || a == "facet") theme_reset();
      else if (f < 0) err.push_back({"", "no built-in theme \"" + a + "\"", 0});
      else theme_apply(theme_builtin_json(f), strlen(theme_builtin_json(f)), err);
    }
    relook();
    auto o = d.to<ArduinoJson::JsonObject>();
    o["ok"] = err.empty();
    errors(o["errors"].to<ArduinoJson::JsonArray>(), err);
    o["live"] = hexs(theme_live()->digest(), 16); o["id"] = std::string(theme_live()->id); o["gen"] = theme_generation();
  } else if (c == "theme_live" || c == "theme_reset") {
    if (c == "theme_reset") { theme_reset(); relook(); }
    d["live"] = hexs(theme_live()->digest(), 16); d["id"] = std::string(theme_live()->id); d["gen"] = theme_generation();
  } else if (c == "theme_break") {
    theme_break_partial(a == "partial");
    fx::break_stale(a == "stale");
    d["partial"] = a == "partial"; d["stale"] = a == "stale";
  } else if (c == "theme_builtins") {
    auto arr = d.to<ArduinoJson::JsonArray>();
    for (int i = 0; i < theme_builtins(); i++) {
      const char *j = theme_builtin_json(i);
      auto s = theme_load(j, strlen(j), err);
      auto o = arr.add<ArduinoJson::JsonObject>();
      o["id"] = theme_builtin_id(i);
      summary(o, s, err);
    }
  } else if (c == "theme_facet") {
    d["digest"] = hexs(theme_facet()->digest(), 16); d["checks"] = theme_facet()->checks; d["id"] = std::string(theme_facet()->id);
  } else if (c == "theme_info") {
    d["schema"] = G::SCHEMA; d["max_bytes"] = G::MAX_BYTES; d["max_values"] = G::MAX_VALUES; d["skin_bytes"] = sizeof(Skin);
    auto l = d["looks"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < G::N_SIZES; i++) if (G::SIZES[i].look) { auto x = l.add<ArduinoJson::JsonArray>(); x.add(G::SIZES[i].name); x.add(G::SIZES[i].lo[0]); x.add(G::SIZES[i].hi[0]); x.add(G::SIZES[i].lo[1]); x.add(G::SIZES[i].hi[1]); }
  } else return "theme_load | theme_apply | theme_use | theme_live | theme_reset | theme_break | theme_dump | theme_contrast | theme_builtins | theme_facet | theme_info";
  std::string out;
  serializeJson(d, out);
  return out;
}
}  // namespace fx
