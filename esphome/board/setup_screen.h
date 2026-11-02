#pragma once
// Facet PUBLIC build: the Set up screen on the panel (packages/net_public.yaml). The screen itself is Facet's `setup`
// system screen (renderer/screens/setup.cpp, the same C++ the workbench previews); this file feeds it firmware facts
// and decides when it shows:
//   no-wifi       the setup hotspot is up (no Wi-Fi saved, or the saved one failed for ap_timeout) and no phone on it:
//                 Wi-Fi QR with the per-panel password (setup_ap.h), Bluetooth (Waveshare: esp32_improv) and USB ways
//   phone-joined  a phone is on the hotspot (esp_wifi_ap_get_sta_list): QR to the setup page 192.168.4.1
//   connecting    new Wi-Fi details arrived (Improv serial / BLE or the setup page: the saved SSID changed) and the
//                 panel is not on Wi-Fi yet, up to CONNECT_MS
//   failed        that try ran out (or the hotspot came back): FAILED_MS, then no-wifi again
//   connected     on Wi-Fi, never added to Home Assistant: "Add me in Home Assistant" + name + address
//   added         the first API connection while the screen shows: ADDED_MS, then Home + a toast "Set up: done"
//   link          after added, while the panel has no Home Assistant link of its own (core/device.h ha_link): QR to
//                 its link page http://<address>/link; leaves once linked (a phone sent ha.connect) or on Later
//   actions       Home Assistant refuses this panel's actions (a call with a call_id got no answer in ACTION_MS: HA
//                 answers every allowed call and drops refused ones silently, homeassistant/components/esphome
//                 manager.py); shown once per boot with Back
// "adopted" = an API client has connected once (own preference slot facet_setup_adopted_v1, not a restored entity:
// prefs_schema untouched; a flash with erase clears it). An adopted panel shows the screen again only when the hotspot
// comes up (Wi-Fi failed past ap_timeout: the user must act); a Wi-Fi blip or HA restart does not bring it back.
// Shown on its own LVGL screen (the device runtime keeps running on fw::device::S.scr underneath; the probation chip
// and toasts stay on the top layer), deleted when it goes. Onboarding timeline (Improv speed): each
// step's ms since boot at WARN in the log and as JSON at GET /setup.json.
#include <cstring>
#include <string>
#include <esp_timer.h>
#include <esp_wifi.h>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace facet::setup_screen {

static constexpr uint32_t CONNECT_MS = 45000;  // Improv serial gives up after 30 s; the setup page has no timeout
static constexpr uint32_t FAILED_MS = 20000;
static constexpr uint32_t ADDED_MS = 4000;
static constexpr uint32_t LEAVE_WAIT_MS = 1000;  // longest wait for transitions before leaving the screen
static constexpr uint32_t ACTION_MS = 6000;

struct Saved {
  uint8_t adopted;
};
struct Timeline {  // ms since boot, 0 = not yet
  uint32_t ap_config = 0, ui = 0, ap_up = 0, phone = 0, creds = 0, wifi = 0, api = 0, failed = 0;
};
struct St {
  lv_obj_t *scr = nullptr;  // our LVGL screen while the Set up screen shows
  std::string shown;        // the state on it ("" = not showing)
  std::string way = "hotspot";
  bool adopted = false;
  esphome::ESPPreferenceObject pref;
  std::string sta;          // the saved / connecting Wi-Fi seen on the last tick
  uint32_t connect_since = 0, failed_until = 0, added_until = 0;
  uint32_t leave_since = 0;  // when want() first said "leave" (0 = it does not)
  bool ap_was = false, api_was = false;
  // actions: last unanswered call (ms sent, 0 = none), refused seen, hint dismissed (Back) this boot
  uint32_t action_sent = 0, action_id = 0;
  bool refused = false, hint_done = false;
  bool offer_link = false, link_later = false;  // added while the screen showed: the link step next, until Later
  Timeline tl;
};
inline St S;

