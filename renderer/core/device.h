// Device runtime: the panel as one live thing, the same C++ on the panels and in the WASM
// workbench: a screen stack, Facet's screen transition and the taps that move between screens. Old Facet:
// @facet/ui device.ts createDevice() (go(id), back, Home's settings button, the Content rise on every cut).
//   mount      dev::mount(parent, "home") makes a full-screen root (lay::screen_w x screen_h) and shows that screen.
//              The workbench's "Device" page (screens/device.cpp) and the panel firmware
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
//   idle       Settings > Idle after (idleAfter s; 0 = never) with no touch and no screen change: the stack becomes
//              [home, idle], the idle clock (screens/idle.cpp: the Clock screen, its header hidden) fading in over 600 ms
//              with no rise, and the screen dims to Idle brightness: the host's on_idle hook (a panel: its backlight),
//              and the event "idle" <pct> ("" = undimmed: the workbench's canvas filter). The first touch is swallowed
//              and goes Home (undimmed). Never over Set up, the Home Assistant link, a ringing timer or a finger down;
//              only on a device whose root is Home. It never counts as activity: Screen off (the host's) still counts
//              from the last touch.
// TODO: Text size (a type-scale rebuild).
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
void toast(Props t, std::function<void()> on_action);  // its action Button runs on_action (core/keys.cpp: Undo)
lv_obj_t *mount(lv_obj_t *parent, const char *start = "home");  // the device root (its screen is its one child)
void go(lv_obj_t *in, const char *id);  // in = the root or anything inside it; queued for the next tick
void back(lv_obj_t *in);
void home(lv_obj_t *in);
// firmware update (Settings > Update): the host's update entity, told here on every change (a panel: ESPHome's
// Firmware entity, packages/workbench.yaml). state: "" = not checked yet, "none" = up to date, "available",
// "installing". update_install() asks the host to install the latest ("" = asked, else why not: no update, no host);
// it installs only when someone asks (the Settings sheet's Update button, Home Assistant's Install).
struct Update { std::string state, installed, latest; };
void update_set(const char *state, const char *installed, const char *latest);
const Update &update_now();
void on_update_install(void (*cb)());
const char *update_install();
// the panel's update password (a manual flash over the network, the public build's board/update_password.h): told once
// at boot; "" = none (the workbench, a build without one): Settings shows no Update password row. Never logged.
void update_password_set(const char *pw);
const std::string &update_password();
void on_idle(void (*cb)(bool idle, int pct));  // the idle clock shows (pct = Idle brightness) or leaves (100)
bool idle();  // a device shows the idle clock
void reload(const char *id);
void kept_changed(const char *id);  // a quiet deck change (spec.h kept_hook): the kept pages it touched go stale
// Navigation timing (a debug flag, off by default; facet-243): each screen change, in lv_tick ms: the tap handled
// (navigate / back / home queued), the change run, the build start and end (in it: the old screen deleted, the new
// one made, its layout done), the feeds' rounds during the build,
// the first frame drawn after it. nav_timing(true) starts recording (the last 32 kept); nav_json lists them.
struct NavTime { std::string screen; uint32_t touch = 0, tap = 0, run = 0, build0 = 0, del = 0, made = 0, laid = 0, build1 = 0, screen_layout0 = 0, render0 = 0, frame = 0, feed_ms = 0, feeds = 0, copy = 0;
  std::vector<std::pair<std::string, uint32_t>> prof; };  // the build's nodes and their ms (spec.h), in build order
