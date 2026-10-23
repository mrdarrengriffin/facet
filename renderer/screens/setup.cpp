// Set up: the system screen a panel shows until it is on Wi-Fi and added to Home Assistant (design: claude-assistant
// notes/facet-design/install-onboarding.md section 3, Darren 2026-10-23). Built in, not in the deck, no Back while the
// panel is not set up (prop back = reached from Settings > Wi-Fi and Home Assistant later). State comes from firmware
// facts only (Wi-Fi, hotspot clients, Improv, API, an "adopted" pref); here every state is a prop so the workbench
// shows each one. Plain words on screen: no "Improv" or "captive portal".
// Composed only of real components:
//   header   the Header (label "Set up", the Status chip with the state's tone + words (internal chip_tone /
//            chip_text), the live clock only once the panel is on Wi-Fi: before that it has no time) at (inset, inset)
//   body     one region from lay::top to the bottom inset, made again on a state / way change (fw::st::refill_fade:
//            what stays fades, what is new appears; silent):
//     720    with a code: the QR block on the left, a column on the right (lay::group apart): Headline title, body
//            text (wraps), facts as Stats in a row, the ways as Picker rows (one group: a tap shows that way)
//            without a code: the column alone, full width
//     480    one column: title, the QR block centred, the facts, the text, then a secondary Button "Other ways" that
//            opens a Bottom sheet (overlay, the screen's last child) with the ways as picker rows
//     foot   the device name (+ its address once on Wi-Fi), screen type label, text-dim, at the bottom
// States (prop state):
//   no-wifi       warn "No Wi-Fi"      hotspot: Wi-Fi QR with the per-panel password (WPA), the network + password
//                                      as facts; or the way picked: Bluetooth (HA app) / USB cable (docs Install)
//   phone-joined  info "Phone joined"  QR to the setup page (http://192.168.4.1) if it did not open by itself
//   allow         info "Asking"        Home Assistant asks to send Wi-Fi details over Bluetooth: Allow / Not now
//   connecting    working (pulse)      the Wi-Fi name, "Joining your Wi-Fi…"
//   failed        error                "Could not join <wifi>", what to check, the hotspot comes back by itself
//   connected     ok "On Wi-Fi"        "Add me in Home Assistant": QR to HA's integrations page, name + address
//   added         ok "Connected"       "Connected to Home Assistant" (the panel then opens Home with a toast)
//   actions       warn "Actions off"   Home Assistant refuses this panel's actions: where to turn them on
// Events: "setup" detail allow / not-now (the Allow buttons), "way" detail hotspot|bluetooth|usb (a way picked).
// Update path: header in place (the chip's tone fades), body made again with a fade (refill_fade), silent.
#include <algorithm>
#include <functional>
#include <string>
#include "fw.h"

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
lv_obj_t *col(lv_obj_t *parent, int w) {  // a flex column, content height
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, fw::lay::section_gap, 0);
  return c;
}
lv_obj_t *wrap_text(lv_obj_t *parent, fw::ty::Id s, const std::string &txt, lv_color_t c) {
  lv_obj_t *l = fw::text(parent, s, txt.c_str(), c);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(l, LV_PCT(100));  // wraps; fw::text leaves a wrapping label's height to its content
  return l;
}

const char *const WAYS[] = {"hotspot", "bluetooth", "usb"};
const char *way_name(const std::string &w) { return w == "bluetooth" ? "Bluetooth" : w == "usb" ? "USB cable" : "Hotspot"; }
const char *way_hint(const std::string &w) {
  return w == "bluetooth" ? "Home Assistant app" : w == "usb" ? "a computer" : "scan with your phone";
}
const char *HA_ADD = "https://my.home-assistant.io/redirect/integrations/";
const char *DOCS_INSTALL = "https://mrdarrengriffin.github.io/facet/install";

// what one state shows: the screen's whole model, from the props (ONE place: the build and the update share it)
struct View {
  std::string chip_tone, chip_text;
  bool pulse = false, clock = false;
  std::string qr, qr_caption;                         // empty = no code
  std::string title, text;
  std::vector<std::pair<std::string, std::string>> facts;  // caps label, value
  bool ways = false;                                  // the ways list (720 rows / 480 "Other ways")
  std::string primary, secondary;                     // buttons (allow)
  bool ip = false;                                    // the foot shows the address
};

