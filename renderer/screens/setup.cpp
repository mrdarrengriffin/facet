// Set up: the system screen a panel shows until it is on Wi-Fi and added to Home Assistant (esphome/packages/net_public.yaml,
// esphome/board/setup_screen.h). Built in, not in the deck, no Back while the panel is not set up (prop back = reached
// from Settings > Wi-Fi and Home Assistant later). State comes from firmware facts only (Wi-Fi, hotspot clients, Improv,
// API, an "adopted" pref); here every state is a prop so the workbench shows each one. Plain words on screen: no
// "Improv" or "captive portal". On the contract: Screen > Page (fx::page) > Header + list Body (C1-C3), real
// components only, one composition at every size (C6: nothing branches on the screen):
//   header   the Header (label "Set up", the Status chip with the state's tone + words, the live clock only once the
//            panel is on Wi-Fi: before that it has no time)
//   body     with a code: a Split (art_w qr): the QR block and the device name (Text label, 2 lines) in the art column,
//            the words in the other; without a code: a Column, the words on top, the device name in its foot
//   words    Headline (Text headline), what to do (Text body dim), each the lines its state's copy takes in the
//            narrowest column (1 to 4), the facts as Stats in a Grid
//            (cols = the facts, aspect row), the buttons in a Grid (Allow / Not now), the ways as Picker rows (a tap
//            shows that way); space 5 (= section-gap) above each block after the first
//   foot     the device name (+ its address once on Wi-Fi), Text label dim: under the code, the name (one line, "...")
//            and the address on a line of its own (whole); without a code, "name · address" on one line
// States (prop state):
//   no-wifi       warn "No Wi-Fi"      hotspot: Wi-Fi QR with the per-panel password (WPA), the network + password
//                                      as facts; or the way picked: Bluetooth (HA app) / USB cable (docs Install)
//   phone-joined  info "Phone joined"  QR to the setup page (http://192.168.4.1) if it did not open by itself
//   allow         info "Asking"        Home Assistant asks to send Wi-Fi details over Bluetooth: Allow / Not now
//   connecting    working (pulse)      the Wi-Fi name, "Joining your Wi-Fi…"
//   failed        error                "Could not join <wifi>", what to check, the hotspot comes back by itself
//   connected     ok "On Wi-Fi"        "Add me in Home Assistant": QR to HA's integrations page, the name as a fact,
//                                      the address in the foot
//   added         ok "Connected"       "Connected to Home Assistant" (the panel then opens Home with a toast)
//   link          ok "Connected"       the panel has no Home Assistant link of its own yet: QR to its link page
//                                      (http://<its address>/link), Later (ghost: "not-now") = Home without it
//   actions       warn "Actions off"   Home Assistant refuses this panel's actions: where to turn them on
// Events: "setup" detail allow / not-now (the Allow buttons; Later on link), "way" detail hotspot|bluetooth|usb (a way picked).
// Update path: the page made again with a fade (refill_fade: a state is another page of the same screen), silent.
#include <algorithm>
#include <functional>
#include <string>
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
using Doc = ArduinoJson::JsonDocument;

lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
lv_obj_t *text(lv_obj_t *parent, const std::string &s, const char *style, bool dim, int lines) {
  Doc d;
  d["text"] = s.c_str(), d["style"] = style, d["tone"] = dim ? "dim" : "default", d["lines"] = lines;
  return build("text", parent, d);
}
void gap(lv_obj_t *o) { if (o) fx::use(o, "above:5"); }  // a block after the first: space 5 above it (= layout section-gap)

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
  int tl = 2, xl = 2;                                 // the lines the title / the text take (Text lines: the copy, at the narrowest column)
  std::vector<std::pair<std::string, std::string>> facts;  // caps label, value
  bool ways = false;                                  // the ways as Picker rows
  std::string primary, secondary;                     // buttons (allow)
  bool ip = false;                                    // the foot shows the address
};

struct Screen;
struct WayTap { Screen *scr; int i; };  // a way row's tap: which way (the row's own user data is its component's)
struct Screen {
  lv_obj_t *root = nullptr;
  Doc props;              // the current props (a way picked on the panel changes "way" here)
  std::vector<std::string> ways;  // the ways this panel has
  WayTap taps[3];
};

