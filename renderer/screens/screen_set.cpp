// Screen settings (Settings > Screens > a screen, 2026-10-23): one screen's picks on this panel. Composed only of
// real components, laid out as the Settings screen:
//   header   the Header (back + the screen's name + the screen chrome)
//   content  one scrolling column, groups as Settings (label, section-gap, rows; groups lay::group apart):
//            "shows"    what the screen shows and what it needs, in words (sources.json "shows" + "needs"; body
//                       text wrapped to the column), so a screen with no options yet still says what it is
//            "home"     List row "Show on Home" with its Toggle (the whole row): the deck entry's on
//            "source"   List row "Source" + the source's name (core/sources.h); more than one source = a chevron and
//                       a tap opens the Bottom sheet with the screen's sources (name; status = the kind, "needs <setup>"
//                       for a direct source that needs setting up). Built-in screens read "Built in". When the
//                       sources set the spec's "source" parameter (C51: the adapter, core/sources.h adapter()), this
//                       row is the one Source: a pick keeps "source" and "params.source", a spec with a "sample"
//                       option and no sample source gets "Sample (previews)", and the parameter is not repeated below.
//            "options"  one row per typed parameter of the screen's spec (core/spec.h "params", hidden ones left
//                       out): select = a List row + Segmented (up to 4 options) or the sheet; toggle = a List row
//                       toggle; entity = its friendly name, and a sheet of the screen's list of the same domain when
//                       it has one (Media: the Player from the Speakers, Auto first when the spec's screen.param
//                       follows a sensor: "auto" or nothing picked = Auto); entities = "N chosen"; text = the value.
//                       Lists of entities and text come from the host's discovery (the hub writes them into the deck).
//                       Entity pickers (2026-10-31): an entity (no screen list of its domain) or entities parameter
//                       with a "domain" opens the sheet on the panel's own Home Assistant list (core/device.h
//                       ha_link): on open it reads ONLY that picker's domain(s) (comma list) when not cached yet
//                       (cached per domain until Read now or a new address), a "Reading Home Assistant" row meanwhile,
//                       "not reached" + Try again on a failure, and "Home Assistant not connected" (a tap: Settings >
//                       Home Assistant, navigate "ha_link") when the link is not set up. Search without a keyboard:
//                       sorted by name; more than PAGE matches = rows by the next letter of the name (a tap narrows,
//                       one-letter levels skipped, "All entities" goes back up), else the entities (name + id).
//                       entity = choose one (+ Auto when the spec has it), the sheet closes; entities = choose many
//                       ("chosen" marks them, the sheet stays open, the row says "N chosen"), kept as a comma list.
//                       No options = one List row "No options yet".
//   sheet    the Bottom sheet in overlay mode, the screen's last child (one, moved in place per row)
// Every change is kept in this panel's deck (fw::spec::deck_set: "on", "source", "params.<name>"; stored like a sent
// deck) and emitted as "deck" "<screen> <key>=<value>" for the host. The screen opens with them next time.
// Props: id = the screen (a deck screen id). Update path: none needed (built each time it opens).
#include "fw.h"
#include "spec.h"
#include "sources.h"
#include "device.h"
#include "sheet.h"
#include <algorithm>