struct Screen {
  lv_obj_t *root = nullptr, *header = nullptr, *body = nullptr, *sheet = nullptr;
  Doc props;              // the current props (a way picked on the panel changes "way" here)
  std::vector<std::string> ways;  // the ways this panel has, sheet order
};

std::string upper(std::string s) {
  for (auto &c : s) c = (char) toupper((unsigned char) c);
  return s;
}

View view_of(fw::Props p) {
  const std::string st = p["state"] | "no-wifi", way = p["way"] | "hotspot";
  const std::string ssid = p["ssid"] | "Facet-1A2B", pw = p["password"] | "k7mqx3tpa9", wifi = p["wifi"] | "Larkfield";
  const std::string dev = p["device"] | "Facet panel 1a2b3c", ip = p["ip"] | "192.168.1.42";
  View v;
  if (st == "phone-joined") {
    v.chip_tone = "info", v.chip_text = "Phone joined";
    v.qr = "http://192.168.4.1", v.qr_caption = "open setup";
    v.title = "Pick your Wi-Fi";
    v.text = "Your phone is on the panel's hotspot. If the setup page did not open, scan this code.";
    v.facts = {{"address", "192.168.4.1"}};
  } else if (st == "allow") {
    v.chip_tone = "info", v.chip_text = "Asking";
    v.title = "Home Assistant wants to set up this panel";
    v.text = "Allow it to send your Wi-Fi details over Bluetooth.";
    v.primary = "Allow", v.secondary = "Not now";
  } else if (st == "connecting") {
    v.chip_tone = "working", v.chip_text = "Connecting", v.pulse = true;
    v.title = wifi;
    v.text = "Joining your Wi-Fi…";
  } else if (st == "failed") {
    v.chip_tone = "error", v.chip_text = "Not connected";
    v.title = "Could not join " + wifi;
    v.text = "Check the password and that the panel is in range. The hotspot comes back in a minute.";
  } else if (st == "connected") {
    v.chip_tone = "ok", v.chip_text = "On Wi-Fi", v.clock = true;
    v.qr = HA_ADD, v.qr_caption = "open in HA";
    v.title = "Add me in Home Assistant";
    v.text = "Settings > Devices & services shows a new ESPHome device. Press Add.";
    v.facts = {{"name", dev}, {"address", ip}};
    v.ip = true;
  } else if (st == "added") {
    v.chip_tone = "ok", v.chip_text = "Connected", v.clock = true;
    v.title = "Connected to Home Assistant";
    v.text = "All set. Home opens in a moment.";
    v.ip = true;
  } else if (st == "actions") {
    v.chip_tone = "warn", v.chip_text = "Actions off", v.clock = true;
    v.title = "Let this panel control your home";
    v.text = "Home Assistant: Settings > Devices & services > ESPHome > " + dev +
             " > Configure. Turn on \"Allow the device to perform Home Assistant actions\".";
    v.ip = true;
  } else {  // no-wifi
    v.chip_tone = "warn", v.chip_text = "No Wi-Fi";
    v.ways = true;
    if (way == "bluetooth") {
      v.qr = HA_ADD, v.qr_caption = "open in HA";
      v.title = "Add it with Bluetooth";
      v.text = "In the Home Assistant app, Settings > Devices & services shows this panel. Press Add.";
    } else if (way == "usb") {
      v.qr = DOCS_INSTALL, v.qr_caption = "install page";
      v.title = "Set it up with USB";
      v.text = "Plug the panel into a computer, open the Facet install page and press Connect to Wi-Fi.";
    } else {
      v.qr = "WIFI:T:WPA;S:" + ssid + ";P:" + pw + ";;", v.qr_caption = "join " + ssid;
      v.title = "Join the panel's Wi-Fi";
      v.text = "Scan to join, then pick your home Wi-Fi.";
      v.facts = {{"wi-fi", ssid}, {"password", pw}};
    }
  }
  return v;
}

void header_props(const View &v, fw::Props p, Doc &d) {
  d["label"] = "Set up";
  d["back"] = p["back"] | false;
  d["clock"] = v.clock;
  d["chip"] = true;
  d["chip_tone"] = v.chip_tone.c_str();
  d["chip_text"] = v.chip_text.c_str();
  d["chip_pulse"] = v.pulse;
}

