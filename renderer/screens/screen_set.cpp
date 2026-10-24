// Screen settings (Settings > Screens > a screen, 2026-10-23): one screen's picks on this panel. Composed only of
// real components, laid out as the Settings screen:
//   header   the Header (back + the screen's name + the screen chrome)
//   content  one scrolling column, groups as Settings (label, section-gap, rows; groups lay::group apart):
//            "home"     List row "Show on Home" with its Toggle (the whole row): the deck entry's on
//            "source"   List row "Source" + the source's name (core/sources.h); more than one source = a chevron and
//                       a tap opens the Bottom sheet with the screen's sources (name; status = the kind, "needs <setup>"
//                       for a direct source that needs setting up). Built-in screens read "Built in".
//            "options"  one row per typed parameter of the screen's spec (core/spec.h "params", hidden ones left
//                       out): select = a List row + Segmented (up to 4 options) or the sheet; toggle = a List row
//                       toggle; entity = its friendly name, and a sheet of the screen's list of the same domain when
//                       it has one (Media: the Player from the Speakers, Auto first when the spec's screen.param
//                       follows a sensor: "auto" or nothing picked = Auto); entities = "N chosen"; text = the value.
//                       Lists of entities and text come from the host's discovery (the hub writes them into the deck).
//   sheet    the Bottom sheet in overlay mode, the screen's last child (one, moved in place per row)
// Every change is kept in this panel's deck (fw::spec::deck_set: "on", "source", "params.<name>"; stored like a sent
// deck) and emitted as "deck" "<screen> <key>=<value>" for the host. The screen opens with them next time.
// Props: id = the screen (a deck screen id). Update path: none needed (built each time it opens).
#include "fw.h"
#include "spec.h"
#include "sources.h"

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
struct Pick { std::string key; std::vector<std::string> ids, names, status; lv_obj_t *row = nullptr; std::string title; int sel = 0; };
struct Page {
  lv_obj_t *root = nullptr, *sheet = nullptr, *home = nullptr;
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
void sheet_props(Page *pg, int k, bool open, Doc &d) {
  d["overlay"] = true;
  d["open"] = open ? "open" : "closed";
  if (k < 0 || k >= (int) pg->picks.size()) { d["title"] = ""; d["items"].to<ArduinoJson::JsonArray>(); d["selected"] = 0; return; }
  const Pick &p = pg->picks[k];
  d["title"] = p.title.c_str();
  ArduinoJson::JsonArray items = d["items"].to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < p.ids.size(); i++) {
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    it["icon"] = "check", it["name"] = p.names[i].c_str(), it["status"] = p.status[i].c_str();
  }
  d["selected"] = p.sel;
}

const char *META = R"({
  "about": "One screen's settings on this panel: on Home or not, where its data comes from and its own options (Media: the visual, the player).",
  "frame": [480, 480],
  "icons": ["back@glyph", "chevron@icon", "check@icon", "close@glyph"],
  "props": [
    {"name": "id", "type": "select", "options": ["media", "weather", "calendar", "energy", "thermostat", "covers", "clock"], "default": "media"}
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
  lv_obj_add_event_cb(pg->root, [](lv_event_t *e) { delete (Page *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, pg);

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

  // home
  group(c, "home", true);
  Doc hr;
  hr["title"] = "Show on Home", hr["icon"] = "none", hr["trailing"] = "toggle", hr["on"] = fw::spec::deck_on(entry);
  if (lv_obj_t *r = build("list_row", c, hr)) pg->ctrls.push_back({r, "on"});

  // source
  group(c, "source", false);
  ArduinoJson::JsonArrayConst srcs = fw::src::of(cat, pg->id);
  const std::string cur = fw::src::picked(cat, pg->id, entry);
  Doc sr;
  sr["title"] = "Source", sr["icon"] = "none";
  sr["value"] = srcs.size() ? fw::src::name(cat, pg->id, cur).c_str() : "Built in";
  sr["trailing"] = srcs.size() > 1 ? "value-chevron" : "value";
  if (lv_obj_t *r = build("list_row", c, sr); r && srcs.size() > 1) {
    Pick pk;
    pk.key = "source", pk.row = r, pk.title = "source";
    for (ArduinoJson::JsonVariantConst x : srcs) {
      pk.ids.push_back(x["id"] | "");
      pk.names.push_back(x["name"] | "");
      std::string st = cat["kinds"][x["kind"] | ""] | "";
      ArduinoJson::JsonArrayConst setup = x["setup"].as<ArduinoJson::JsonArrayConst>();
      if (setup.size()) {
        std::string need;
        for (ArduinoJson::JsonVariantConst s : setup) need += std::string(need.empty() ? "" : " + ") + (s | "");
        st += " \xC2\xB7 needs " + need;
      }
      pk.status.push_back(st);
    }
    for (size_t i = 0; i < pk.ids.size(); i++) if (pk.ids[i] == cur) pk.sel = (int) i;
    pg->picks.push_back(pk);
  }

  // options: the spec's typed parameters
  bool any = false;
  ArduinoJson::JsonObjectConst params = spec["params"].as<ArduinoJson::JsonObjectConst>();
  auto value = [&](const char *k, ArduinoJson::JsonVariantConst def) {
    const char *v = entry["params"][k] | (const char *) nullptr;
    return v ? std::string(v) : fw::spec::param_default(def);
  };
  for (ArduinoJson::JsonPairConst kv : params) {
    if (!kv.value().is<ArduinoJson::JsonObjectConst>()) continue;
    const std::string type = kv.value()["type"] | "", key = kv.key().c_str();
    if (type == "hidden" || type.empty()) continue;
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
      std::string auto_src;
      for (ArduinoJson::JsonVariantConst nd : spec["nodes"].as<ArduinoJson::JsonArrayConst>())
        for (ArduinoJson::JsonPairConst o : nd["on"].as<ArduinoJson::JsonObjectConst>())
          if (!strcmp(o.value()["do"] | "", "screen.param") && key == (o.value()["param"] | "") && o.value()["auto"].is<const char *>()) {
            const char *ak = o.value()["auto"];
            auto_src = value(ak, params[ak]);
          }
      if (!fw::spec::entity_ok(auto_src) || auto_src[0] == '$') auto_src.clear();
      const bool is_auto = !auto_src.empty() && (v == "auto" || entry["params"][key.c_str()].isNull());
      const size_t n = from.size() + (auto_src.empty() ? 0 : 1);
      rd["value"] = is_auto ? "Auto" : v.empty() || v == "auto" ? "" : friendly(v).c_str();
      rd["trailing"] = n > 1 ? "value-chevron" : "value";
      lv_obj_t *r = build("list_row", c, rd);
      if (!r || n < 2) continue;
      Pick pk;
      pk.key = "params." + key, pk.row = r, pk.title = label;
      if (!auto_src.empty()) { pk.ids.push_back("auto"); pk.names.push_back("Auto"); pk.status.push_back("follows what's playing"); }
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
    rd["trailing"] = "value";
    rd["value"] = type == "entities" ? (split(v).empty() ? "" : std::to_string(split(v).size()) + " chosen") : v;
    build("list_row", c, rd);
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
      p.sel = i;
      Doc rp;
      rp["value"] = p.names[i].c_str();
      fw::update_child(fw::find("list_row"), p.row, rp.as<fw::Props>());
      Doc v;
      v.set(p.ids[i].c_str());
      keep(pg, pg->sheet, p.key, v.as<ArduinoJson::JsonVariantConst>());
    }, LV_EVENT_VALUE_CHANGED, pg);
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
