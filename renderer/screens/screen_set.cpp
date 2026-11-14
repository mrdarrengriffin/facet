// Screen settings (Settings > Screens > a screen): one screen's picks on this panel. On the contract: Screen > Page
// (fx::page) > Header + list Body (C1-C3), real components only, laid out as the Settings screen:
//   header   the Header (back + the screen's name + the screen chrome)
//   body     groups as Settings: a Section header (size label) over its rows, groups a group apart (above:group):
//            "shows"    what the screen shows and what it needs, in words (sources.json "shows" + "needs"; the Text
//                       component, body, up to 4 lines), so a screen with no options yet still says what it is
//            "source"   List row "Source" + the source's name (core/sources.h); more than one source = a chevron and
//                       a tap opens the Bottom sheet with the screen's sources (name; status = the kind, "needs <setup>"
//                       for a direct source that needs setting up; a Home Assistant integration that is not set up
//                       is listed greyed, not pickable, "Not set up in Home Assistant"). Built-in screens read "Built in". When the
//                       sources set the spec's "source" parameter (C51: the adapter, core/sources.h adapter()), this
//                       row is the one Source: a pick keeps "source" and "params.source", a spec with a "sample"
//                       option and no sample source gets "Sample (previews)", and the parameter is not repeated below.
//                       A source's sheet line is its sources.json "line" when it has one; a source that "finds" its own
//                       entity in HA's registry gets one sheet row per candidate only when there is more than one, the
//                       row setting that parameter too. A source with "value" (sources.json: a feed attribute) adds
//                       that word of its feed after its name in the row's value ("adsb.lol · centred on Home").
//                       A source with "from" (sources.json: a standard place in HA it reads, Energy: HA's Energy
//                       dashboard) adds the row "From <it>" with what the feed found there (its "found" attribute:
//                       "Grid, Solar · 3 devices", "Not set up in Home Assistant"), following the pick live.
//            "options"  one row per typed parameter of the screen's spec (core/spec.h "params", hidden ones left
//                       out): select = a List row + Segmented in its end slot (up to 4 options, no labels) or a value row and the
//                       sheet (more options, or the spec's "labels" / "labels_mi": words with a unit); toggle = a List row
//                       toggle; entity = its friendly name, and a sheet of the screen's list of the same domain when
//                       it has one (Media: the Player from the Speakers, Auto first when the spec's screen.param
//                       follows a sensor: "auto" or nothing picked = Auto); entities = "N chosen"; text = the value.
//                       Lists of entities and text come from the host's discovery (the workbench writes them into the deck).
//                       Entity pickers: an entity (no screen list of its domain) or entities parameter
//                       with a "domain" opens the sheet on the panel's own Home Assistant list (core/device.h
//                       ha_link): on open it reads ONLY that picker's domain(s) (comma list) when not cached yet
//                       (cached per domain until a new address; a list older than ha_link::STALE_MS is read again in
//                       the background on open, the old list showing meanwhile), a "Reading Home Assistant" row meanwhile,
//                       "not reached" + Try again on a failure, and "Home Assistant not connected" (a tap: Settings >
//                       Home Assistant, navigate "ha_link") when the link is not set up. Search without a keyboard:
//                       sorted by name; more than PAGE matches = rows by the next letter of the name (a tap narrows,
//                       one-letter levels skipped, "All entities" goes back up), else the entities (name + id).
//                       entity = choose one (+ Auto when the spec has it), the sheet closes; entities = choose many
//                       (a checkbox marks them, the sheet stays open, the row says "N chosen"), kept as a comma list.
//                       No options = one List row "No options yet".
//   sheet    the Bottom sheet in overlay mode, the screen's last child (one, moved in place per row)
// Every change is kept in this panel's deck (fw::spec::deck_set: "source", "params.<name>"; stored like a sent
// deck) and emitted as "deck" "<screen> <key>=<value>" for the host. The screen opens with them next time.
// Props: id = the screen (a deck screen id). Update path: none needed (built each time it opens).
#include <map>
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include <set>
#include "sources.h"
#include "device.h"
#include "sheet.h"
#include <algorithm>