void show_way(Screen *scr, const char *w);
void sheet_props(Screen *scr, bool open, Doc &d);

void facts(lv_obj_t *parent, const View &v) {
  if (v.facts.empty()) return;
  lv_obj_t *row = box(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
  for (const auto &f : v.facts) {
    lv_obj_t *cell = box(row);
    lv_obj_set_size(cell, 0, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(cell, 1);
    Doc sp;
    sp["label"] = f.first.c_str();
    sp["value"] = f.second.c_str();
    sp["tone"] = "text";
    sp["fill"] = true;
    build("stat", cell, sp);
  }
}

void buttons(Screen *scr, lv_obj_t *parent, const View &v) {
  if (v.primary.empty()) return;
  lv_obj_t *row = box(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
  lv_obj_set_style_pad_top(row, fw::lay::section_gap, 0);
  struct B { const std::string &label; const char *kind, *what; } bs[] = {{v.primary, "primary", "allow"},
                                                                          {v.secondary, "ghost", "not-now"}};
  for (const B &b : bs) {
    if (b.label.empty()) continue;
    Doc bp;
    bp["kind"] = b.kind;
    bp["label"] = b.label.c_str();
    lv_obj_t *o = build("button", row, bp);
    if (!o) continue;
    lv_obj_add_event_cb(o, [](lv_event_t *e) {  // after the Button's own tap (event + sound)
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      fw::emit(o, "setup", (const char *) lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, (void *) b.what);
  }
  (void) scr;
}

// the ways as one group of Picker rows (720: in the column; a tap shows that way)
void way_rows(Screen *scr, lv_obj_t *parent, const std::string &cur) {
  lv_obj_t *list = box(parent);
  lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  for (const std::string &w : scr->ways) {
    Doc rp;
    rp["icon"] = "none";
    rp["name"] = way_name(w);
    rp["status"] = way_hint(w);
    rp["selected"] = w == cur;
    lv_obj_t *r = build("picker_row", list, rp);
    if (!r) continue;
    lv_obj_set_width(r, LV_PCT(100));  // the column's width (a List row is content wide by default)
    lv_obj_add_event_cb(r, [](lv_event_t *e) {
      auto *r = (lv_obj_t *) lv_event_get_current_target(e);
      if (lv_obj_has_state(r, fw::st::OFF)) return;
      auto *scr = (Screen *) lv_event_get_user_data(e);
      lv_obj_t *list = lv_obj_get_parent(r);
      const int i = (int) lv_obj_get_index(r);
      if (i >= 0 && i < (int) scr->ways.size()) show_way(scr, scr->ways[i].c_str());
      (void) list;
    }, LV_EVENT_CLICKED, scr);
  }
}

void fill_body(Screen *scr) {
  const fw::Theme &t = *fw::T;
  fw::Props p = scr->props.as<fw::Props>();
  const View v = view_of(p);
  const bool small = fw::lay::screen_w < 600;
  lv_obj_t *b = scr->body;
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(b, fw::lay::section_gap, 0);

  lv_obj_t *main = box(b);
  lv_obj_set_size(main, LV_PCT(100), 0);
  lv_obj_set_flex_grow(main, 1);
  lv_obj_t *c = nullptr, *foot_in = nullptr;
  if (!small && !v.qr.empty()) {  // 720: code left, column right
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(main, fw::lay::group, 0);
    lv_obj_t *left = col(main, fw::lay::qr_block_w);  // the code, the device name under it (the column needs the height)
    Doc qp;
    qp["text"] = v.qr.c_str();
    qp["caption"] = v.qr_caption.c_str();
    build("qr_block", left, qp);
    foot_in = left;
    c = col(main, fw::lay::content_w - fw::lay::qr_block_w - fw::lay::group);
  } else {
    c = col(main, LV_PCT(100));
  }
  wrap_text(c, fw::ty::headline, v.title, t.text);
  if (small && !v.qr.empty()) {  // 480: the code under the title, centred
    lv_obj_t *q = box(c);
    lv_obj_set_size(q, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(q, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(q, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    Doc qp;
    qp["text"] = v.qr.c_str();
    qp["caption"] = v.qr_caption.c_str();
    build("qr_block", q, qp);
  }
  if (small) {
    facts(c, v);
    wrap_text(c, fw::ty::body, v.text, t.text_dim);
  } else {
    wrap_text(c, fw::ty::body, v.text, t.text_dim);
    facts(c, v);
  }
  buttons(scr, c, v);
  if (v.ways && scr->ways.size() > 1) {
    const std::string cur = p["way"] | "hotspot";
    if (!small) way_rows(scr, c, cur);
    else {
      Doc bp;
      bp["kind"] = "secondary";
      bp["label"] = "Other ways";
      if (lv_obj_t *o = build("button", c, bp))
        lv_obj_add_event_cb(o, [](lv_event_t *e) {
          auto *scr = (Screen *) lv_event_get_user_data(e);
          if (!scr->sheet) return;
          Doc d;
          sheet_props(scr, true, d);
          fw::update_child(fw::find("bottom_sheet"), scr->sheet, d.as<fw::Props>());
        }, LV_EVENT_CLICKED, scr);
    }
  }

  // foot: the device name (+ address on Wi-Fi), caps, text-dim
  std::string foot = p["device"] | "Facet panel 1a2b3c";
  if (v.ip) foot += std::string(" \xC2\xB7 ") + (p["ip"] | "192.168.1.42");
  if (foot_in) {  // 720 with a code: under the QR block, wrapping in its width
    wrap_text(foot_in, fw::ty::label, upper(foot), t.text_dim);
    return;
  }
  lv_obj_t *f = fw::text(b, fw::ty::label, upper(foot).c_str(), t.text_dim);
  lv_obj_remove_flag(f, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(f, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(f, LV_PCT(100));
  lv_obj_set_height(f, fw::style(fw::ty::label).box);
}

// the Bottom sheet's props (480 "Other ways"): one picker row per way, the current one ringed
void sheet_props(Screen *scr, bool open, Doc &d) {
  const std::string cur = scr->props["way"] | "hotspot";
  d["open"] = open ? "open" : "closed";
  d["overlay"] = true;
  d["title"] = "ways to set up";
  int sel = 0;
  ArduinoJson::JsonArray items = d["items"].to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < scr->ways.size(); i++) {
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    it["icon"] = "none";
    it["name"] = way_name(scr->ways[i]);
    it["status"] = way_hint(scr->ways[i]);
    if (scr->ways[i] == cur) sel = (int) i;
  }
  d["selected"] = sel;
}

void rebuild_body(Screen *scr, bool animate) {
  fw::st::refill_fade(scr->body, [scr] {
    fw::refill(scr->body);
    fill_body(scr);
  }, animate);
}

void rebuild_async(void *u) { rebuild_body((Screen *) u, true); }

// a way picked on the panel (a 720 row or the 480 sheet): remember it, report it, show it (after the tap returns: the
// row that was tapped is made again)
void show_way(Screen *scr, const char *w) {
  if (std::string(scr->props["way"] | "hotspot") == w) return;
  scr->props["way"] = w;
  fw::emit(scr->root, "way", w);
  lv_async_call(rebuild_async, scr);
}

const char *META = R"J({
  "about": "The screen a new panel shows until it is on Wi-Fi and added to Home Assistant: a code to join its hotspot (with the password), the other ways to set it up, then each step until it is connected.",
  "frame": [480, 480],
  "props": [
    {"name": "state", "type": "select", "options": ["no-wifi", "phone-joined", "allow", "connecting", "failed", "connected", "added", "actions"], "default": "no-wifi"},
    {"name": "way", "type": "select", "options": ["hotspot", "bluetooth", "usb"], "default": "hotspot"},
    {"name": "bluetooth", "type": "bool", "default": true},
    {"name": "back", "type": "bool", "default": false},
    {"name": "ssid", "type": "text", "default": "Facet-1A2B"},
    {"name": "password", "type": "text", "default": "k7mqx3tpa9"},
    {"name": "wifi", "type": "text", "default": "Larkfield"},
    {"name": "device", "type": "text", "default": "Facet panel 1a2b3c"},
    {"name": "ip", "type": "text", "default": "192.168.1.42"}
  ],
  "audit": {"snaps": {"way": "another way is another page of the screen: the body is made again and fades (refill_fade)"}},
  "variants": [
    {"label": "1 no Wi-Fi · hotspot", "props": {}},
    {"label": "1b no Wi-Fi · Bluetooth", "props": {"way": "bluetooth"}},
    {"label": "1c no Wi-Fi · USB", "props": {"way": "usb"}},
    {"label": "1d no Wi-Fi · no Bluetooth (Guition)", "props": {"bluetooth": false}},
    {"label": "2 phone joined", "props": {"state": "phone-joined"}},
    {"label": "3 allow Home Assistant?", "props": {"state": "allow"}},
    {"label": "4 connecting", "props": {"state": "connecting"}},
    {"label": "5 could not connect", "props": {"state": "failed"}},
    {"label": "6 on Wi-Fi, add in Home Assistant", "props": {"state": "connected"}},
    {"label": "7 added", "props": {"state": "added"}},
    {"label": "8 allow actions hint", "props": {"state": "actions"}},
    {"label": "from Settings (Back)", "props": {"state": "added", "back": true}}
  ]
})J";

void build_setup(lv_obj_t *parent, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *scr = new Screen;
  scr->props.set(p);
  scr->ways = {"hotspot"};
  if (p["bluetooth"] | true) scr->ways.push_back("bluetooth");
  scr->ways.push_back("usb");
  if (std::find(scr->ways.begin(), scr->ways.end(), std::string(p["way"] | "hotspot")) == scr->ways.end())
    scr->props["way"] = "hotspot";

  scr->root = box(parent);
  lv_obj_set_user_data(scr->root, scr);
  lv_obj_set_size(scr->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(scr->root, t.bg, 0);
  lv_obj_set_style_bg_opa(scr->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(scr->root, [](lv_event_t *e) {
    auto *scr = (Screen *) lv_event_get_user_data(e);
    lv_async_call_cancel(rebuild_async, scr);
    delete scr;
  }, LV_EVENT_DELETE, scr);

  const View v = view_of(scr->props.as<fw::Props>());
  Doc hd;
  header_props(v, scr->props.as<fw::Props>(), hd);
  scr->header = build("header", scr->root, hd);
  if (scr->header) lv_obj_set_pos(scr->header, fw::lay::inset, fw::lay::inset);

  scr->body = box(scr->root);
  lv_obj_set_pos(scr->body, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(scr->body, fw::lay::content_w, fw::lay::screen_h - fw::lay::top - fw::lay::inset);
  fill_body(scr);

  if (fw::lay::screen_w < 600 && scr->ways.size() > 1) {  // 480: "Other ways" opens this sheet (overlay)
    Doc d;
    sheet_props(scr, false, d);
    scr->sheet = build("bottom_sheet", scr->root, d);
    if (scr->sheet)
      lv_obj_add_event_cb(scr->sheet, [](lv_event_t *e) {  // a pick (the sheet closes itself)
        auto *scr = (Screen *) lv_event_get_user_data(e);
        const int i = (int) (intptr_t) lv_event_get_param(e);
        if (i >= 0 && i < (int) scr->ways.size()) show_way(scr, scr->ways[i].c_str());
      }, LV_EVENT_VALUE_CHANGED, scr);
  }
}
const int reg = fw::add({"setup", "Set up", "screens", META, build_setup});
}  // namespace

// Update path: the header in place (the chip's tone fades, its words change), the body made again with a fade
// (refill_fade: a state is a different page of the same screen); the ways list / sheet follow. Silent.
FW_UPDATE(setup) {
  auto *scr = (Screen *) lv_obj_get_user_data(root);
  if (!scr) return false;
  const bool had_bt = std::find(scr->ways.begin(), scr->ways.end(), "bluetooth") != scr->ways.end();
  if ((p["bluetooth"] | true) != had_bt) return false;  // another board's ways: build again
  scr->props.set(p);
  const View v = view_of(p);
  Doc hd;
  header_props(v, p, hd);
  if (scr->header) fw::update_child(fw::find("header"), scr->header, hd.as<fw::Props>());
  rebuild_body(scr, true);
  if (scr->sheet) {
    Doc d;
    sheet_props(scr, false, d);
    fw::update_child(fw::find("bottom_sheet"), scr->sheet, d.as<fw::Props>());
  }
  return true;
}