inline uint32_t now() { return esphome::millis(); }
inline void mark(uint32_t &slot, const char *what) {
  if (slot) return;
  slot = now() ? now() : 1;
  ESP_LOGW("setup", "timeline: %s at %u ms", what, (unsigned) slot);
}

inline int ap_clients() {
  wifi_sta_list_t l{};
  return esp_wifi_ap_get_sta_list(&l) == ESP_OK ? l.num : 0;
}

inline std::string ip() {
  for (auto &a : esphome::wifi::global_wifi_component->get_ip_addresses()) {
    if (!a.is_set() || !a.is_ip4()) continue;
    char b[esphome::network::IP_ADDRESS_BUFFER_SIZE];
    return a.str_to(b);
  }
  return "";
}

// ---------------------------------------------------------------- refused actions
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
// the Facet runtime's action sink (fw::ha::on_act), as fw_device.h ha_action but with a call_id so Home Assistant
// answers: no answer in ACTION_MS = refused ("Allow the device to perform Home Assistant actions" is off)
inline void act(const char *service, const char *entity, const char *data_json) {
  auto *api = esphome::api::global_api_server;
  if (!api || !api->is_connected()) return;
  std::vector<std::pair<std::string, std::string>> kv{{"entity_id", entity}};
  ArduinoJson::JsonDocument d;
  if (data_json && *data_json && !deserializeJson(d, data_json))
    for (ArduinoJson::JsonPairConst p : d.as<ArduinoJson::JsonObjectConst>()) {
      std::string v;
      if (p.value().is<const char *>()) v = p.value().as<const char *>();
      else serializeJson(p.value(), v);
      kv.push_back({p.key().c_str(), v});
    }
  const std::string svc = service;
  esphome::api::HomeassistantActionRequest r;
  r.service = esphome::StringRef(svc);
  r.data.init(kv.size());
  for (auto &x : kv) {
    auto &m = r.data.emplace_back();
    m.key = esphome::StringRef(x.first);
    m.value = esphome::StringRef(x.second);
  }
  if (S.refused) {  // known refused this session: no call_id (an unanswered one stays registered in the API server)
    api->send_homeassistant_action(r);
    return;
  }
  static uint32_t next = 0xFAC00000u;
  const uint32_t id = ++next;
  r.call_id = id;
  api->register_action_response_callback(id, [id](const esphome::api::ActionResponse &res) {
    if (S.action_id == id) S.action_sent = 0;  // answered (success or HA's own error): actions are allowed
    S.refused = false;
    (void) res;
  });
  if (!S.action_sent) S.action_sent = now() ? now() : 1, S.action_id = id;
  api->send_homeassistant_action(r);
  ESP_LOGI("facet_wb", "HA action %s %s %s (call %u)", service, entity, data_json ? data_json : "", (unsigned) id);
}
#endif