namespace {
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
// a group's Section header (size label); not the first: a group apart (the spec renderer's rule, as Settings)
void group(lv_obj_t *c, const char *label, bool first) {
  Doc sh;
  sh["title"] = label, sh["size"] = "label", sh["badge"] = false, sh["right"] = "none";
  lv_obj_t *h = build("section_header", c, sh);
  if (h && !first) fx::use(h, "above:group");
}
// a List row's end slot (shown once it holds something), where its trailing control goes (C8a)
lv_obj_t *end_of(lv_obj_t *row) {
  lv_obj_t *e = fw::spec::slot_of(row, 0);
  if (!e) return row;
  lv_obj_remove_flag(e, LV_OBJ_FLAG_HIDDEN);
  return e;
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
  std::set<std::string> off;  // ids shown greyed, not pickable (a Home Assistant integration that is not set up, #156)
  std::map<std::string, std::pair<std::string, std::string>> detect;  // id -> its integration + domain (sources.json detect)
  std::map<std::string, std::string> status0;  // id -> its status line when it is set up
  bool ha = false, multi = false, auto_ok = false, tried = false;
  std::string dom, prefix, cur;     // cur: the single pick's value ("auto" = Auto)
  std::vector<std::string> chosen;  // multi: the picks
  std::vector<std::string> pkey, pval;  // the Source: a parameter a row sets too (its own sensor, sources.json "finds")
};
struct Page {
  lv_obj_t *root = nullptr, *sheet = nullptr, *home = nullptr;
  uint32_t seen = 0;
  bool dirty = false;
  std::string id;
  std::vector<Pick> picks;
  int open = -1;                                           // the pick the sheet shows
  // "from": the row saying what the picked source found (sources.json "from", the feed's "found" attribute)
  lv_obj_t *from_row = nullptr;
  std::string from_feed, from_shown = "\x01";  // \x01: not shown yet
  int src_pick = -1;                    // the Source pick in picks (-1 = one source, src_from[0])
  std::vector<std::string> src_from;    // per source: its "from" words ("" = none)
  // "value": a source whose Source row value adds a word of the feed's (sources.json "value": the feed attribute, the
  // Overhead's "centre": "adsb.lol · centred on Home"), following the pick live
  lv_obj_t *src_row = nullptr;
  std::string wait_reg;  // a registry domain not read yet when the Source was worked out: built again once it is (#129)
  std::map<std::string, std::string> src_value;  // source id -> its "value" attribute (none = the name alone)
  std::string src_shown = "\x01";
  uint32_t src_ticks = 0;
  std::vector<std::pair<lv_obj_t *, std::string>> ctrls;  // Segmented / Toggle -> its key
};
bool keep(Page *pg, lv_obj_t *from, const std::string &key, ArduinoJson::JsonVariantConst v) { return fw::spec::deck_change(from, pg->id, key, v); }
// a pick's row shows its new value (the List row's update takes the whole row: title and icon kept)
void row_value(Pick &p, const std::string &value) {
  Doc rp;
  rp["title"] = p.key == "source" ? "Source" : p.title.c_str(), rp["icon"] = "none", rp["trailing"] = "value-chevron", rp["value"] = value.c_str();
  fw::update_child(fw::find("list_row"), p.row, rp.as<fw::Props>());
}
namespace hl = fw::dev::ha_link;
constexpr size_t PAGE = 24;  // at most this many entity rows in the sheet; more = rows by the next letter
std::string upper(std::string s) { for (char &c : s) c = (char) toupper((unsigned char) c); return s; }
// the sheet's rows for an HA pick, from the link as it is now; starts the read of the domains not cached yet, and once
// per open of the stale ones (in the background: their old list shows; the sheet follows the read through hl::rev)
void fill_ha(Pick &p) {
  p.ids.clear(), p.names.clear(), p.status.clear();
  p.sel = -1;
  auto row = [&](const std::string &a, const std::string &n, const std::string &st) { p.ids.push_back(a), p.names.push_back(n), p.status.push_back(st); };
  if (!hl::ready()) { row("set", "Home Assistant not connected", "set it up in Settings > Home Assistant"); return; }
  std::string need, old;
  std::vector<const hl::Entity *> all;
  for (const std::string &d : split(p.dom)) {
    if (const std::vector<hl::Entity> *l = hl::list(d)) {
      for (const hl::Entity &e : *l) all.push_back(&e);
      if (hl::stale(d)) old += (old.empty() ? "" : ",") + d;
    } else need += (need.empty() ? "" : ",") + d;
  }
  if (!need.empty()) {
    if (hl::status() == hl::FAIL && p.tried) { row("retry", "Home Assistant not reached", hl::why()); row("retry", "Try again", ""); return; }
    if (hl::status() != hl::LOADING && !*hl::fetch(old.empty() ? need : need + "," + old)) p.tried = true;
    row("", "Reading Home Assistant", "loading " + need);
    return;
  }
  if (!old.empty() && !p.tried && hl::status() != hl::LOADING && !*hl::fetch(old)) p.tried = true;  // the old list shows meanwhile
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
        row("e:" + m[k]->id, name_of(m[k]), m[k]->id);
        if (!p.multi && m[k]->id == p.cur) p.sel = (int) p.ids.size() - 1;
      }
      i = j;
    }
  } else
    for (const hl::Entity *e : m) {
      row("e:" + e->id, name_of(e), e->id);
      if (!p.multi && e->id == p.cur) p.sel = (int) p.ids.size() - 1;
    }
}
// the Source's Home Assistant integrations not set up (#156): still listed, greyed, "Not set up in Home Assistant", so
// the user sees what they could add; checked again each time the sheet opens (an integration added meanwhile)
void grey_unset(Pick &p) {
  namespace hl = fw::dev::ha_link;
  for (size_t i = 0; i < p.ids.size(); i++) {
    auto d = p.detect.find(p.ids[i]);
    if (d == p.detect.end()) continue;
    hl::reg_fetch(d->second.second);
    const std::vector<hl::RegRow> *r = hl::reg(d->second.second);
    const bool set_up = !r || std::any_of(r->begin(), r->end(), [&](const hl::RegRow &e) { return e.platform == d->second.first; });
    // only a change of state touches the line: a source with several candidates keeps each row's own words (its sensor)
    if (set_up) { if (p.off.erase(p.ids[i])) p.status[i] = p.status0[p.ids[i]]; }
    else if (p.off.insert(p.ids[i]).second || p.status[i] != "Not set up in Home Assistant") p.status[i] = "Not set up in Home Assistant";
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
    // the radio is the one mark: a letter row (HA picks) = chevron, every other row no icon; a pick-many list
    // (entities) rings each chosen one, a choose-one list the selected one
    const bool letter = p.ha && p.ids[i].rfind("g:", 0) == 0;
    it["icon"] = letter ? "chevron" : "none", it["name"] = p.names[i].c_str(), it["status"] = p.status[i].c_str();
    if (p.off.count(p.ids[i])) it["state"] = "disabled";
    if (p.multi && !letter)  // a pick-many list: its checkbox says it (no "chosen" word)
      it["selected"] = p.ids[i].rfind("e:", 0) == 0 && std::find(p.chosen.begin(), p.chosen.end(), p.ids[i].substr(2)) != p.chosen.end();
  }
  d["selected"] = p.sel;
}