namespace {
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
void spacer(lv_obj_t *parent, int h) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, 1, h);
}
void group(lv_obj_t *c, const char *label, bool first) {
  if (!first) spacer(c, fw::lay::group);
  lv_obj_t *gl = fw::text(c, fw::ty::label, label, fw::T->text_dim);
  lv_obj_remove_flag(gl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_height(gl, fw::style(fw::ty::label).box);
  spacer(c, fw::lay::section_gap);
}
std::string friendly(const std::string &e) {
  if (e.empty()) return "";
  fw::ha::want(e, "friendly_name");
  const std::string *n = fw::ha::get(e, "friendly_name");
  return n && !n->empty() ? *n : e;
}
std::vector<std::string> split(const std::string &l) {
  std::vector<std::string> v;
  for (size_t i = 0; i < l.size();) {
    size_t j = l.find(',', i);
    if (j == std::string::npos) j = l.size();
    std::string x = l.substr(i, j - i);
    while (!x.empty() && x[0] == ' ') x.erase(0, 1);
    if (!x.empty()) v.push_back(x);
    i = j + 1;
  }
  return v;
}
std::string label_of(const std::string &opt) {  // a select option shown: "cd" -> "CD", "vinyl" -> "Vinyl"
  std::string s = opt;
  if (s.size() <= 2) { for (char &c : s) c = (char) toupper((unsigned char) c); return s; }
  if (!s.empty()) s[0] = (char) toupper((unsigned char) s[0]);
  return s;
}

// what a row picks from the sheet: the source, a select's options, an entity from a list
// ha: the list is the panel's own HA list of `dom` (ids = what a row does: "e:<entity>", "g:<prefix>", "auto",
// "up", "set", "retry", "" = nothing), rebuilt by fill_ha
struct Pick {
  std::string key; std::vector<std::string> ids, names, status, sets; lv_obj_t *row = nullptr; std::string title; int sel = 0;
  bool ha = false, multi = false, auto_ok = false, tried = false;
  std::string dom, prefix, cur;     // cur: the single pick's value ("auto" = Auto)
  std::vector<std::string> chosen;  // multi: the picks
};
struct Page {
  lv_obj_t *root = nullptr, *sheet = nullptr, *home = nullptr;
  lv_timer_t *tick = nullptr;
  uint32_t seen = 0;
  bool dirty = false;
  std::string id;
  std::vector<Pick> picks;
  int open = -1;                                           // the pick the sheet shows
  std::vector<std::pair<lv_obj_t *, std::string>> ctrls;  // Segmented / Toggle -> its key
};
void keep(Page *pg, lv_obj_t *from, const std::string &key, ArduinoJson::JsonVariantConst v) {
  fw::spec::deck_set(pg->id, key, v);
  std::string vs;
  if (v.is<const char *>()) vs = v.as<const char *>();
  else serializeJson(v, vs);
  fw::emit(from, "deck", (pg->id + " " + key + "=" + vs).c_str());
}
namespace hl = fw::dev::ha_link;
constexpr size_t PAGE = 24;  // at most this many entity rows in the sheet; more = rows by the next letter
std::string upper(std::string s) { for (char &c : s) c = (char) toupper((unsigned char) c); return s; }
// the sheet's rows for an HA pick, from the link as it is now; starts the read of the domains not cached yet
void fill_ha(Pick &p) {
  p.ids.clear(), p.names.clear(), p.status.clear();
  p.sel = -1;
  auto row = [&](const std::string &a, const std::string &n, const std::string &st) { p.ids.push_back(a), p.names.push_back(n), p.status.push_back(st); };
  if (!hl::ready()) { row("set", "Home Assistant not connected", "set it up in Settings > Home Assistant"); return; }
  std::string need;
  std::vector<const hl::Entity *> all;
  for (const std::string &d : split(p.dom)) {
    if (const std::vector<hl::Entity> *l = hl::list(d)) for (const hl::Entity &e : *l) all.push_back(&e);
    else need += (need.empty() ? "" : ",") + d;
  }
  if (!need.empty()) {
    if (hl::status() == hl::FAIL && p.tried) { row("retry", "Home Assistant not reached", hl::why()); row("retry", "Try again", ""); return; }
    if (hl::status() != hl::LOADING && !*hl::fetch(need)) p.tried = true;
    row("", "Reading Home Assistant", "loading " + need);
    return;
  }
  auto name_of = [](const hl::Entity *e) { return e->name.empty() ? e->id : e->name; };
  std::sort(all.begin(), all.end(), [&](const hl::Entity *a, const hl::Entity *b) { return upper(name_of(a)) < upper(name_of(b)); });
  std::vector<const hl::Entity *> m;
  for (;;) {  // the matches of the prefix; one letter level only = narrow by itself
    m.clear();
    for (const hl::Entity *e : all) if (upper(name_of(e)).rfind(p.prefix, 0) == 0) m.push_back(e);
    if (m.size() <= PAGE) break;
    std::string next;
    bool one = true, ended = false;
    for (const hl::Entity *e : m) {
      const std::string u = upper(name_of(e));
      if (u.size() <= p.prefix.size()) { ended = true; continue; }
      if (next.empty()) next = u.substr(0, p.prefix.size() + 1);
      else if (u.compare(0, p.prefix.size() + 1, next)) one = false;
    }
    if (!one || ended || next.empty()) break;
    p.prefix = next;
  }
  if (!p.prefix.empty()) row("up", "All entities", std::to_string(all.size()) + " in all");
  else if (p.auto_ok) row("auto", "Auto", "follows what's playing");
  if (p.prefix.empty() && p.auto_ok && p.cur == "auto") p.sel = 0;
  if (all.empty()) { row("", "No entities", "none in " + p.dom); return; }
  if (m.size() > PAGE) {  // rows by the next letter
    for (size_t i = 0; i < m.size();) {
      auto key = [&](const hl::Entity *e) { const std::string u = upper(name_of(e)); return u.substr(0, p.prefix.size() + 1); };
      const std::string g = key(m[i]);
      size_t j = i;
      while (j < m.size() && key(m[j]) == g) j++;
      if (g.size() > p.prefix.size() && j - i > 1) row("g:" + g, g + "\xE2\x80\xA6", std::to_string(j - i) + (j - i == 1 ? " entity" : " entities"));
      else for (size_t k = i; k < j; k++) {  // a letter with one entity, or names that end here: the entities
        row("e:" + m[k]->id, name_of(m[k]), p.multi && std::find(p.chosen.begin(), p.chosen.end(), m[k]->id) != p.chosen.end() ? "chosen \xC2\xB7 " + m[k]->id : m[k]->id);
        if (!p.multi && m[k]->id == p.cur) p.sel = (int) p.ids.size() - 1;
      }
      i = j;
    }
  } else
    for (const hl::Entity *e : m) {
      const bool on = std::find(p.chosen.begin(), p.chosen.end(), e->id) != p.chosen.end();
      row("e:" + e->id, name_of(e), p.multi && on ? "chosen \xC2\xB7 " + e->id : e->id);
      if (!p.multi && e->id == p.cur) p.sel = (int) p.ids.size() - 1;
    }
}
void sheet_props(Page *pg, int k, bool open, Doc &d) {
  d["overlay"] = true;
  d["open"] = open ? "open" : "closed";
  if (k < 0 || k >= (int) pg->picks.size()) { d["title"] = ""; d["items"].to<ArduinoJson::JsonArray>(); d["selected"] = 0; return; }
  const Pick &p = pg->picks[k];
  d["title"] = p.prefix.empty() ? p.title.c_str() : (p.title + " \xC2\xB7 " + p.prefix).c_str();
  if (p.ha) d["stay"] = true;  // the sheet closes itself after a real pick (a letter row narrows)
  ArduinoJson::JsonArray items = d["items"].to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < p.ids.size(); i++) {
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    // HA picks: a letter row = chevron, a chosen entity (entities) = check, the rest none; other lists: check
    const char *ic = !p.ha ? "check" : p.ids[i].rfind("g:", 0) == 0 ? "chevron" : p.status[i].rfind("chosen", 0) == 0 ? "check" : "none";
    it["icon"] = ic, it["name"] = p.names[i].c_str(), it["status"] = p.status[i].c_str();
  }
  d["selected"] = p.sel;
}