// ---------------------------------------------------------------- the screen
inline const char *want() {
  auto *w = esphome::wifi::global_wifi_component;
  const uint32_t t = now();
  const bool conn = w->is_connected(), ap = w->is_ap_active();
  bool api_on = false;
#ifdef USE_API
  api_on = esphome::api::global_api_server && esphome::api::global_api_server->is_connected();
#endif
  std::string sta;
  if (w->has_sta()) {
    auto s = w->get_sta().get_ssid();
    sta.assign(s.c_str(), s.size());
  }
  if (sta != S.sta) {  // new Wi-Fi details (Improv sets the STA before it connects; the setup page saves it)
    S.sta = sta;
    if (!sta.empty() && !conn) {
      S.connect_since = t ? t : 1, S.failed_until = 0;
      S.tl.creds = 0, S.tl.wifi = 0, S.tl.failed = 0;
      mark(S.tl.creds, "Wi-Fi details received");
    }
  }
  if (ap && !S.ap_was) mark(S.tl.ap_up, "hotspot up");
  S.ap_was = ap;
  if (conn) {
    if (S.connect_since) S.connect_since = 0;
    mark(S.tl.wifi, "on Wi-Fi");
  }
  if (api_on != S.api_was) {  // a new API session (e.g. HA reloaded the entry after "Allow actions"): ask again
    S.api_was = api_on;
    S.refused = false, S.hint_done = false, S.action_sent = 0;
  }
  if (api_on && !S.adopted) {
    S.adopted = true;
    Saved s{1};
    S.pref.save(&s);
    esphome::global_preferences->sync();
    mark(S.tl.api, "Home Assistant connected (adopted)");
    if (!S.shown.empty()) S.added_until = t + ADDED_MS, S.offer_link = true;
  }
  if (S.action_sent && t - S.action_sent > ACTION_MS) {
    S.action_sent = 0;
    if (!S.refused) ESP_LOGW("setup", "Home Assistant did not answer an action in %u ms: actions not allowed?", (unsigned) ACTION_MS);
    S.refused = true;
  }

  if (S.added_until) {
    if ((int32_t) (S.added_until - t) > 0) return "added";
    S.added_until = 0;
  }
  if (S.refused && !S.hint_done && api_on) return "actions";
  if (conn && S.offer_link && !S.link_later && !fw::dev::ha_link::ready()) return "link";
  if (conn) return S.adopted ? "" : "connected";
  if (S.connect_since) {
    if (t - S.connect_since < CONNECT_MS) return "connecting";
    S.connect_since = 0;
    S.failed_until = t + FAILED_MS;
    mark(S.tl.failed, "could not join");
  }
  if (S.failed_until && (int32_t) (S.failed_until - t) > 0) return (ap || !S.adopted) ? "failed" : "";
  S.failed_until = 0;
  if (ap) {
    if (ap_clients() > 0) {
      mark(S.tl.phone, "phone joined the hotspot");
      return "phone-joined";
    }
    return "no-wifi";
  }
  return "";  // no hotspot, not connected (booting, a blip): the device's own screens
}

inline void props(const char *state, std::string &out) {
  ArduinoJson::JsonDocument p;
  p["state"] = state;
  p["way"] = S.way.c_str();
#ifdef USE_ESP32_IMPROV
  p["bluetooth"] = true;
#else
  p["bluetooth"] = false;
#endif
  p["back"] = !strcmp(state, "actions");
  p["ssid"] = setup_ap::ssid().c_str();
  p["password"] = setup_ap::password().c_str();
  p["wifi"] = S.sta.c_str();
  auto fn = esphome::App.get_friendly_name();
  p["device"] = std::string(fn.c_str(), fn.size());
  p["ip"] = ip();
  p["link"] = "http://" + ip() + "/link";
  serializeJson(p, out);
}

inline void on_event(lv_obj_t *, const char *name, const char *detail, void *) {
  if (!strcmp(name, "way") && detail) S.way = detail;
  else if (!strcmp(name, "back") && S.shown == "actions") S.hint_done = true;  // next tick leaves
  else if (!strcmp(name, "setup") && detail && !strcmp(detail, "not-now") && S.shown == "link") S.link_later = true;  // Later
}

inline void leave() {
  if (!S.scr) return;
  ESP_LOGW("setup", "Set up screen: leaving (%s)", S.shown.c_str());
  const bool done = S.shown == "added" || S.shown == "link";
  lv_screen_load_anim(fw::device::S.scr, LV_SCREEN_LOAD_ANIM_FADE_IN, 300, 0, true);  // deletes our screen after
  S.scr = nullptr;
  S.shown.clear();
  S.leave_since = 0;
  if (done) {
    ArduinoJson::JsonDocument d;
    d["tone"] = "ok";
    d["title"] = "Set up: done";
    d["body"] = "This panel is in Home Assistant now.";
    fw::dev::toast(d.as<fw::Props>());
  }
}

