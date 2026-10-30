// Home Assistant (Settings > Home Assistant, 2026-10-30): the panel's own link to Home Assistant (core/device.h
// ha_link), so the entity pickers have lists on a bare panel. Composed only of real components, laid out as Settings:
//   header   the Header (back + "Home Assistant" + the screen chrome)
//   content  one scrolling column, groups as Settings (label, section-gap, rows; groups lay::group apart):
//            "connection"  List row "Address" (the URL, or "Not set") and List row "Token" (the last 4 characters
//                          only, "Not set"): set by the action ha.connect (an agent, HA, the workbench's props)
//            "entities"    List row "Entity lists", its sub line the status (Not set up / Ready / Reading Home
//                          Assistant / "N entities" / the failure in words), a real Button "Read now" (secondary)
//                          appended as its trailing part: ha.entities over the domains the screens' pickers use
//                          (PICK_DOMAINS); event "ha_link" "entities" or why not
//   The rows follow the link while the page shows (a 250 ms timer compares ha_link::rev() and builds them again).
//   Read now shows only when the link is set up and not reading.
// Props (the workbench): url, token = ha.connect on build when given (a token typed here stays in the module; the
// workbench's own fetch goes through the hub with the hub's token). Update path: none needed (rows follow the link).
#include "fw.h"
#include "device.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
namespace hl = fw::dev::ha_link;
constexpr const char *PICK_DOMAINS = "light,sensor,binary_sensor,media_player,calendar,weather,climate,cover,lock,vacuum,camera,switch,fan,scene,script";
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
std::string status_words() {
  switch (hl::status()) {
    case hl::OFF: return "Not set up";
    case hl::IDLE: return "Ready";
    case hl::LOADING: return "Reading Home Assistant";
    case hl::OK: return std::to_string(hl::count()) + " entities";
    default: return "Not reached: " + hl::why();
  }
}
struct Page { lv_obj_t *root = nullptr, *col = nullptr, *read = nullptr; lv_timer_t *tick = nullptr; uint32_t rev = 0; };
// the rows, from the link as it is now (built again whenever it changes: a List row update is not a merge, and the
// Status row carries its Button)
void rows(Page *pg) {
  pg->rev = hl::rev();
  lv_obj_t *c = pg->col;
  lv_obj_clean(c);
  pg->read = nullptr;
  group(c, "connection", true);
  Doc a;
  a["title"] = "Address", a["icon"] = "none", a["trailing"] = "value", a["value"] = hl::url().empty() ? "Not set" : hl::url().c_str();
  if (lv_obj_t *r = build("list_row", c, a)) lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  Doc t;
  t["title"] = "Token", t["icon"] = "none", t["trailing"] = "value", t["value"] = hl::has_token() ? hl::token_hint().c_str() : "Not set";
  if (lv_obj_t *r = build("list_row", c, t)) lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  group(c, "entities", false);
  Doc s;
  s["title"] = "Entity lists", s["icon"] = "none", s["trailing"] = "none", s["sub"] = status_words().c_str();
  if (lv_obj_t *r = build("list_row", c, s)) {
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    Doc rb;
    rb["kind"] = "secondary", rb["label"] = "Read now";
    if (hl::ready() && hl::status() != hl::LOADING) pg->read = build("button", r, rb);
  }
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > Home Assistant: the panel's own link to Home Assistant (its address and a long-lived token, kept on the panel; the token is never shown back) and the entity lists the screens' pickers offer, read from Home Assistant on demand. Set with the action ha.connect; Read now = ha.entities.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "url", "about": "Home Assistant's address (ha.connect on open; empty = leave it).", "type": "text", "default": ""},
    {"name": "token", "about": "A long-lived access token (ha.connect on open; empty = keep the one set). The workbench reads through the hub with the hub's own token.", "type": "text", "default": ""}
  ]
})";

void build_page(lv_obj_t *parent, fw::Props p) {
  const fw::Theme &th = *fw::T;
  const std::string pu = p["url"] | "", pt = p["token"] | "";
  if (!pu.empty() && (pu != hl::url() || !pt.empty())) hl::configure(pu, pt);
  auto *pg = new Page;
  pg->root = box(parent);
  lv_obj_set_size(pg->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(pg->root, th.bg, 0);
  lv_obj_set_style_bg_opa(pg->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(pg->root, [](lv_event_t *e) {
    auto *pg = (Page *) lv_event_get_user_data(e);
    if (pg->tick) lv_timer_delete(pg->tick);
    delete pg;
  }, LV_EVENT_DELETE, pg);
  Doc hd;
  hd["label"] = "Home Assistant";
  if (lv_obj_t *h = build("header", pg->root, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);
  lv_obj_t *c = box(pg->root);
  lv_obj_add_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  pg->col = c;
  rows(pg);

  pg->tick = lv_timer_create([](lv_timer_t *tm) {
    auto *pg = (Page *) lv_timer_get_user_data(tm);
    if (pg->rev != hl::rev()) rows(pg);
  }, 250, pg);
  fw::listen(pg->root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *pg = (Page *) user;
    if (!strcmp(name, "tap") && pg->read)
      for (lv_obj_t *o = from; o; o = lv_obj_get_parent(o))
        if (o == pg->read) {
          const char *why = hl::fetch(PICK_DOMAINS);
          fw::emit(pg->read, "ha_link", *why ? why : "entities");
          return;
        }
    if (lv_obj_t *up = lv_obj_get_parent(pg->root)) fw::notify(up, name, detail);
  }, pg);
}
const int reg = fw::add({"ha_link", "Home Assistant", "screens", META, build_page});
}  // namespace

FW_UPDATE(ha_link) { return false; }  // url / token = ha.connect on build