const char *META = R"({
  "role": "detail",
  "about": "One screen's settings on this panel: what it shows, on Home or not, where its data comes from and its own options (Media: the visual, the player). Opened by tapping a screen in Settings > Screens, or nav.open screen_set with params {id}.",
  "frame": [480, 480],
  "icons": ["back@glyph", "chevron@icon", "check@icon", "close@glyph"],
  "props": [
    {"name": "id", "about": "Which screen's settings to show (a screen id of the deck or the factory).", "type": "select", "options": ["agents", "bins", "calendar", "cameras", "clock", "covers", "energy", "flightradar", "github", "living_room", "media", "moon", "overhead", "plants", "print", "strike_map", "thermostat", "timer", "vacuum", "weather"], "default": "media"}
  ]
})";

void build_page(lv_obj_t *parent, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *pg = new Page;
  pg->id = p["id"] | "media";
  pg->root = box(parent);
  lv_obj_set_size(pg->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(pg->root, t.bg, 0);
  lv_obj_set_style_bg_opa(pg->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(pg->root, [](lv_event_t *e) {
    auto *pg = (Page *) lv_event_get_user_data(e);
    if (pg->tick) lv_timer_delete(pg->tick);
    delete pg;
  }, LV_EVENT_DELETE, pg);

  Doc entry, spec, cat;
  fw::spec::deck_entry(pg->id, entry);
  if (const char *j = fw::spec::screen_json(pg->id)) deserializeJson(spec, j);
  fw::src::load(cat);
  std::string name = spec["name"] | "";
  if (name.empty()) { const fw::Component *c = fw::find(pg->id.c_str()); name = c ? c->name : pg->id; }

  Doc hd;
  hd["label"] = name.c_str();
  if (lv_obj_t *h = build("header", pg->root, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);
  lv_obj_t *c = box(pg->root);
  lv_obj_add_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // clickable: a drag in a gap scrolls too
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);

  // shows: what the screen is, in words
  {
    ArduinoJson::JsonVariantConst sc = cat["screens"][pg->id];
    std::string about = sc["shows"] | "";
    const std::string needs = sc["needs"] | "";
    if (!needs.empty()) about += std::string(about.empty() ? "Needs " : " Needs ") + needs + ".";
    else if (!about.empty()) about += " Works in every home.";
    if (!about.empty()) {
      group(c, "shows", true);
      lv_obj_t *l = fw::text(c, fw::ty::body, about.c_str(), t.text, fw::lay::content_w);
      lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    }
  }
  // home
  group(c, "home", !cat["screens"][pg->id]["shows"].is<const char *>() && !cat["screens"][pg->id]["needs"].is<const char *>());
  Doc hr;
  hr["title"] = "Show on Home", hr["icon"] = "none", hr["trailing"] = "toggle", hr["on"] = fw::spec::deck_on(entry);
  if (lv_obj_t *r = build("list_row", c, hr)) pg->ctrls.push_back({r, "on"});

  // source
  group(c, "source", false);
  ArduinoJson::JsonArrayConst srcs = fw::src::of(cat, pg->id);
  ArduinoJson::JsonObjectConst params = spec["params"].as<ArduinoJson::JsonObjectConst>();
  ArduinoJson::JsonVariantConst sparam = params["source"];
  const char *sv0 = entry["params"]["source"] | (const char *) nullptr;
  const std::string sv = sv0 ? std::string(sv0) : fw::spec::param_default(sparam);
  const std::string cur = fw::src::shown(cat, pg->id, entry, sparam, sv);
  Pick pk;
  pk.key = "source", pk.title = "source";
  bool linked = false, has_sample = false;
  for (ArduinoJson::JsonVariantConst x : srcs) {
    pk.ids.push_back(x["id"] | "");
    pk.names.push_back(x["name"] | "");
    pk.sets.push_back(fw::src::adapter(x, sparam));
    linked |= !pk.sets.back().empty();
    has_sample |= pk.sets.back() == "sample";
    std::string st = cat["kinds"][x["kind"] | ""] | "";
    ArduinoJson::JsonArrayConst setup = x["setup"].as<ArduinoJson::JsonArrayConst>();
    if (setup.size()) {
      std::string need;
      for (ArduinoJson::JsonVariantConst s : setup) need += std::string(need.empty() ? "" : " + ") + (s | "");
      st += " \xC2\xB7 needs " + need;
    }
    pk.status.push_back(st);
  }
  bool sample_opt = false;
  for (ArduinoJson::JsonVariantConst o : sparam["options"].as<ArduinoJson::JsonArrayConst>()) sample_opt |= !strcmp(o | "", "sample");
  if (linked && sample_opt && !has_sample) {  // previews: the spec's sample adapter, not a catalogue source
    pk.ids.push_back(fw::src::SAMPLE), pk.names.push_back(fw::src::SAMPLE_NAME), pk.sets.push_back("sample");
    pk.status.push_back(cat["kinds"]["builtin"] | "");
  }
  for (size_t i = 0; i < pk.ids.size(); i++) if (pk.ids[i] == cur) pk.sel = (int) i;
  Doc sr;
  sr["title"] = "Source", sr["icon"] = "none";
  sr["value"] = pk.ids.empty() ? "Built in" : pk.names[pk.sel].c_str();
  sr["trailing"] = pk.ids.size() > 1 ? "value-chevron" : "value";
  if (lv_obj_t *r = build("list_row", c, sr); r && pk.ids.size() > 1) {
    pk.row = r;
    pg->picks.push_back(pk);
  }

  // options: the spec's typed parameters
  bool any = false;
  auto value = [&](const char *k, ArduinoJson::JsonVariantConst def) {
    const char *v = entry["params"][k] | (const char *) nullptr;
    return v ? std::string(v) : fw::spec::param_default(def);
  };
  for (ArduinoJson::JsonPairConst kv : params) {
    if (!kv.value().is<ArduinoJson::JsonObjectConst>()) continue;
    const std::string type = kv.value()["type"] | "", key = kv.key().c_str();
    if (type == "hidden" || type.empty()) continue;
    if (linked && key == "source") continue;  // the Source row above sets it
    if (!any) { group(c, "options", false); any = true; }
    const std::string v = value(kv.key().c_str(), kv.value()), label = kv.value()["label"] | kv.key().c_str();
    Doc rd;
    rd["title"] = label.c_str(), rd["icon"] = "none";
    if (type == "select") {
      ArduinoJson::JsonArrayConst opts = kv.value()["options"].as<ArduinoJson::JsonArrayConst>();
      if (opts.size() <= 4) {  // a List row (title only, not a tap target) + the Segmented as its trailing part
        rd["trailing"] = "none";
        lv_obj_t *r = build("list_row", c, rd);
        if (!r) continue;
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
        std::string labels;
        for (ArduinoJson::JsonVariantConst o : opts) labels += std::string(labels.empty() ? "" : ", ") + label_of(o | "");
        Doc sd;
        sd["options"] = labels.c_str(), sd["selected"] = v.c_str();
        if (lv_obj_t *sg = build("segmented", r, sd)) pg->ctrls.push_back({sg, "params." + key});
        continue;
      }
      rd["trailing"] = "value-chevron", rd["value"] = label_of(v).c_str();
      lv_obj_t *r = build("list_row", c, rd);
      if (!r) continue;
      Pick pk;
      pk.key = "params." + key, pk.row = r, pk.title = label;
      for (ArduinoJson::JsonVariantConst o : opts) { pk.ids.push_back(o | ""); pk.names.push_back(label_of(o | "")); pk.status.push_back(""); }
      for (size_t i = 0; i < pk.ids.size(); i++) if (pk.ids[i] == v) pk.sel = (int) i;
      pg->picks.push_back(pk);
      continue;
    }
    if (type == "toggle") {
      rd["trailing"] = "toggle", rd["on"] = v == "true";
      if (lv_obj_t *r = build("list_row", c, rd)) pg->ctrls.push_back({r, "params." + key});
      continue;
    }
    if (type == "entity") {  // a sheet when the screen lists entities of the same domain (Media: Speakers)
      const std::string dom = kv.value()["domain"] | "";
      std::vector<std::string> from;
      for (ArduinoJson::JsonPairConst o : params)
        if (!strcmp(o.value()["type"] | "", "entities") && dom == (o.value()["domain"] | "")) from = split(value(o.key().c_str(), o.value()));
      // Auto (the spec's screen.param "auto" for this parameter, its sensor set): the sheet's first row; the value
      // "auto" or nothing picked = Auto (the screen follows HA's target); a real pick sticks until Auto is picked
      // (or, with no sensor set, Auto follows what's playing among the list: offered whenever the list has entities)
      std::string auto_src;
      bool has_auto = false;
      for (ArduinoJson::JsonVariantConst nd : spec["nodes"].as<ArduinoJson::JsonArrayConst>())
        for (ArduinoJson::JsonPairConst o : nd["on"].as<ArduinoJson::JsonObjectConst>())
          if (!strcmp(o.value()["do"] | "", "screen.param") && key == (o.value()["param"] | "") && o.value()["auto"].is<const char *>()) {
            const char *ak = o.value()["auto"];
            auto_src = value(ak, params[ak]);
            has_auto = true;
          }
      if (!fw::spec::entity_ok(auto_src) || auto_src[0] == '$') auto_src.clear();
      const bool offer_auto = !auto_src.empty() || (has_auto && !from.empty());
      const bool is_auto = offer_auto && (v == "auto" || !fw::spec::deck_picked(pg->id, key));
      if (from.empty() && !dom.empty()) {  // the panel's own HA list of the domain (fill_ha)
        rd["value"] = is_auto ? "Auto" : v.empty() || v == "auto" ? "None" : friendly(v).c_str();
        rd["trailing"] = "value-chevron";
        lv_obj_t *r = build("list_row", c, rd);
        if (!r) continue;
        Pick pk;
        pk.key = "params." + key, pk.row = r, pk.title = label, pk.ha = true, pk.dom = dom, pk.auto_ok = offer_auto;
        pk.cur = is_auto ? "auto" : v;
        pg->picks.push_back(pk);
        continue;
      }
      const size_t n = from.size() + (offer_auto ? 1 : 0);
      rd["value"] = is_auto ? "Auto" : v.empty() || v == "auto" ? "" : friendly(v).c_str();
      rd["trailing"] = n > 1 ? "value-chevron" : "value";
      lv_obj_t *r = build("list_row", c, rd);
      if (!r || n < 2) continue;
      Pick pk;
      pk.key = "params." + key, pk.row = r, pk.title = label;
      if (offer_auto) { pk.ids.push_back("auto"); pk.names.push_back("Auto"); pk.status.push_back("follows what's playing"); }
      for (const std::string &e : from) {
        pk.ids.push_back(e);
        pk.names.push_back(friendly(e));
        fw::ha::want(e);
        const std::string *st = fw::ha::get(e);
        pk.status.push_back(st ? *st : "");
      }
      for (size_t i = 0; i < pk.ids.size(); i++) if (pk.ids[i] == (is_auto ? "auto" : v)) pk.sel = (int) i;
      pg->picks.push_back(pk);
      continue;
    }
    if (type == "entities" && kv.value()["domain"].is<const char *>()) {  // choose many from the HA list
      const std::vector<std::string> ch = split(v);
      rd["value"] = ch.empty() ? "None" : (std::to_string(ch.size()) + " chosen").c_str();
      rd["trailing"] = "value-chevron";
      lv_obj_t *r = build("list_row", c, rd);
      if (!r) continue;
      Pick pk;
      pk.key = "params." + key, pk.row = r, pk.title = label, pk.ha = true, pk.multi = true, pk.dom = kv.value()["domain"] | "";
      pk.chosen = ch;
      pg->picks.push_back(pk);
      continue;
    }
    rd["trailing"] = "value";
    rd["value"] = type == "entities" ? (split(v).empty() ? "" : std::to_string(split(v).size()) + " chosen") : v;
    build("list_row", c, rd);
  }

  if (!any) {  // still a page: say so
    group(c, "options", false);
    Doc nr;
    nr["title"] = "No options yet", nr["icon"] = "none", nr["trailing"] = "none";
    if (lv_obj_t *r = build("list_row", c, nr)) lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  }

  // the one sheet (over the whole screen)
  Doc sd;
  sheet_props(pg, -1, false, sd);
  pg->sheet = build("bottom_sheet", pg->root, sd);
  for (size_t k = 0; k < pg->picks.size(); k++) {
    lv_obj_set_user_data(pg->picks[k].row, (void *) (intptr_t) k);
    lv_obj_add_event_cb(pg->picks[k].row, [](lv_event_t *e) {  // after the List row's own tap: the sheet with its list
      auto *pg = (Page *) lv_event_get_user_data(e);
      auto *row = (lv_obj_t *) lv_event_get_current_target(e);
      if (!pg->sheet || lv_obj_has_state(row, fw::st::OFF)) return;
      pg->open = (int) (intptr_t) lv_obj_get_user_data(row);
      Pick &p = pg->picks[pg->open];
      if (p.ha) p.prefix.clear(), p.tried = false, fill_ha(p), pg->seen = hl::rev();
      Doc d;
      sheet_props(pg, pg->open, true, d);
      fw::update_child(fw::find("bottom_sheet"), pg->sheet, d.as<fw::Props>());
    }, LV_EVENT_CLICKED, pg);
  }
  if (pg->sheet)
    lv_obj_add_event_cb(pg->sheet, [](lv_event_t *e) {  // a pick: the row shows it, the deck keeps it
      auto *pg = (Page *) lv_event_get_user_data(e);
      const int i = (int) (intptr_t) lv_event_get_param(e);
      if (pg->open < 0 || pg->open >= (int) pg->picks.size()) return;
      Pick &p = pg->picks[pg->open];
      if (i < 0 || i >= (int) p.ids.size()) return;
      if (p.ha) {  // what the row does (fill_ha); the sheet's rows again after this tap (pg->dirty, the tick)
        const std::string a = p.ids[i];
        pg->dirty = true;
        if (a == "set") { fw::sheet::close(pg->sheet); fw::emit(pg->sheet, "navigate", "ha_link"); pg->dirty = false; return; }
        if (a == "retry") { p.tried = false; return; }
        if (a == "up") { p.prefix.clear(); return; }
        if (a.rfind("g:", 0) == 0) { p.prefix = a.substr(2); return; }
        if (a.empty()) return;
        Doc v;
        if (p.multi) {
          const std::string e = a.substr(2);
          auto it = std::find(p.chosen.begin(), p.chosen.end(), e);
          if (it == p.chosen.end()) p.chosen.push_back(e); else p.chosen.erase(it);
          std::string l;
          for (const std::string &x : p.chosen) l += (l.empty() ? "" : ",") + x;
          v.set(l.c_str());
          Doc rp;
          rp["value"] = p.chosen.empty() ? "None" : (std::to_string(p.chosen.size()) + " chosen").c_str();
          fw::update_child(fw::find("list_row"), p.row, rp.as<fw::Props>());
          keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
          return;
        }
        p.cur = a == "auto" ? "auto" : a.substr(2);
        v.set(p.cur.c_str());
        Doc rp;
        rp["value"] = p.names[i].c_str();
        fw::update_child(fw::find("list_row"), p.row, rp.as<fw::Props>());
        keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
        fw::sheet::close(pg->sheet);
        pg->dirty = false;
        return;
      }
      p.sel = i;
      Doc rp;
      rp["value"] = p.names[i].c_str();
      fw::update_child(fw::find("list_row"), p.row, rp.as<fw::Props>());
      if (p.ids[i] != fw::src::SAMPLE) {
        Doc v;
        v.set(p.ids[i].c_str());
        keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
      }
      if (p.key == "source" && i < (int) p.sets.size() && !p.sets[i].empty()) {  // C51: the adapter it fills the feed with
        Doc v;
        v.set(p.sets[i].c_str());
        keep(pg, pg->sheet, "params.source", v.as<ArduinoJson::JsonVariantConst>());
      }
    }, LV_EVENT_VALUE_CHANGED, pg);
  // an open HA pick follows the link (a read ends) and a tap that changes its rows (rebuilt here, after the tap)
  pg->tick = lv_timer_create([](lv_timer_t *tm) {
    auto *pg = (Page *) lv_timer_get_user_data(tm);
    if (pg->open < 0 || pg->open >= (int) pg->picks.size() || !pg->picks[pg->open].ha || !pg->sheet) return;
    if (!pg->dirty && pg->seen == hl::rev()) return;
    if (!pg->dirty && !fw::sheet::is_open(pg->sheet)) return;
    pg->dirty = false;
    Pick &p = pg->picks[pg->open];
    fill_ha(p);
    pg->seen = hl::rev();
    Doc d;
    sheet_props(pg, pg->open, true, d);
    fw::update_child(fw::find("bottom_sheet"), pg->sheet, d.as<fw::Props>());
  }, 50, pg);
  fw::listen(pg->root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *pg = (Page *) user;
    const bool flip = !strcmp(name, fw::st::SWITCH_EVENT), change = !strcmp(name, "change");
    if (flip || change)
      for (auto &cx : pg->ctrls) {
        lv_obj_t *o = cx.first;
        bool in = false;
        for (lv_obj_t *a = from; a; a = lv_obj_get_parent(a)) if (a == o) { in = true; break; }
        if (!in) continue;
        Doc v;
        if (flip && cx.second == "on") v.set(!strcmp(detail, "on"));
        else if (flip) v.set(!strcmp(detail, "on") ? "true" : "false");
        else v.set(detail);  // Segmented: the option id (labels lower-cased = the ids)
        keep(pg, from, cx.second, v.as<ArduinoJson::JsonVariantConst>());
        return;
      }
    if (lv_obj_t *up = lv_obj_get_parent(pg->root)) fw::notify(up, name, detail);
  }, pg);
}
const int reg = fw::add({"screen_set", "Screen settings", "screens", META, build_page});
}  // namespace

FW_UPDATE(screen_set) { return false; }
