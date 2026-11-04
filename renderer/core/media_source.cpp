// The media feed (CONTRACT.md C51): the reserved entity panel.media (core/spec.h "local sources") in the HA store, the
// one shape the Media screen and its Home card bind to, whatever plays the music. Same file on the panels and in WASM.
//   state        ok | loading | error (no player picked) | missing (Home Assistant has no such player) | unavailable
//                (it has no state now) (the Feed base, core/feed.h); player playing | paused | idle | off | loading |
//                error | missing | unavailable
//   state_label  "now playing" | "paused" | "idle" | "Not found" | "Unavailable" | "Offline" (no player picked, Home
//                Assistant not reached) (docs .fs-m-state; feed::words); state_tone accent-text while playing (docs data-tone working), else text-dim
//   playing      "true" | "false" (the Media visual turns; the play button is active)
//   play_state   active | default | unavailable (the play Icon button); ctl_state default | unavailable | missing (prev
//                / next, the volume Slider row: its word)
//   title        the track, or "Nothing playing"; artist; album; art (the player's entity_picture: "" = none; a path
//                on Home Assistant gets the panel's HA address in front, in the workbench it stays a path its host
//                fetches; core/images.h); source (the input)
//   progress     0-100 of the track, moving on locally once a second while playing (from HA's position and the time
//                it was read); position / duration in seconds
//   volume       0-100 (the Slider); volume_text the number shown beside it ("--" = unknown)
//   speaker      the player's name; speakers = the speaker sheet's rows, a JSON list of {id, name, status, icon,
//                tone, selected}: Auto first (follows what's playing), then each player
//   sheet        open | closed (the speaker Bottom sheet)
//   card_text / card_sub / card_accent  the Home card: the track (or Nothing playing), "artist" or the speaker's
//                state (the words), accent while playing
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source; no player or speaker settings:
// the speakers are the source's, found in Home Assistant (core/device.h ha_link::reg: the entity, device and area
// registries; before they answer, every media_player for "ha"), at most 24, in the sheet's order: playing first, then by
// area (HA's order, none last), then name. Auto (the default) follows the first one playing; a pick holds one.
//   ha               every media_player in Home Assistant
//   music_assistant, cast, sonos  the players of that integration (the registry's platform)
//   spotify          a Spotify account's player; its speakers = the Spotify Connect devices (source_list), a pick =
//                    media_player.select_source; no Auto (the account plays on one device)
//   Each speaker is read for its state, track and name; the one shown for every attribute (state, media_title,
//   media_artist, media_album_name, entity_picture, media_position(_updated_at), media_duration, volume_level, source,
//   friendly_name). The controls go out through the action media.control (core/spec.h run_action: the one checked
//   path) as media_player services on the player shown.
//   sample  three made-up tracks on a made-up speaker, playing on the real clock; the controls act on them here
//           (play / pause, previous / next, volume, a speaker pick): nothing leaves the panel
// Controls (control(); the screen's taps through media.control on e panel.media): speakers / close (the sheet),
// pick <id> (a sheet row: auto or a player), previous, next, play_pause, volume N (released), live N (dragging: the
// number follows; HA at most every 200 ms, the release sends the last). Play / pause and the volume show at once and
// hold HA's echoes off for 3 s / 2.5 s (docs volumeControl).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include "feed.h"
#include "fw.h"
#include "ha.h"
#include "device.h"

