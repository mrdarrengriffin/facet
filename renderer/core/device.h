// Device runtime (Darren 2026-10-22): the panel as one live thing, the same C++ on the panels and in the WASM
// workbench: a screen stack, Facet's screen transition and the taps that move between screens. Old Facet:
// @facet/ui device.ts createDevice() (go(id), back, Home's settings button, the Content rise on every cut).
//   mount      dev::mount(parent, "home") makes a full-screen root (lay::screen_w x screen_h) and shows that screen.
//              The workbench's "Device" page (screens/device.cpp, /wasm/screens/device) and the panel firmware
//              (POST /fw/show {"id":"device"}, firmware/fw_device.h) both just render that component.
//   navigate   any fw::emit from inside the device (fw::listen on the root): "navigate" <id> (the screen chrome's
//              settings button, Home's cards) = go(id); "back" (the Header's Back button) = back. The tap's own sound
//              already played (states / sound hooks); the runtime adds none. The cut runs on the next LVGL tick (a
//              one-shot timer), never inside the tapped object's own event (it is deleted with its screen).
//   specs      an id that is no registered component but a spec screen (core/spec.h: factory or sent) is built from
//              its spec; "light?{...}" = the screen's parameters (one Light spec for every light)
//   stack      go(id) pushes id (an id already on the stack pops back to it: no loops); back pops (at the root: no-op);
//              home() clears to the root. Ids = registered components: a "screens" component is built as the whole
//              screen (props {}); any other (the placeholder Home lists the foundation pages) is shown as a page: the
//              Header (back + its name, the screen chrome) and the component scrolling under it from lay::top.
//   transition Motion pattern `screen` (the Facet Content rise, fw::rise): the header first, then the content blocks in
//              rows by their top, from below going forward, from above going back / Home; reduced motion = instant.
//              Hidden and floating children (a closed Bottom sheet) are left alone.
//   settings   the panel's personalisation values (tokens/settings.json, fw::set::), ONE set() path for the Settings
//              screen, Home Assistant (the ESPHome config entities: firmware/settings_yaml.mjs + fw_device.h) and the
//              workbench, so the three never disagree. set() checks the value (a select takes its option id or label,
//              a number is clamped + snapped to its step, a switch takes true/false/on/off), keeps it, applies it to
//              every mounted device, tells the observers (the firmware's entities + NVS) and emits "setting" <id>=<value>
//              (from the screen's control, else from each device root). An unchanged value does nothing (no echo loops).
//              Applied now: theme + accent restyle the device live (the current screen is built again in place with
//              the new theme after its running animations end, scroll kept, no rise), transition (instant = no rise),
//              any change from outside the screen moves the Settings screen's controls (its update path). Theme auto =
//              light while the host says it is day (set_daylight: the panel's sun.sun), else the stage's theme (the
//              workbench theme, the panel's /fw/show theme). Accent: the stage's set until one is set (workbench).
//              The rest (sounds, sound pack, volume, idle, units, toasts) is kept and reported for their consumers;
//              the workbench host switches its sound pack on the "setting" event.
//   toasts     every mounted device has a Toast layer over its screens (core/toasts.h, on the display's top layer).
//              toast({tone, title, body, icon?, action?, life?}) pushes one to every device (position = Settings >
//              Toasts). From Home Assistant: the host puts the toast JSON on the pseudo pair TOAST_PAIR ("facet.toast",
//              state) through ha::put (the panel: its ESPHome "toast" action; the workbench: fw_ha_put); each put
//              must differ (include an id or the time), an unchanged value is no new toast.
// TODO (with the panel host): idle -> clock, as createDevice; Text size (a type-scale rebuild).
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "fw.h"