View view_of(fw::Props p) {
  const std::string st = p["state"] | "no-wifi", way = p["way"] | "hotspot";
  const std::string ssid = p["ssid"] | "Facet-1A2B", pw = p["password"] | "k7mqx3tpa9", wifi = p["wifi"] | "Larkfield";
  const std::string dev = p["device"] | "Facet panel 1a2b3c", ip = p["ip"] | "192.168.1.42", link = p["link"] | "http://192.168.1.42/link";
  View v;
  if (st == "phone-joined") {
    v.chip_tone = "info", v.chip_text = "Phone joined";
    v.qr = "http://192.168.4.1", v.qr_caption = "open setup";
    v.title = "Pick your Wi-Fi", v.tl = 1, v.xl = 3;
    v.text = "Your phone is on the panel's hotspot. If the setup page did not open, scan this code.";
    v.facts = {{"address", "192.168.4.1"}};
  } else if (st == "allow") {
    v.chip_tone = "info", v.chip_text = "Asking";
    v.title = "Home Assistant wants to set up this panel";
    v.text = "Allow it to send your Wi-Fi details over Bluetooth.";
    v.primary = "Allow", v.secondary = "Not now";
  } else if (st == "connecting") {
    v.chip_tone = "working", v.chip_text = "Connecting", v.pulse = true;
    v.title = wifi, v.tl = 1, v.xl = 1;
    v.text = "Joining your Wi-Fi…";
  } else if (st == "failed") {
    v.chip_tone = "error", v.chip_text = "Not connected";
    v.title = "Could not join " + wifi;
    v.text = "Check the password and that the panel is in range. The hotspot comes back in a minute.";
  } else if (st == "connected") {
    v.chip_tone = "ok", v.chip_text = "On Wi-Fi", v.clock = true;
    v.qr = HA_ADD, v.qr_caption = "open in HA";
    v.title = "Add me in Home Assistant", v.xl = 3;
    v.text = "Settings > Devices & services shows a new ESPHome device. Press Add.";
    v.facts = {{"name", dev}};  // the address: whole in the foot under the code (two facts side by side cut both)
    v.ip = true;
  } else if (st == "added") {
    v.chip_tone = "ok", v.chip_text = "Connected", v.clock = true;
    v.title = "Connected to Home Assistant", v.xl = 1;
    v.text = "All set. Home opens in a moment.";
    v.ip = true;
  } else if (st == "link") {
    v.chip_tone = "ok", v.chip_text = "Connected", v.clock = true;
    v.qr = link, v.qr_caption = "link page";
    v.title = "Link the screens to Home Assistant", v.xl = 4;
    v.text = "Scan to give this panel its own Home Assistant token. Or later, in Settings > Home Assistant.";
    v.secondary = "Later";
    v.ip = true;
  } else if (st == "actions") {
    v.chip_tone = "warn", v.chip_text = "Actions off", v.clock = true;
    v.title = "Let this panel control your home", v.xl = 4;
    v.text = "Home Assistant: Settings > Devices & services > ESPHome > " + dev +
             " > Configure. Turn on \"Allow the device to perform Home Assistant actions\".";
    v.ip = true;
  } else {  // no-wifi
    v.chip_tone = "warn", v.chip_text = "No Wi-Fi";
    v.ways = true;
    if (way == "bluetooth") {
      v.qr = HA_ADD, v.qr_caption = "open in HA";
      v.title = "Add it with Bluetooth", v.xl = 3;
      v.text = "In the Home Assistant app, Settings > Devices & services shows this panel. Press Add.";
    } else if (way == "usb") {
      v.qr = DOCS_INSTALL, v.qr_caption = "install page";
      v.title = "Set it up with USB", v.xl = 4;
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

// a Grid of n cells in one row (aspect row): the facts, the buttons
lv_obj_t *row_grid(lv_obj_t *parent, int n) {
  Doc g;
  g["cols"] = n, g["aspect"] = "row", g["gap"] = "group", g["_cells"] = n;
  return build("grid", parent, g);
}
void facts(lv_obj_t *parent, const View &v) {
  if (v.facts.empty()) return;
  lv_obj_t *g = row_grid(parent, (int) v.facts.size());
  gap(g);
  for (size_t i = 0; g && i < v.facts.size(); i++) {
    Doc sp;
    sp["label"] = v.facts[i].first.c_str(), sp["value"] = v.facts[i].second.c_str(), sp["tone"] = "text", sp["fill"] = true;
    if (lv_obj_t *c = fw::spec::slot_of(g, (int) i)) build("stat", c, sp);
  }
}
void buttons(lv_obj_t *parent, const View &v) {
  if (v.primary.empty() && v.secondary.empty()) return;
  const bool one = v.primary.empty();  // the ghost alone (Later): one cell
  lv_obj_t *g = row_grid(parent, one ? 1 : 2);
  gap(g);
  struct B { const std::string &label; const char *kind, *what; } bs[] = {{v.primary, "primary", "allow"}, {v.secondary, "ghost", "not-now"}};
  for (int i = one ? 1 : 0, k = 0; g && i < 2; i++, k++) {
    lv_obj_t *c = fw::spec::slot_of(g, k);
    if (!c || bs[i].label.empty()) continue;
    Doc bp;
    bp["kind"] = bs[i].kind, bp["label"] = bs[i].label.c_str();
    if (lv_obj_t *o = build("button", c, bp))
      lv_obj_add_event_cb(o, [](lv_event_t *e) {  // after the Button's own tap (event + sound)
        fw::emit((lv_obj_t *) lv_event_get_current_target(e), "setup", (const char *) lv_event_get_user_data(e));
      }, LV_EVENT_CLICKED, (void *) bs[i].what);
  }
}
// the ways as Picker rows (a tap shows that way)
void way_rows(Screen *scr, lv_obj_t *parent, const std::string &cur) {
  bool first = true;
  for (size_t i = 0; i < scr->ways.size() && i < 3; i++) {
    Doc rp;
    rp["icon"] = "none", rp["name"] = way_name(scr->ways[i]), rp["status"] = way_hint(scr->ways[i]), rp["selected"] = scr->ways[i] == cur;
    lv_obj_t *r = build("picker_row", parent, rp);
    if (!r) continue;
    if (first) gap(r), first = false;
    scr->taps[i] = {scr, (int) i};
    lv_obj_add_event_cb(r, [](lv_event_t *e) {
      if (lv_obj_has_state((lv_obj_t *) lv_event_get_current_target(e), fw::st::OFF)) return;
      auto *w = (WayTap *) lv_event_get_user_data(e);
      if (w->i < (int) w->scr->ways.size()) show_way(w->scr, w->scr->ways[w->i].c_str());
    }, LV_EVENT_CLICKED, &scr->taps[i]);
  }
}

void fill(Screen *scr) {
  fw::Props p = scr->props.as<fw::Props>();
  const View v = view_of(p);
  Doc hd;
  header_props(v, p, hd);
  const fx::Page fp = fx::page(scr->root, "list");  // Screen > Page > Header + list Body (C1-C3)
  build("header", fp.head, hd);
  std::string foot = p["device"] | "Facet panel 1a2b3c";
  if (v.ip) foot += std::string(" \xC2\xB7 ") + (p["ip"] | "192.168.1.42");
  lv_obj_t *words = nullptr;
  if (!v.qr.empty()) {  // a code: the QR block and the name in the art column, the words beside it
    Doc sd;
    sd["art_w"] = "qr";
    lv_obj_t *sp = build("split", fp.body, sd);
    lv_obj_t *art = sp ? fw::spec::slot_of(sp, 0) : nullptr;
    words = sp ? fw::spec::slot_of(sp, 2) : nullptr;
    if (!art || !words) return;
    Doc qp;
    qp["text"] = v.qr.c_str(), qp["caption"] = v.qr_caption.c_str();
    build("qr_block", art, qp);
    // the name on one line (cut with "..." when long), the address whole on its own under it
    gap(text(art, p["device"] | "Facet panel 1a2b3c", "label", true, 1));
    if (v.ip) text(art, p["ip"] | "192.168.1.42", "label", true, 1);
  } else {  // no code: the words on top, the name at the foot
    Doc cd;
    lv_obj_t *col = build("column", fp.body, cd);
    words = col ? fw::spec::slot_of(col, 0) : nullptr;
    lv_obj_t *ft = col ? fw::spec::slot_of(col, 1) : nullptr;
    if (!words || !ft) return;
    text(ft, foot, "label", true, 1);
  }
  text(words, v.title, "headline", false, v.tl);
  gap(text(words, v.text, "body", true, v.xl));
  facts(words, v);
  buttons(words, v);
  if (v.ways && scr->ways.size() > 1) way_rows(scr, words, p["way"] | "hotspot");
}

void rebuild(Screen *scr, bool animate) {
  fw::st::refill_fade(scr->root, [scr] {
    fw::refill(scr->root);
    fx::use(scr->root, "fill");
    fill(scr);
  }, animate);
}
void rebuild_async(void *u) { rebuild((Screen *) u, true); }

// a way picked on the panel: remember it, report it, show it (after the tap returns: the row that was tapped is made
// again)
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
    {"name": "state", "about": "Which step of setting up the panel is on: no Wi-Fi yet, phone joined, Home Assistant asking, connecting, failed, connected, added, linking the screens to Home Assistant, or the hint to allow actions. The panel sets this itself; pick one here to preview a step.", "type": "select", "options": ["no-wifi", "phone-joined", "allow", "connecting", "failed", "connected", "added", "link", "actions"], "default": "no-wifi"},
    {"name": "way", "about": "How the panel is being set up: by joining its own hotspot from a phone, through the Home Assistant app over Bluetooth, or over a USB cable from a computer.", "type": "select", "options": ["hotspot", "bluetooth", "usb"], "default": "hotspot"},
    {"name": "bluetooth", "about": "Whether this panel can be set up over Bluetooth. Off on boards without Bluetooth, which hides that option.", "type": "bool", "default": true},
    {"name": "back", "about": "Shows a Back button. On when the screen is opened again from Settings, off on a brand-new panel.", "type": "bool", "default": false},
    {"name": "ssid", "about": "The name of the panel's own hotspot, shown in the QR code and under Wi-Fi.", "type": "text", "default": "Facet-1A2B"},
    {"name": "password", "about": "The password for the panel's hotspot. Each panel makes its own.", "type": "text", "default": "k7mqx3tpa9"},
    {"name": "wifi", "about": "The name of your home Wi-Fi network the panel is joining, shown while it connects.", "type": "text", "default": "Larkfield"},
    {"name": "device", "about": "The name the panel shows for itself and in Home Assistant.", "type": "text", "default": "Facet panel 1a2b3c"},
    {"name": "ip", "about": "The panel's address on your network, shown once it is connected.", "type": "text", "default": "192.168.1.42"},
    {"name": "link", "about": "The panel's own link page, where a phone sets its Home Assistant address and token.", "type": "text", "default": "http://192.168.1.42/link"}
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
    {"label": "7b link Home Assistant", "props": {"state": "link"}},
    {"label": "8 allow actions hint", "props": {"state": "actions"}},
    {"label": "from Settings (Back)", "props": {"state": "added", "back": true}}
  ]
})J";