namespace fw::dev::media_source {
namespace {
const char *const ATTRS[] = {"", "media_title", "media_artist", "media_album_name", "entity_picture", "media_position",
                             "media_duration", "media_position_updated_at", "volume_level", "source", "friendly_name"};
struct Track { const char *title, *artist, *album; int secs; };
const Track SAMPLE[] = {{"Harbour Lights", "The Lanterns", "Night Ferries", 214},
                        {"Paper Moons", "Juniper Row", "Small Hours", 187},
                        {"Slow Tide", "Marla Kent", "Coastline", 242}};
const char *const SAMPLE_SPK[][2] = {{"sample.kitchen", "Kitchen speaker"}, {"sample.living_room", "Living room TV"},
                                     {"sample.office", "Office speaker"}};
struct St {
  std::string cfg_seen, src = "sample", player, picked, follow_last, sheet = "closed";
  std::vector<std::string> speakers;
  std::map<std::string, int> area_rank;            // speaker -> its area's place in HA's order
  std::map<std::string, std::string> area_name;    // speaker -> its area's name
  bool follow = true, waiting = false, had_reg = false;  // waiting: the source's speakers are not known yet
  uint32_t link_rev = 0, looked = 0;
  // sample
  int track = 0, vol = 42, spk = 0;
  bool playing = true;
  double at_ms = 0, pos0 = 37;  // the track's position pos0 at wall time at_ms
  // holds (lv ticks): HA echoes ignored until then
  uint32_t hold_play = 0, hold_vol = 0, last_send = 0;
  std::string held_state;
  int held_vol = -1, live_vol = -1;
};
St &S() { static St s; return s; }

double now_ms() { return (double) time(nullptr) * 1000; }
std::function<void(const char *, const std::string &)> put;  // the feed's put (Media below)
void put_state(feed::State st);
std::string num(double v) { char b[24]; snprintf(b, sizeof b, "%d", (int) std::lround(v)); return b; }
std::string esc(const std::string &s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; if ((unsigned char) c >= 0x20) o += c; }
  return o;
}
// "2026-10-30T00:12:34.123+00:00" -> unix ms (HA writes UTC); 0 = not a time
double iso_ms(const std::string *s) {
  if (!s) return 0;
  int y, mo, d, h, mi;
  double sec;
  if (sscanf(s->c_str(), "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &sec) != 6) return 0;
  y -= mo <= 2;  // days since 1970-01-01 (H. Hinnant; no timegm in newlib, the panels' libc)
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400), doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long days = era * 146097 + (long) (yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
  return ((double) days * 86400 + h * 3600 + mi * 60) * 1000 + sec * 1000;
}
const std::string *attr(const std::string &e, const char *a) { return e.empty() ? nullptr : ha::get(e, a); }
std::string name_of(const std::string &e) {
  const std::string *f = attr(e, "friendly_name");
  if (f && !f->empty()) return *f;
  std::string n = e.substr(e.find('.') + 1);
  for (char &c : n) if (c == '_') c = ' ';
  if (!n.empty()) n[0] = (char) toupper((unsigned char) n[0]);
  return n;
}

// the shown values, from whichever adapter
struct Shown {
  std::string state = "idle", title, artist, album, art, source, speaker;
  double pos = 0, dur = 0;
  int vol = -1;
};
void put_shown(const Shown &m) {
  St &s = S();
  const bool gone = m.state == "missing", down = gone || m.state == "error" || m.state == "unavailable", playing = m.state == "playing";
  const char *word = feed::words(gone ? feed::MISSING : m.state == "unavailable" ? feed::UNAVAILABLE : feed::ERROR);  // (the label is caps)
  put("state_label", down ? word : playing ? "now playing" : m.state == "paused" ? "paused" : m.state == "loading" ? "loading" : "idle");
  put("state_tone", playing ? "accent-text" : "text-dim");
  put("playing", playing ? "true" : "false");
  put("play_state", down ? "unavailable" : playing ? "active" : "default");
  put("ctl_state", gone ? "missing" : down ? "unavailable" : "default");
  put("title", m.title.empty() ? "Nothing playing" : m.title);
  put("artist", m.artist), put("album", m.album), put("art", m.art), put("source", m.source);
  put("position", num(m.pos)), put("duration", num(m.dur));
  put("progress", m.dur > 0 ? num(LV_CLAMP(0.0, m.pos / m.dur * 100, 100.0)) : "0");
  const int v = s.live_vol >= 0 ? s.live_vol : m.vol;
  if (m.vol >= 0) put("volume", num(m.vol));
  put("volume_text", v >= 0 ? num(v) : "--");
  put("speaker", m.speaker.empty() ? "none" : m.speaker);
  put("sheet", s.sheet);
  put("card_text", m.title.empty() ? "Nothing playing" : m.title);
  put("card_sub", down ? std::string(word) : !m.artist.empty() ? m.artist : m.state == "paused" ? "paused · " + m.speaker : m.speaker);
  put("card_accent", playing ? "true" : "false");
  put("player", m.state);
  put_state(gone ? feed::MISSING : m.state == "unavailable" ? feed::UNAVAILABLE : down ? feed::ERROR : m.state == "loading" ? feed::LOADING : feed::OK);
}
std::string row(const std::string &id, const std::string &name, const std::string &status, const char *icon, bool working, bool sel) {
  return "{\"id\":\"" + esc(id) + "\",\"name\":\"" + esc(name) + "\",\"status\":\"" + esc(status) + "\",\"icon\":\"" + icon +
         "\",\"tone\":\"" + (working ? "working" : "dim") + "\",\"selected\":" + (sel ? "true" : "false") + "}";
}

// ---- adapter: sample
double sample_pos() {
  St &s = S();
  return s.playing ? s.pos0 + (now_ms() - s.at_ms) / 1000 : s.pos0;
}
void sample_now() {
  St &s = S();
  if (s.at_ms == 0) s.at_ms = now_ms();
  double p = sample_pos();
  while (p >= SAMPLE[s.track].secs) {  // the next track starts
    p -= SAMPLE[s.track].secs;
    s.track = (s.track + 1) % 3;
    s.pos0 = p, s.at_ms = now_ms();
  }
  const Track &t = SAMPLE[s.track];
  Shown m;
  m.state = s.playing ? "playing" : "paused";
  m.title = t.title, m.artist = t.artist, m.album = t.album, m.source = "Music";
  m.pos = p, m.dur = t.secs, m.vol = s.vol, m.speaker = SAMPLE_SPK[s.spk][1];
  std::string l = "[" + row("auto", "Auto", "follows what's playing · now " + m.speaker, "auto", false, s.follow);
  for (int i = 0; i < 3; i++) {
    const bool here = i == s.spk;
    l += "," + row(SAMPLE_SPK[i][0], SAMPLE_SPK[i][1], here ? (s.playing ? std::string("playing · ") + t.title : "paused") : "idle",
                   i == 1 ? "tv" : "speaker", here && s.playing, !s.follow && here);
  }
  put("speakers", l + "]");
  put_shown(m);
}

// ---- adapters: ha (every player), an integration's players (music_assistant, cast, sonos: the source names the
// platform), spotify (one Spotify account player; its speakers = the Spotify Connect devices in its source_list)
bool is_spotify() { return S().src == "spotify"; }
std::string current() {  // the player shown: a pick, else Auto's target, else the first speaker
  St &s = S();
  if (is_spotify()) return s.player;
  if (!s.follow && !s.picked.empty()) return s.picked;
  if (!s.follow_last.empty()) return s.follow_last;
  return s.player;
}
void want_all(const std::string &e) { for (const char *a : ATTRS) ha::want(e, a); }
void follow_now() {  // Auto: the first speaker that plays (they are in that order already)
  St &s = S();
  if (!s.follow || is_spotify()) return;
  for (const std::string &e : s.speakers) {
    const std::string *st = ha::get(e);
    if (st && *st == "playing") {
      if (s.follow_last != e) want_all(e);
      s.follow_last = e;
      return;
    }
  }
}
// a list attribute as HA's API gives it ("['A', 'B']" through the panels' native API, JSON in WASM) -> its strings
std::vector<std::string> str_list(const std::string *v) {
  std::vector<std::string> out;
  if (!v) return out;
  for (size_t i = 0; i < v->size() && out.size() < 24; i++) {
    const char q = (*v)[i];
    if (q != '\'' && q != '"') continue;
    const size_t j = v->find(q, i + 1);
    if (j == std::string::npos) break;
    out.push_back(v->substr(i + 1, j - i - 1));
    i = j;
  }
  return out;
}
// the speakers in the sheet's order: playing first, then by Home Assistant area (HA's order; none last), then name
void order() {
  St &s = S();
  auto rank = [&](const std::string &e) { auto it = s.area_rank.find(e); return it == s.area_rank.end() ? 1 << 20 : it->second; };
  auto on = [](const std::string &e) { const std::string *st = ha::get(e); return st && *st == "playing"; };
  std::stable_sort(s.speakers.begin(), s.speakers.end(), [&](const std::string &x, const std::string &y) {
    if (on(x) != on(y)) return on(x);
    if (rank(x) != rank(y)) return rank(x) < rank(y);
    return name_of(x) < name_of(y);
  });
}
void rebuild() {
  St &s = S();
  if (s.src == "sample") return;
  order();
  follow_now();
  const std::string e = current();
  Shown m;
  const std::string *st = attr(e, "");
  m.speaker = e.empty() ? "" : name_of(e);
  const feed::State fs = feed::of(st);
  // no speaker: still finding them (loading), Home Assistant has none for the source (missing: Not found), or no link
  // (error: Offline), connected long enough to say so (missing: Not found); else the player's own state
  if (e.empty() || fs != feed::OK)
    m.state = !e.empty() ? feed::state_id(fs) : ha::offline() ? "error" : s.waiting || !ha::settled() ? "loading" : "missing";
  else {
    m.state = *st == "playing" || *st == "paused" || *st == "off" ? *st : "idle";
    if (lv_tick_get() < s.hold_play && !s.held_state.empty()) m.state = s.held_state;  // our own tap wins for 3 s
    auto a = [&](const char *k) { const std::string *v = attr(e, k); return v ? *v : std::string(); };
    m.title = a("media_title"), m.artist = a("media_artist"), m.album = a("media_album_name");
    m.art = a("entity_picture"), m.source = a("source");
    if (m.art == "None") m.art.clear();
    if (m.art.size() > 1 && m.art[0] == '/' && m.art[1] != '/' && !fw::dev::ha_link::url().empty()) m.art = fw::dev::ha_link::url() + m.art;
    m.dur = atof(a("media_duration").c_str());
    m.pos = atof(a("media_position").c_str());
    const double at = iso_ms(attr(e, "media_position_updated_at"));
    if (m.state == "playing" && at > 0) m.pos += std::max(0.0, (now_ms() - at) / 1000);
    if (m.dur > 0) m.pos = std::min(m.pos, m.dur);
    const std::string v = a("volume_level");
    m.vol = v.empty() ? -1 : (int) std::lround(atof(v.c_str()) * 100);
    if (lv_tick_get() < s.hold_vol && s.held_vol >= 0) m.vol = s.held_vol;
  }
  std::string l = "[";
  if (is_spotify()) {  // the Spotify Connect devices; the one playing now is the account's source
    const std::string *cur = attr(e, "source");
    for (const std::string &d : str_list(attr(e, "source_list"))) {
      const bool here = cur && *cur == d;
      l += std::string(l.size() > 1 ? "," : "") + row(d, d, here ? (m.state == "playing" ? "playing" : m.state) : "", "speaker", here && m.state == "playing", here);
    }
    if (l.size() > 1 && m.source.size()) m.speaker = m.source;
  } else {
    l += row("auto", "Auto", s.follow_last.empty() ? "follows what's playing" : "follows what's playing · now " + name_of(s.follow_last), "auto", false, s.follow);
    for (const std::string &p : s.speakers) {
      const std::string *ps = ha::get(p), *pt = attr(p, "media_title");
      const bool on = ps && *ps == "playing";
      auto ar = s.area_name.find(p);
      const std::string where = ar == s.area_name.end() ? "" : ar->second;
      const std::string what = on && pt && !pt->empty() ? "playing · " + *pt : ps ? *ps : "";
      l += "," + row(p, name_of(p), where.empty() ? what : what.empty() ? where : where + " · " + what, "speaker", on, !s.follow && p == e);
    }
  }
  put("speakers", l + "]");
  put_shown(m);
}
// the speakers the source offers: from HA's registries (the platform, the area) once read, else every media_player
// HA has (the link's list) for "every player"; until either comes, none (the feed sets up again when they come)
bool speakers_now(std::vector<std::string> &out) {
  namespace hl = fw::dev::ha_link;
  St &s = S();
  out.clear(), s.area_rank.clear(), s.area_name.clear();
  hl::reg_fetch("media_player");
  const std::string plat = s.src == "ha" ? "" : s.src;
  if (const std::vector<hl::RegRow> *r = hl::reg("media_player"); r && !r->empty()) {
    for (const hl::RegRow &x : *r) {
      if (!plat.empty() && x.platform != plat) continue;
      if (out.size() >= 24) break;
      out.push_back(x.id);
      s.area_rank[x.id] = x.area_order;
      if (!x.area.empty()) s.area_name[x.id] = x.area;
    }
    return true;
  }
  if (!plat.empty() && !(hl::status() == hl::FAIL)) return false;  // an integration's players: wait for the registry
  const std::vector<hl::Entity> *l = hl::list("media_player");
  if (!l) {
    if (hl::ready() && hl::status() != hl::LOADING && (hl::status() != hl::FAIL || lv_tick_diff(lv_tick_get(), s.looked) > 30000)) {
      s.looked = lv_tick_get();  // again after a failed read too, at most every 30 s
      hl::fetch("media_player");
    }
    return false;
  }
  for (const hl::Entity &e : *l) {
    if (out.size() >= 24) break;
    if (plat.empty() || e.id.rfind("media_player." + plat, 0) == 0) out.push_back(e.id);  // no registry: Spotify by its id
  }
  return true;
}
// the settings: the source, Auto, the sheet closed again (a new screen or a new source)
void setup(const ArduinoJson::JsonDocument &c) {
  St &s = S();
  s.src = c["source"] | "sample";
  s.speakers.clear(), s.player.clear();
  s.waiting = s.src != "sample" && !speakers_now(s.speakers);
  if (!s.speakers.empty()) s.player = s.speakers[0];
  s.picked.clear(), s.follow_last.clear();
  s.follow = true;
  s.sheet = "closed";
}
std::vector<feed::Field> media_fields() {
  std::vector<feed::Field> v;
  for (const char *n : {"state_label", "state_tone", "playing", "play_state", "ctl_state", "title", "card_text", "card_sub", "card_accent", "player", "sheet"})
    v.push_back({n, true, ""});
  for (const char *n : {"artist", "album", "art", "source", "speakers"}) v.push_back({n, false, ""});
  for (const char *n : {"position", "duration", "progress"}) v.push_back({n, false, "0"});
  v.push_back({"volume", false, nullptr});  // the Slider keeps its place when the player does not say
  v.push_back({"volume_text", false, "--"});
  v.push_back({"speaker", false, "none"});
  return v;
}
class Media : public feed::Feed {
 public:
  Media() : Feed("panel.media", 1000, "ha", media_fields()) {}  // the progress moves on once a second while shown
  using Feed::put;
  using Feed::state;

 protected:
  void sample() override {
    if (changed()) setup(cfg()), reads({});
    sample_now();
  }
  void fill() override {
    St &s = S();
    namespace hl = fw::dev::ha_link;
    const uint32_t rev = hl::rev();
    if (s.src != "sample" && !s.waiting && s.speakers.empty() && lv_tick_diff(lv_tick_get(), s.looked) > 30000) {
      s.looked = lv_tick_get();  // none found: look again (a player added in Home Assistant, a speaker back)
      hl::reg_fetch("media_player", true);
    }
    if (changed() || (rev != s.link_rev && (s.waiting || s.speakers.empty() || !s.had_reg)) ||
        (s.waiting && lv_tick_diff(lv_tick_get(), s.looked) > 30000)) {
      s.link_rev = rev;
      setup(cfg());
      s.had_reg = hl::reg("media_player") != nullptr;
      std::vector<std::pair<std::string, std::string>> r;
      for (const std::string &p : s.speakers)  // each speaker: its state, track and name (the sheet); the shown one: all
        for (const char *a : {"", "media_title", "friendly_name"}) r.push_back({p, a});
      if (!s.player.empty()) for (const char *a : ATTRS) r.push_back({s.player, a});
      if (is_spotify() && !s.player.empty()) r.push_back({s.player, "source_list"});
      reads(r);
    }
    s.link_rev = rev;
    rebuild();
  }
};
Media F;
void put_state(feed::State st) { F.state(st); }
[[maybe_unused]] const bool wired = (put = [](const char *a, const std::string &x) { F.put(a, x); }, true);
}  // namespace

// a tap on the Media screen (see the header); svc / ent / data = a media_player service to call ("" = none: done here)
const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data) {
  St &s = S();
  svc->clear(), data->clear();
  const bool ha = s.src != "sample";
  *ent = ha ? current() : "";
  auto again = [&] { F.run(); };
  if (v == "speakers" || v == "close") { s.sheet = v == "speakers" ? "open" : "closed"; put("sheet", s.sheet); return ""; }
  if (v.rfind("pick ", 0) == 0) {  // a sheet row by its id or its name (a Picker row's tap says the name)
    std::string id = v.substr(5);
    if (id == "Auto") id = "auto";
    s.sheet = "closed";
    if (ha && is_spotify()) {  // a Spotify Connect device: the account plays there
      if (ent->empty()) return "no media player";
      ArduinoJson::JsonDocument d;
      d["source"] = id;
      *svc = "media_player.select_source", serializeJson(d, *data);
      again();
      return "";
    }
    for (int i = 0; i < 3; i++) if (id == SAMPLE_SPK[i][1]) id = SAMPLE_SPK[i][0];
    for (const std::string &e : s.speakers) if (id == name_of(e)) id = e;
    if (id == "auto") s.follow = true, s.follow_last.clear();
    else if (ha) {
      if (std::find(s.speakers.begin(), s.speakers.end(), id) == s.speakers.end()) return "not one of the speakers";
      s.follow = false, s.picked = id;
      want_all(id);
    } else {
      for (int i = 0; i < 3; i++) if (id == SAMPLE_SPK[i][0]) s.spk = i, s.follow = false;
    }
    *ent = ha ? current() : "";
    again();
    return "";
  }
  if (ha && ent->empty()) return "no media player";
  if (v == "play_pause") {
    if (ha) {
      const std::string *st = attr(*ent, "");
      s.held_state = st && *st == "playing" ? "paused" : "playing";
      s.hold_play = lv_tick_get() + 3000;
      *svc = "media_player.media_play_pause";
    } else {
      s.pos0 = sample_pos(), s.at_ms = now_ms();
      s.playing = !s.playing;
    }
  } else if (v == "previous" || v == "next") {
    if (ha) *svc = v == "next" ? "media_player.media_next_track" : "media_player.media_previous_track";
    else {
      s.track = (s.track + (v == "next" ? 1 : 2)) % 3;
      s.pos0 = 0, s.at_ms = now_ms();
    }
  } else if (v.rfind("volume ", 0) == 0 || v.rfind("live ", 0) == 0) {
    const bool live = v[0] == 'l';
    const int n = LV_CLAMP(0, atoi(v.c_str() + v.find(' ') + 1), 100);
    s.live_vol = live ? n : -1;
    if (!ha) { if (!live) s.vol = n; }
    else {
      s.held_vol = n, s.hold_vol = lv_tick_get() + 2500;
      if (!live || lv_tick_diff(lv_tick_get(), s.last_send) >= 200) {
        char b[40];
        snprintf(b, sizeof b, "{\"volume_level\":%.2f}", n / 100.0);
        *svc = "media_player.volume_set", *data = b;
        s.last_send = lv_tick_get();
      }
    }
    if (live) { put("volume_text", num(n)); return ""; }  // the Slider keeps the finger's value
  } else return "previous, next, play_pause, volume N, speakers, close or pick <id>";
  again();
  return "";
}
}  // namespace fw::dev::media_source