namespace fw {
namespace dev {
constexpr const char *TOAST_PAIR = "facet.toast";  // the HA store pair a host puts a toast's JSON on
void toast(Props t);  // a toast over every mounted device's screen (see "toasts" above)
lv_obj_t *mount(lv_obj_t *parent, const char *start = "home");  // the device root (its screen is its one child)
void go(lv_obj_t *in, const char *id);  // in = the root or anything inside it; queued for the next tick
void back(lv_obj_t *in);
void home(lv_obj_t *in);
void reload(const char *id);
void state(ArduinoJson::JsonArray out);  // every mounted device: {stack, builds, objects} (spec hello diagnostics)  // a spec changed (core/spec.h apply): devices showing it (or every device: "deck") build it again
const char *current(lv_obj_t *in);  // the id on top of the stack ("" = not a device)

// settings (above)
enum From : uint8_t { SCREEN, HA, HOST, BOOT };  // who set it: the Settings screen, Home Assistant, a host / API, restore
const char *setting(const char *id);  // the value now (option id, "true" / "false", the number), nullptr = no such id
// "" = applied, "same" = already that value, else the error; obj = the control that changed it (emits from there)
const char *set(const char *id, const char *value, From from, lv_obj_t *obj = nullptr);
const char *set(const char *id, double value, From from);  // numbers (HA number entities)
const char *option_label(const char *id, const char *value);  // a select's label for an option id ("Dark"), else value
using Observer = void (*)(const char *id, const char *value, From from);
void observe(Observer cb);  // every applied change (firmware: publish to the entity, which keeps it in NVS)
// backup: {"facet":"settings","version":1,"values":{id: value, ...}} (no secrets: there are none in settings)
void settings_json(std::string &out);
std::string restore(const char *json, From from);  // a backup's values through set(); "" = ok, else the first error
void set_daylight(int day);                          // the sun for theme auto: 1 day, 0 night, -1 unknown
const Theme *look(const Theme *stage);               // the theme the device shows over a stage theme
const Theme *theme_at(lv_obj_t *o);                  // o is a device root: the look it is built in, else nullptr
void on_look(void (*cb)());
void refresh_look();  // the look's theme changed under the same setting (a new sent theme): select it again + rebuild  // the look may have changed (theme / accent set, the sun): hosts redraw what they show

// hardware button (a panel's physical key, e.g. the Waveshare's BOOT key; Darren 2026-10-24): the "Contextual" action.
// A part that has something to do with the key claims it while it lives: claim_button(obj, level, fn, user). A press
// goes to the claims whose object is shown (on the active screen or the top layer, not hidden), highest level first,
// then newest first; the first fn that returns true took it. A claim ends with its object (LV_EVENT_DELETE).
// Levels: BUTTON_ALERT (a toast, a ringing timer: dismiss), BUTTON_SHEET (an open bottom sheet: close), BUTTON_SCREEN (the screen's own
// action, e.g. Media play / pause). press = "single" | "double" | "long"; parts act on "single" unless they say so.
enum ButtonLevel : uint8_t { BUTTON_SCREEN = 0, BUTTON_SHEET = 1, BUTTON_ALERT = 2 };
using ButtonFn = bool (*)(void *user, const char *press);
void claim_button(lv_obj_t *obj, ButtonLevel level, ButtonFn fn, void *user);
const char *button(const char *press);  // the host's key press: "" = nothing took it, else the taker's object kind ("alert" ...)

// the panel's timers (Darren 2026-10-28, notes timer.md; core/timers.cpp): a list of up to TIMERS_MAX, kept here so
// they run whatever screen shows. Each {id, name, total s, ends (lv_tick ms), state running | done}. A name is unique
// on the panel: a preset's is its length ("5 min", core length_name), a second "5 min" becomes "5 min 2"; HA / voice
// name their own ("Pasta"). Running out = state done + the timer-done sound ONCE (whatever screen shows), until it is
// stopped (stop / cancel remove it). Order = all(): soonest first (running by time left, then the done ones).
// The shown one (the Timer page's ring) = the selected one while it exists, else the soonest running, else the first
// done. A 5th start is refused: start() returns the reason ("" = started) and the panel shows it as a warn toast.
// extend on a done timer starts it again with s left (+1 min after it rang). Hosts listen with observe(): one call
// per change (start, extend, cancel, stop, done, select), never per second (the panel publishes its HA entities).
// Data source: the reserved entity TIMER_ENTITY ("panel.timer", core/spec.h "local sources") in the HA store, filled
// by the list itself (never by Home Assistant): binds read it like an entity; values that move each second are put
// once a second only while something bound to it is built. Legacy: timer() = the soonest running as the old one-timer
// struct (state 0 idle, 1 running, 2 done; ends = wall-clock seconds) for the Home header's time left + Timer view.
// the Calendar feed's own state (core/calendar_source.cpp): calendar.control; "" = done, else why not
namespace calendar_source { const char *control(const std::string &v); }
// the Media feed (core/media_source.cpp): media.control on panel.media; "" = done, else why not; svc = a media_player
// service to call on ent with data ("" = none)
namespace media_source { const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data); }
// the Covers feed (core/covers_source.cpp): cover.control on panel.covers; "" = done, else why not; svc = a cover
// service to call on ent with data ("" = none)
namespace covers_source { const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data); }
// the Vacuum feed (core/vacuum_source.cpp): vacuum.control on panel.vacuum; "" = done, else why not; svc = a vacuum /
// select service to call on ent with data ("" = none: the sample robot does it)
namespace vacuum_source { const char *control(const std::string &v, const std::string &rooms, std::string *svc, std::string *ent, std::string *data); }
// the Flights feed's selection (core/flights_source.cpp): flights.select; true = the lead again (open its details)
namespace flights_source { bool select(const std::string &d); }
constexpr int TIMERS_MAX = 4;
constexpr const char *TIMER_ENTITY = "panel.timer";
struct TimerItem { int id = 0; std::string name; int total = 0; uint32_t ends = 0; int state = 1; bool named = false; };  // named = given a name (HA / voice); else it shows as its length  // state 1 running, 2 done
namespace timers {
enum Change : uint8_t { START, EXTEND, CANCEL, STOP, DONE, SELECT };
const char *start(const char *name, int seconds, int *id_out = nullptr);  // "" = started, else why not (full, bad length)
const char *extend(int id, int seconds);
const char *cancel(int id);  // a running or a done one: removed
const char *stop(int id);    // a done one: removed (a running one: "not done")
const char *select(int id);  // the shown one
// picking a length for a new timer (the page's + New): the panel.timer state reads "picking" until a start or back
void picking(bool on);
int find(const char *name_or_id);  // by name (any case), else by id ("3"); 0 = none
int left(const TimerItem &t);       // seconds left, rounded up (0:00 only when it ran out); done = 0
long long ends_at(const TimerItem &t);  // wall-clock end (time()); 0 = done / the clock is not set
std::vector<TimerItem> all();       // soonest first
const TimerItem *get(int id);
const TimerItem *shown();
int running();
std::string length_name(int seconds);  // "5 min", "1 h 30 min", "45 s"
std::string countdown(int seconds);    // "4:32", "1:02:05"
void list_json(std::string &out);      // HA's Timers list: [[name, ends epoch, total s, "running" | "done"], ...]
using Observer = void (*)(Change what, const TimerItem &t);
void observe(Observer cb);
void reset();  // tests: every timer gone, no observer call
}  // namespace timers
struct Timer { int state = 0; long long ends = 0; int total = 0; std::string label; };
// The panel's own Home Assistant link (core/ha_link.cpp, Settings > Home Assistant, 2026-10-30): HA's address + a
// long-lived token, kept on the device (the firmware's NVS through on_store / restore; WASM: memory only), never shown
// back in full (token_hint: the last 4) and never logged. fetch(domains) reads GET <url>/api/states once and keeps,
// per wanted domain (comma list), each entity's id + friendly name + icon (the entity pickers' lists). The reply is
// never held whole: Scan reads it chunk by chunk (depth stack + three capped strings + the kept rows), on any thread
// (the firmware's HTTP task parses there and hands the Scan over with done()). The transport is the platform's:
// on_get(id, url, token) starts the GET, chunk()/done() bring the body and the end back (main loop). set_proxy: the
// host fetches with its own token (the workbench's hub proxy), so the token never leaves the module.
// Agents / HA: actions ha.connect {url, token} and ha.entities {domains} (core/spec.h, CONTRACT.md C50).
namespace ha_link {
struct Entity { std::string id, name, icon; };
class Scan {
 public:
  explicit Scan(const std::string &domains);  // "" = every domain
  void feed(const char *p, size_t n);
  bool bad() const { return bad_; }
  size_t held() const { return held_; }  // bytes held now (buffers + kept rows); peak() the most at any point
  size_t peak() const { return peak_; }
  std::vector<std::string> want;
  std::map<std::string, std::vector<Entity>> out;
 private:
  void str_end();
  void obj_end();
  void add(char c);
  enum Cap : uint8_t { NONE, KEY, ID, NAME, ICON };
  static constexpr int DEPTH = 32;
  char kind_[DEPTH] = {};
  bool expect_key_[DEPTH] = {};
  int depth_ = 0, hex_ = -1;
  uint32_t cp_ = 0, hi_ = 0;
  bool in_str_ = false, esc_ = false, attrs_ = false, bad_ = false;
  Cap cap_ = NONE;
  std::string buf_, key2_, key3_, id_, name_, icon_;
  size_t held_ = 0, peak_ = 0;
};
enum St { OFF, IDLE, LOADING, OK, FAIL };
void configure(const std::string &url, const std::string &token);  // token "" = keep the one set; stores it
void restore(const std::string &url, const std::string &token);    // the firmware at boot (no store)
void on_store(void (*f)(const std::string &url, const std::string &token));
void set_proxy(bool on);
const std::string &url();
bool has_token();
std::string token_hint();  // "" or "...ab12"
bool ready();              // an address + a token (or a proxy host)
const char *fetch(const std::string &domains);  // "" = started, else why not
St status();
const std::string &why();  // the last failure, in words
uint32_t rev();            // moves on every change (pages poll it)
size_t peak();             // the last fetch's parse peak (bytes)
int count();               // entities kept by the last fetch
const std::vector<Entity> *list(const std::string &domain);  // nullptr = not fetched yet
using GetSink = void (*)(uint32_t id, const std::string &url, const std::string &token);
void on_get(GetSink f);
void chunk(uint32_t id, const char *p, size_t n);
void done(uint32_t id, bool ok, const std::string &why, Scan *parsed = nullptr);  // parsed: the platform's own Scan (taken)
// One WebSocket read (2026-10-31, the Energy feed: HA's Energy dashboard): connect to <url>/api/websocket, auth with
// the token, send cmd (one command as JSON, no "id"), stream its result message back through feed (chunk() with an
// id from WS_ID up), then end(ok, why) (done()); the link closes. Reads only: the transport allows a closed list
// (energy/get_prefs, recorder/statistics_during_period, recorder/list_statistic_ids; WASM: the hub's
// /api/panel/ha-ws, its own token; the firmware: to come). on_ws: the platform's sender. ws() = 0: not ready or no
// transport (end is not called).
constexpr uint32_t WS_ID = 1u << 30;
using WsSink = void (*)(uint32_t id, const std::string &url, const std::string &token, const std::string &cmd);
void on_ws(WsSink f);
uint32_t ws(const std::string &cmd, std::function<void(const char *, size_t)> feed, std::function<void(bool, const std::string &)> end);
}  // namespace ha_link
Timer &timer();  // legacy view of the list (above): the soonest running, else the first done
bool in_device(lv_obj_t *obj);  // obj is part of a mounted device (the panel), not a workbench page alone
lv_obj_t *first_root();  // the first mounted device's root, nullptr = none (fw::spec::act: nav.* from an agent / HA)
}  // namespace dev
}  // namespace fw