void build_setup(lv_obj_t *parent, fw::Props p) {
  auto *scr = new Screen;
  scr->props.set(p);
  scr->ways = {"hotspot"};
  if (p["bluetooth"] | true) scr->ways.push_back("bluetooth");
  scr->ways.push_back("usb");
  if (std::find(scr->ways.begin(), scr->ways.end(), std::string(p["way"] | "hotspot")) == scr->ways.end())
    scr->props["way"] = "hotspot";
  scr->root = fx::box(parent, "fill");
  lv_obj_set_user_data(scr->root, scr);
  lv_obj_add_event_cb(scr->root, [](lv_event_t *e) {
    auto *scr = (Screen *) lv_event_get_user_data(e);
    lv_async_call_cancel(rebuild_async, scr);
    delete scr;
  }, LV_EVENT_DELETE, scr);
  fill(scr);
}
const int reg = fw::add({"setup", "Set up", "screens", META, build_setup});
}  // namespace

// Update path: the page made again with a fade (refill_fade: a state is another page of the same screen). Silent.
FW_UPDATE(setup) {
  auto *scr = (Screen *) lv_obj_get_user_data(root);
  if (!scr) return false;
  const bool had_bt = std::find(scr->ways.begin(), scr->ways.end(), "bluetooth") != scr->ways.end();
  if ((p["bluetooth"] | true) != had_bt) return false;  // another board's ways: build again
  scr->props.set(p);
  rebuild(scr, true);
  return true;
}