void nav_timing(bool on);
bool nav_timing();
const std::vector<NavTime> &nav_times();
void nav_feed(uint32_t ms);  // a feed round's time (core/feed.cpp), counted into the change being built
void nav_prof(const char *what, uint32_t ms);  // a spec node built during the change being measured (its ms)
void nav_copy(uint32_t ms);  // the host's flush time inside the first frame (copy to the panel, its rotation)
void nav_render();           // the host's "a render starts" (its display's render-start: the wait before it is the loop's)
void nav_frame();            // the host's "a frame was drawn" (its display's render-ready)
void nav_touch();            // the host's "a finger lifted" (its pointer's release): the change it causes starts there
// A tap during a build (#94): a page build holds the loop (the Guition's kept pages: 150-1230 ms), so a polled touch
// chip is not read and a press and its lift both inside one build were lost. late_start(poll): the host's "read the
// chip now" (its touch listener tells late_press / late_lift); build_tick() calls it at most every 20 ms during any
// build (between a screen's nodes and inside the core atoms: one big component's build too). A press and a lift both
// seen there, with LVGL's own pointer never pressed in between, are given to LVGL after the build as one tap at the
// press point (a pointer of the core's own: the same path as a finger). A press still down after the build needs
// nothing: LVGL reads it as usual. late_ms(ms): the poll spacing (tests: 0 = every tick).
void late_start(void (*poll)());
void late_press(int32_t x, int32_t y);
void late_lift();
void late_ms(uint32_t ms);
void build_tick();
// Kept pages (facet-243, core/device.cpp): Home and the deck's pages built once in the idle gaps and kept hidden, so a
// tap shows a page instead of building it. keep_pages(n): at most n kept (0 = off, every screen built on tap). on_mem:
// the host's free-PSRAM reading (each kept page's cost). kept_json: [{page, bytes, objects, build_ms, stale, shown, waiting}] (waiting: binds whose data changed while
// hidden, repainted when it shows).
void keep_pages(int n);
int keep_pages();
uint32_t kept_all();  // lv_tick ms when every page to keep was first kept (boot to all kept; 0 = not yet)
void on_mem(size_t (*f)());
void kept_json(ArduinoJson::JsonArray out);
void state(ArduinoJson::JsonArray out);  // every mounted device: {stack, builds, objects} (spec hello diagnostics)  // a spec changed (core/spec.h apply): devices showing it (or every device: "deck") build it again
const char *current(lv_obj_t *in);  // the id on top of the stack ("" = not a device)

// settings (above)
enum From : uint8_t { SCREEN, HA, HOST, BOOT, EDITOR };  // who set it: the Settings screen, Home Assistant, a host / API (an
                                                       // agent's key), restore, the panel's own editor (same origin)
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

// hardware button (a panel's physical key, e.g. the Waveshare's BOOT key): the "Contextual" action.
// A part that has something to do with the key claims it while it lives: claim_button(obj, level, fn, user). A press
// goes to the claims whose object is shown (on the active screen or the top layer, not hidden), highest level first,
// then newest first; the first fn that returns true took it. A claim ends with its object (LV_EVENT_DELETE).
// Levels: BUTTON_ALERT (a toast, a ringing timer: dismiss), BUTTON_SHEET (an open bottom sheet: close), BUTTON_SCREEN (the screen's own
// action, e.g. Media play / pause). press = "single" | "double" | "long"; parts act on "single" unless they say so.
enum ButtonLevel : uint8_t { BUTTON_SCREEN = 0, BUTTON_SHEET = 1, BUTTON_ALERT = 2 };
using ButtonFn = bool (*)(void *user, const char *press);
void claim_button(lv_obj_t *obj, ButtonLevel level, ButtonFn fn, void *user);
const char *button(const char *press);  // the host's key press: "" = nothing took it, else the taker's object kind ("alert" ...)