inline void show(const char *state) {
  std::string pj;
  props(state, pj);
  const fw::Theme *look = fw::dev::look(fw::theme(fw::device::S.light, fw::device::S.accent));
  const bool fresh = !S.scr;
  if (fresh) S.scr = lv_obj_create(nullptr);
  const char *e = fresh ? "rebuild" : fw::update(S.scr, "setup", pj.c_str(), look->light, look->accent_set);
  if (*e) {
    e = fw::render(S.scr, "setup", pj.c_str(), look->light, look->accent_set);
    if (*e) {
      ESP_LOGE("setup", "Set up screen: %s", e);
      if (fresh) lv_obj_delete(S.scr), S.scr = nullptr;
      return;
    }
    if (lv_obj_t *root = lv_obj_get_child(S.scr, 0)) fw::listen(root, on_event);
  }
  if (fresh) {
    lv_screen_load_anim(S.scr, LV_SCREEN_LOAD_ANIM_FADE_IN, 300, 0, false);
    mark(S.tl.ui, "Set up screen shown");
  }
  ESP_LOGW("setup", "Set up screen: %s", state);
  S.shown = state;
}

inline void tick(lv_timer_t *) {
  const char *st = want();
  if (!*st) {
    // leave once the running transitions have ended, but never wait longer than LEAVE_WAIT_MS: a looping animation
    // (the "connecting" chip pulse, a mascot on Home) keeps lv_anim_count_running() above 0 forever, which kept an
    // adopted panel on "connecting" after Wi-Fi + Home Assistant were back (seen on a public build).
    // Deleting the screen with its loops running is fine: LVGL drops the animations of deleted objects.
    if (S.scr) {
      const uint32_t t = now();
      if (!S.leave_since) S.leave_since = t ? t : 1;
      if (lv_anim_count_running() == 0 || t - S.leave_since >= LEAVE_WAIT_MS) leave();
    }
    return;
  }
  S.leave_since = 0;
  if (S.shown == st) return;
  show(st);
}

// ---------------------------------------------------------------- GET /setup.json (onboarding timeline)
class Handler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *req) const override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return req->url_to(buf) == "/setup.json";
  }
  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *req) override {
    ArduinoJson::JsonDocument d;
    d["shown"] = S.shown.c_str();  // (read without the LVGL lock: a snapshot, may be a tick old)
    d["adopted"] = S.adopted;
    d["actions_refused"] = S.refused;
    ArduinoJson::JsonObject t = d["timeline_ms"].to<ArduinoJson::JsonObject>();
    const Timeline tl = S.tl;
    t["hotspot_config"] = tl.ap_config, t["setup_screen"] = tl.ui, t["hotspot_up"] = tl.ap_up, t["phone"] = tl.phone;
    t["wifi_details"] = tl.creds, t["on_wifi"] = tl.wifi, t["failed"] = tl.failed, t["home_assistant"] = tl.api;
    d["uptime_ms"] = now();
    std::string s;
    serializeJson(d, s);
    auto *r = req->beginResponse(200, "application/json", s);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  }
};

// on_boot after fw::device::setup() (net_public.yaml)
inline void start() {
  S.pref = esphome::global_preferences->make_preference<Saved>(esphome::fnv1_hash("facet_setup_adopted_v1"));
  Saved s{};
  S.adopted = S.pref.load(&s) && s.adopted == 1;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  fw::ha::on_act(act);
#endif
  if (auto *ws = esphome::web_server_base::global_web_server_base) ws->add_handler(new Handler());
  ESP_LOGW("setup", "Set up: adopted %d, Wi-Fi saved %d, boot to UI %u ms", (int) S.adopted,
           (int) esphome::wifi::global_wifi_component->has_sta(), (unsigned) now());
  lv_timer_create(tick, 500, nullptr);
  tick(nullptr);
}
// on_boot 600 (setup_ap applied): the first timeline point
inline void note_boot() { S.tl.ap_config = now() ? now() : 1; }

}  // namespace facet::setup_screen