const char *META = R"({
  "role": "detail",
  "about": "One screen's settings on this panel: what it shows, where its data comes from and its own options (Media: the visual, the player). Opened by tapping a screen in Settings > Screens, or nav.open screen_set with params {id}.",
  "frame": [480, 480],
  "icons": ["back@glyph", "chevron@icon", "check@icon", "close@glyph"],
  "props": [
    {"name": "id", "about": "Which screen's settings to show (a screen id of the deck or the factory).", "type": "select", "options": ["bins", "calendar", "clock", "energy", "living_room", "media", "moon", "overhead", "timer", "weather"], "default": "media"}
  ]
})";

void build_page(lv_obj_t *parent, fw::Props p) {
  auto *pg = new Page;
  pg->id = p["id"] | "media";
  pg->root = fx::box(parent, "fill");
  lv_obj_add_event_cb(pg->root, [](lv_event_t *e) {
    auto *pg = (Page *) lv_event_get_user_data(e);
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
  const fx::Page fp = fx::page(pg->root, "list");  // Screen > Page > Header + list Body (C1-C3)
  build("header", fp.head, hd);
  lv_obj_t *c = fp.body;

  // shows: what the screen is, in words
  {
    ArduinoJson::JsonVariantConst sc = cat["screens"][pg->id];
    std::string about = sc["shows"] | "";
    const std::string needs = sc["needs"] | "";
    if (!needs.empty()) about += std::string(about.empty() ? "Needs " : " Needs ") + needs + ".";
    else if (!about.empty()) about += " Works in every home.";
    if (!about.empty()) {
      group(c, "shows", true);
      Doc td;  // the Text component: up to 4 lines of body text, then "..."
      td["text"] = about.c_str(), td["style"] = "body", td["lines"] = 4;
      build("text", c, td);
    }
  }
  // source (a screen's card on Home is added and removed like any card: the editor's Add card / delete)
  group(c, "source", !cat["screens"][pg->id]["shows"].is<const char *>() && !cat["screens"][pg->id]["needs"].is<const char *>());
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
    if (x["line"].is<const char *>()) { pk.status.push_back(x["line"] | ""); continue; }  // its own one line (the sheet)
    ArduinoJson::JsonArrayConst setup = x["setup"].as<ArduinoJson::JsonArrayConst>();
    if (setup.size()) {
      std::string need;
      for (ArduinoJson::JsonVariantConst s : setup) need += std::string(need.empty() ? "" : " + ") + (s | "");
      st += " \xC2\xB7 needs " + need;
    }
    pk.status.push_back(st);
  }
  {  // an integration's source is pickable only when Home Assistant has it (its registry, once read; until then all are)
    size_t k = 0;
    for (ArduinoJson::JsonVariantConst x : srcs) {
      const std::string in = x["detect"]["integration"] | "", dom = x["detect"]["domain"] | "";
      if (!in.empty() && !dom.empty()) pk.detect[pk.ids[k]] = {in, dom}, pk.status0[pk.ids[k]] = pk.status[k];
      k++;
    }
    grey_unset(pk);
    for (auto &dt : pk.detect) if (!fw::dev::ha_link::reg(dt.second.second)) pg->wait_reg = dt.second.second;
  }
  if (linked)  // only the sources this firmware has an adapter for: a planned one (Spotify, an iCal link) picked here
    for (size_t i = pk.ids.size(); i-- > 0;)  // left the screen with nothing to read (Offline, its settings gone)
      if (pk.sets[i].empty()) {
        pk.ids.erase(pk.ids.begin() + (long) i), pk.names.erase(pk.names.begin() + (long) i);
        pk.sets.erase(pk.sets.begin() + (long) i), pk.status.erase(pk.status.begin() + (long) i);
      }
  bool sample_opt = false;
  for (ArduinoJson::JsonVariantConst o : sparam["options"].as<ArduinoJson::JsonArrayConst>()) sample_opt |= !strcmp(o | "", "sample");
  if (linked && sample_opt && !has_sample) {  // previews: the spec's sample adapter, not a catalogue source
    pk.ids.push_back(fw::src::SAMPLE), pk.names.push_back(fw::src::SAMPLE_NAME), pk.sets.push_back("sample");
    pk.status.push_back(cat["kinds"]["builtin"] | "");
  }
  {  // a Home Assistant source that finds its own entity (sources.json "finds"): one row per candidate only when HA
     // has more than one (the row sets the parameter too), else the source finds it itself (the parameter cleared)
    Pick ex = pk;
    ex.ids.clear(), ex.names.clear(), ex.sets.clear(), ex.status.clear();
    for (size_t i = 0; i < pk.ids.size(); i++) {
      ArduinoJson::JsonVariantConst f;
      for (ArduinoJson::JsonVariantConst x : srcs) if (pk.ids[i] == (x["id"] | "")) f = x["finds"];
      const std::string fk = f.isNull() ? "" : std::string("params.") + (f["param"] | "");
      const std::vector<std::string> cands = f.isNull() ? std::vector<std::string>() : fw::src::candidates(f);
      const size_t n = cands.size() > 1 ? cands.size() : 1;
      for (size_t k = 0; k < n; k++) {
        ex.ids.push_back(pk.ids[i]), ex.names.push_back(pk.names[i]), ex.sets.push_back(pk.sets[i]);
        ex.status.push_back(n > 1 ? friendly(cands[k]) : pk.status[i]);
        ex.pkey.push_back(fk), ex.pval.push_back(n > 1 ? cands[k] : "");
      }
    }
    pk = ex;
  }
  for (size_t i = 0; i < pk.ids.size(); i++)
    if (pk.ids[i] == cur && (pk.pval[i].empty() || pk.pval[i] == (entry["params"][pk.pkey[i].substr(7)] | ""))) { pk.sel = (int) i; break; }
  Doc sr;
  sr["title"] = "Source", sr["icon"] = "none";
  sr["value"] = pk.ids.empty() ? "Built in" : pk.names[pk.sel].c_str();
  sr["trailing"] = pk.ids.size() > 1 ? "value-chevron" : "value";
  lv_obj_t *srow = build("list_row", c, sr);
  pg->src_row = srow;
  for (ArduinoJson::JsonVariantConst x : srcs) if (x["value"].is<const char *>()) pg->src_value[x["id"] | ""] = x["value"] | "";
  if (lv_obj_t *r = srow; r && pk.ids.size() > 1) {
    pk.row = r;
    pg->src_pick = (int) pg->picks.size();
    pg->picks.push_back(pk);
  }
  // from: a source that reads a standard place in HA (Energy: HA's Energy dashboard) says what it found
  // there, live from the feed's "found" attribute (the adapter reads it when asked, core/ha.h local)
  for (ArduinoJson::JsonVariantConst x : srcs) pg->src_from.push_back(x["from"] | "");
  for (ArduinoJson::JsonPairConst f : spec["feeds"].as<ArduinoJson::JsonObjectConst>()) { pg->from_feed = f.key().c_str(); break; }
  if (!pg->from_feed.empty() && !pg->src_value.empty())
    if (auto fresh = fw::ha::local(pg->from_feed)) fresh();  // the feed's words for the Source value
  if (!pg->from_feed.empty() && std::any_of(pg->src_from.begin(), pg->src_from.end(), [](const std::string &f) { return !f.empty(); })) {
    if (auto fresh = fw::ha::local(pg->from_feed)) fresh();
    Doc fr;
    fr["title"] = "From", fr["icon"] = "none", fr["trailing"] = "none", fr["sub"] = "";
    pg->from_row = build("list_row", c, fr);
    if (pg->from_row) lv_obj_remove_flag(pg->from_row, LV_OBJ_FLAG_CLICKABLE);
  }

  // options: the spec's typed parameters
  bool any = false;
  auto value = [&](const char *k, ArduinoJson::JsonVariantConst def) {
    const char *v = entry["params"][k] | (const char *) nullptr;
    return v ? fw::spec::param_value(def, v) : fw::spec::param_default(def);  // an old pick no longer offered: the default
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
      // the option words: the spec's labels (labels_mi with the distance unit miles), else the option itself
      const char *du = fw::dev::setting("distanceUnit");
      ArduinoJson::JsonArrayConst lab = kv.value()[du && !strcmp(du, "mi") && kv.value()["labels_mi"].is<ArduinoJson::JsonArrayConst>() ? "labels_mi" : "labels"];
      auto word = [&](size_t i, const char *o) { return i < lab.size() ? std::string(lab[i] | o) : label_of(o); };
      if (opts.size() <= 4 && lab.isNull()) {  // a List row (title only, not a tap target) + the Segmented as its trailing part
        rd["trailing"] = "none";
        lv_obj_t *r = build("list_row", c, rd);
        if (!r) continue;
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
        std::string labels;
        for (ArduinoJson::JsonVariantConst o : opts) labels += std::string(labels.empty() ? "" : ", ") + label_of(o | "");
        Doc sd;
        sd["options"] = labels.c_str(), sd["selected"] = v.c_str();
        if (lv_obj_t *sg = build("segmented", end_of(r), sd)) pg->ctrls.push_back({sg, "params." + key});
        continue;
      }
      // with labels (words with a unit: "1,000 ft"), or more than 4: a value row + the sheet, so the title always shows
      Pick pk;
      for (size_t i = 0; i < opts.size(); i++) { pk.ids.push_back(opts[i] | ""); pk.names.push_back(word(i, opts[i] | "")); pk.status.push_back(""); }
      std::string vw = label_of(v);
      for (size_t i = 0; i < pk.ids.size(); i++) if (pk.ids[i] == v) vw = pk.names[i];
      rd["trailing"] = "value-chevron", rd["value"] = vw.c_str();
      lv_obj_t *r = build("list_row", c, rd);
      if (!r) continue;
      pk.key = "params." + key, pk.row = r, pk.title = label;
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
      if (!p.detect.empty()) grey_unset(p);
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
          row_value(p, p.chosen.empty() ? "None" : std::to_string(p.chosen.size()) + " chosen");
          keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
          return;
        }
        p.cur = a == "auto" ? "auto" : a.substr(2);
        v.set(p.cur.c_str());
        row_value(p, p.names[i]);
        keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
        fw::sheet::close(pg->sheet);
        pg->dirty = false;
        return;
      }
      p.sel = i;
      row_value(p, p.names[i]);
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
      if (p.key == "source" && i < (int) p.pkey.size() && !p.pkey[i].empty()) {  // its own sensor ("" = found by itself)
        Doc v;
        v.set(p.pval[i].c_str());
        keep(pg, pg->sheet, p.pkey[i], v.as<ArduinoJson::JsonVariantConst>());
      }
    }, LV_EVENT_VALUE_CHANGED, pg);
  // an open HA pick follows the link (a read ends) and a tap that changes its rows (rebuilt here, after the tap)
  fx::every(pg->root, 50, [pg] {
    // the Source was worked out before Home Assistant's registry came: once it has, the page again (the row names the
    // integration the forecast really comes from, the greyed ones are right)
    if (!pg->wait_reg.empty() && fw::dev::ha_link::reg(pg->wait_reg)) { pg->wait_reg.clear(); fw::dev::reload("screen_set"); return; }
    if (pg->src_row && !pg->from_feed.empty() && pg->src_pick >= 0) {  // the Source value's feed word follows the pick and the feed
      const Pick &sp = pg->picks[pg->src_pick];
      const int sel = sp.sel;
      std::string attr;  // the picked source's "value" (by its id: the picks may leave sources out)
      if (sel >= 0 && sel < (int) sp.ids.size()) { auto it = pg->src_value.find(sp.ids[sel]); if (it != pg->src_value.end()) attr = it->second; }
      if (!attr.empty()) {
        // the feed runs only for what shows it (a bind): asked again every half second while this page shows it, so the
        // word follows Home Assistant (its home arriving after the page was built)
        if (++pg->src_ticks % 10 == 0) if (auto fresh = fw::ha::local(pg->from_feed)) fresh();
        const std::string *w = fw::ha::get(pg->from_feed, attr.c_str());
        const std::string shown = sp.names[sel] + (w && !w->empty() ? " \xC2\xB7 " + *w : "");
        if (shown != pg->src_shown) { pg->src_shown = shown; row_value(pg->picks[pg->src_pick], shown); }
      }
    }
    if (pg->from_row) {  // the source's "from" row follows the pick and what the feed found
      const int sel = pg->src_pick >= 0 ? pg->picks[pg->src_pick].sel : 0;
      const std::string from = sel >= 0 && sel < (int) pg->src_from.size() ? pg->src_from[sel] : "";
      const std::string *found = fw::ha::get(pg->from_feed, "found");
      const std::string shown = from.empty() ? "" : from + "\n" + (found ? *found : "Reading Home Assistant");
      if (shown != pg->from_shown) {
        pg->from_shown = shown;
        if (from.empty()) lv_obj_add_flag(pg->from_row, LV_OBJ_FLAG_HIDDEN);
        else {
          lv_obj_remove_flag(pg->from_row, LV_OBJ_FLAG_HIDDEN);
          Doc fr;
          fr["title"] = ("From " + from).c_str(), fr["icon"] = "none", fr["trailing"] = "none", fr["sub"] = found ? found->c_str() : "Reading Home Assistant";
          fw::update_child(fw::find("list_row"), pg->from_row, fr.as<fw::Props>());
        }
      }
    }
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
  });
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
        if (flip) v.set(!strcmp(detail, "on") ? "true" : "false");
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