// the panel's timers (notes timer.md; core/timers.cpp): a list of up to TIMERS_MAX, kept here so
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
namespace calendar_source { const char *control(const std::string &v); std::string hidden_list(); }
// the Media feed (core/media_source.cpp): media.control on panel.media; "" = done, else why not; svc = a media_player
// service to call on ent with data ("" = none)
namespace media_source { const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data); }
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
// The panel's own Home Assistant link (core/ha_link.cpp, Settings > Home Assistant): HA's address + a
// long-lived token, kept on the device (the firmware's NVS through on_store / restore; WASM: memory only), never shown
// back in full (token_hint: the last 4) and never logged. fetch(domains) reads GET <url>/api/states once and keeps,
// per wanted domain (comma list), each entity's id + friendly name + icon (the entity pickers' lists). The reply is
// never held whole: Scan reads it chunk by chunk (depth stack + three capped strings + the kept rows), on any thread
// (the firmware's HTTP task parses there and hands the Scan over with done()). The transport is the platform's:
// on_get(id, url, token) starts the GET, chunk()/done() bring the body and the end back (main loop). set_proxy: the
// host fetches with its own token (the workbench's host), so the token never leaves the module.
// Agents / HA: actions ha.connect {url, token} and ha.entities {domains} (core/spec.h, CONTRACT.md C50).
namespace ha_link {
struct Entity { std::string id, name, icon, dc; };  // dc: attributes.device_class ("" = none)
// a search (find): only the entities whose device class is one of dc (comma list, "" = any) and whose id or name holds
// q (ASCII case-insensitive, "" = any) are kept, at most FIND_MAX; the rest that match are counted (more)
struct Find { std::string dc, q; };
struct Hit { Entity e; std::string st, unit; };  // a search's match: its state and unit_of_measurement as Home Assistant has them
constexpr size_t FIND_MAX = 40;
constexpr size_t INDEX_MAX = 192 * 1024;  // an index's most bytes (a house of ~2,400 entities: ~130 KB for the card domains)
class Scan {
 public:
  explicit Scan(const std::string &domains);  // "" = every domain
  Scan(const std::string &domains, const Find &f);
  bool search = false, index = false;  // index: every wanted entity as one line in lines (read_index)
  int more = 0;  // a search's matches past FIND_MAX; an index's lines past INDEX_MAX
  std::string lines;
  std::vector<Hit> hits;  // a search's matches, in Home Assistant's order (a search keeps nothing in out)
  void feed(const char *p, size_t n);
  bool bad() const { return bad_; }
  size_t held() const { return held_; }  // bytes held now (buffers + kept rows); peak() the most at any point
  size_t peak() const { return peak_; }
  std::vector<std::string> want;
  std::map<std::string, std::vector<Entity>> out;
  std::map<std::string, int> left;  // per domain: entities not kept (ha_link.cpp PER, BUDGET)
 private:
  void str_end();
  void obj_end();
  void add(char c);
  bool match(const std::string &dom) const;
  Find f_;
  enum Cap : uint8_t { NONE, KEY, ID, NAME, ICON, DC, ST, UNIT };
  static constexpr int DEPTH = 32;
  char kind_[DEPTH] = {};
  bool expect_key_[DEPTH] = {};
  int depth_ = 0, hex_ = -1;
  uint32_t cp_ = 0, hi_ = 0;
  bool in_str_ = false, esc_ = false, attrs_ = false, bad_ = false;
  Cap cap_ = NONE;
  std::string buf_, key2_, key3_, id_, name_, icon_, dc_, st_, unit_;
  size_t held_ = 0, peak_ = 0, kept_ = 0;
};
enum St { OFF, IDLE, LOADING, OK, FAIL, CHECKING };
// configure: token "" = keep the one set; stores it, then checks the link (GET <url>/api/ through get(): CHECKING, then
// IDLE + checked() or FAIL + why(): "Home Assistant refused the token", "cannot reach Home Assistant at that address")
void configure(const std::string &url, const std::string &token);
void restore(const std::string &url, const std::string &token);    // the firmware at boot (no store)
// the link as it is now, to put back (core/keys.cpp: Undo of an agent's ha.connect); never leaves the core
struct Link { std::string url, token; };
Link kept();
void put_back(const Link &l);
void on_store(void (*f)(const std::string &url, const std::string &token));
void set_proxy(bool on);
const std::string &url();
bool has_token();
std::string token_hint();  // "" or "...ab12"
bool ready();              // an address + a token (or a proxy host)
const char *fetch(const std::string &domains);  // "" = started, else why not
St status();
const std::string &why();  // the last failure, in words
bool checked();            // the last check since the link was set answered (the address and token work)
// the panel's own link page (firmware: http://<its address>/link once on Wi-Fi, fw_device.h; "" = none): Settings >
// Home Assistant and Set up show it as a QR code while the panel is not linked
void set_page(const std::string &u);
const std::string &page();
// Home Assistant's own connection TO the panel (the ESPHome API) and its encryption key, which the firmware keeps:
// pairing() = NO_PAIRING (this build has none: WASM, a build without encryption), UNPAIRED (no key: Home Assistant can
// adopt the panel and set one), PAIRED (a key Home Assistant set) or FIXED (a key built in: no reset).
// reset_pairing() clears a key Home Assistant set (Settings > Home Assistant's Reset pairing, POST /fw/pairing/reset),
// so a Home Assistant that lost it ("invalid encryption key") can adopt the panel again; false = nothing to clear.
enum Pairing { NO_PAIRING = -1, UNPAIRED = 0, PAIRED = 1, FIXED = 2 };
void on_pairing(Pairing (*state)(), bool (*reset)());  // the firmware's (fw_device.h); the workbench: the screen's prop
Pairing pairing();
bool reset_pairing();
const char *pairing_word(Pairing p);  // "none" | "unpaired" | "paired" | "fixed"
uint32_t rev();            // moves on every change (pages poll it)
size_t peak();             // the last fetch's parse peak (bytes)
int count();               // entities kept by the last fetch
int left_out(const std::string &domain);  // entities of that domain the last read did not keep (too many)
void forget(const std::string &domain);  // free a domain's list (its next picker reads it again)
const std::vector<Entity> *list(const std::string &domain);  // nullptr = not fetched yet
// the editor's add list (/fw/entities ?q= &dc=): find() reads GET <url>/api/states once more and keeps only the matches
// (Find), never the lists (the cache is left as it is); found(domains, f) = those matches once that read answered,
// nullptr before (or when the last search was another one). One search is held at a time; a new one replaces it.
const char *find(const std::string &domains, const Find &f);  // "" = started, else why not
const std::vector<Hit> *found(const std::string &domains, const Find &f, int *more = nullptr);
// the search's answer, the same on the panel (GET /fw/entities?q=&dc=) and in WASM (fw_ha_find): {"state": ok | loading
// | off | failed, "why"?, "found": [{id, name, dc, st, unit}], "more": n}; loading = asked again until ok (a read started here)
std::string find_json(const std::string &domains, const Find &f);
// the editor's search index (/fw/entities?index=1): one read of /api/states keeping, for the wanted domains, one line
// per entity "id\tname\tdevice_class\tstate\tunit\n" in one string (no per-entity strings: ~80 bytes each), at most
// INDEX_MAX. index_text(domains, out): "#loading\n" (a read started or running: ask again), "#off\n", "#failed\t<why>\n",
// or the lines + "#ok\t<left out>\n", MOVED into out: the panel keeps nothing once it has answered (the browser searches it)
void index_text(const std::string &domains, std::string &out);
// the platform's largest free block (firmware: heap_caps_get_largest_free_block; none = WASM, no limit): an index's
// string is allocated once, before its read, at most half of it (and INDEX_MAX), and never grown; lines past it are
// left out (counted), never a failed allocation
void on_largest(size_t (*f)());  // called once per index read only (it walks the heap)
void on_free(size_t (*f)());     // the free total (cheap): the low-memory refusal
// a domain's list read longer ago than this: its picker reads it again in the background when it opens (the old list
// shows meanwhile), so entities added to Home Assistant appear without a manual read
constexpr uint32_t STALE_MS = 5 * 60 * 1000;
bool stale(const std::string &domain);  // cached and read more than STALE_MS ago
using GetSink = void (*)(uint32_t id, const std::string &url, const std::string &token);
void on_get(GetSink f);
void chunk(uint32_t id, const char *p, size_t n);
void done(uint32_t id, bool ok, const std::string &why, Scan *parsed = nullptr);  // parsed: the platform's own Scan (taken)
// One WebSocket read (the Energy feed: HA's Energy dashboard): connect to <url>/api/websocket, auth with
// the token, send cmd (one command as JSON, no "id"), stream its result message back through feed (chunk() with an
// id from WS_ID up), then end(ok, why) (done()); the link closes. Reads only: the transport allows a closed list
// (energy/get_prefs, energy/info, recorder/statistics_during_period, recorder/list_statistic_ids, and the three registry lists
// above; WASM: the workbench
// host, with its own token; the firmware: esp_transport_ws, firmware/fw_device.h). on_ws: the platform's sender. ws() = 0: not ready or no
// transport (end is not called).
// HA's registries for one domain's entities (the Media sources: the integration behind each player, and its area): the
// entity registry (config/entity_registry/list_for_display: platform, own area, device), the device registry (the
// device's area) and the area registry (names, in HA's order), three WS reads in a row, only the wanted rows kept.
// reg_fetch(domain) starts them once per link (again after a failure, or on a new link; again = read afresh, the
// rows kept until the new ones come); reg(domain) = nullptr until all three answered. A failed read leaves reg() empty, never wrong.
struct RegRow { std::string id, platform, area; int area_order = 1 << 20; };  // area "" = none (order last)
void reg_fetch(const std::string &domain, bool again = false);
const std::vector<RegRow> *reg(const std::string &domain);
constexpr uint32_t WS_ID = 1u << 30;
using WsSink = void (*)(uint32_t id, const std::string &url, const std::string &token, const std::string &cmd);
void on_ws(WsSink f);
uint32_t ws(const std::string &cmd, std::function<void(const char *, size_t)> feed, std::function<void(bool, const std::string &)> end);
// One REST read (the Energy feed's extras: an integration's entities and calendars): GET <url>/api/<path>
// with the token through the platform's GET (on_get; ids from WS_ID up, kept like a ws read), the body streamed back
// through feed, then end(ok, why). Reads only, a closed list on the transport (WASM: the workbench host's HA reads:
// states/<entity>, calendars, calendars/<calendar>?start&end). 0 = not ready or no transport (end is not called).
uint32_t get(const std::string &path, std::function<void(const char *, size_t)> feed, std::function<void(bool, const std::string &)> end);
}  // namespace ha_link
Timer &timer();  // legacy view of the list (above): the soonest running, else the first done
bool in_device(lv_obj_t *obj);  // obj is part of a mounted device (the panel), not a workbench page alone
lv_obj_t *first_root();  // the first mounted device's root, nullptr = none (fw::spec::act: nav.* from an agent / HA)
}  // namespace dev
}  // namespace fw
