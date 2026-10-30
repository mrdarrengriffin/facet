// The media feed (CONTRACT.md C51): the reserved entity panel.media (core/spec.h "local sources") in the HA store, the
// one shape the Media screen and its Home card bind to, whatever plays the music. Same file on the panels and in WASM.
//   state        playing | paused | idle | off | loading | error
//   state_label  "now playing" | "paused" | "idle" | "unavailable" (docs .fs-m-state); state_tone accent-text while
//                playing (docs data-tone working), else text-dim
//   playing      "true" | "false" (the Media visual turns; the play button is active)
//   play_state   active | default | unavailable (the play Icon button); ctl_state default | unavailable (prev / next,
//                the volume Slider row)
//   title        the track, or "Nothing playing"; artist; album; art (an image URL, "" = none); source (the input)
//   progress     0-100 of the track, moving on locally once a second while playing (from HA's position and the time
//                it was read); position / duration in seconds
//   volume       0-100 (the Slider); volume_text the number shown beside it ("--" = unknown)
//   speaker      the player's name; speakers = the speaker sheet's rows, a JSON list of {id, name, status, icon,
//                tone, selected}: Auto first (follows what's playing), then each player
//   sheet        open | closed (the speaker Bottom sheet)
//   card_text / card_sub / card_accent  the Home card: the track (or Nothing playing), "artist" or the speaker's
//                state, accent while playing
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant media_player (setting player; speakers = a comma list of players for the sheet; auto =
//           an optional sensor naming the player to follow), read live like any bind (state, media_title,
//           media_artist, media_album_name, entity_picture, media_position(_updated_at), media_duration,
//           volume_level, source, friendly_name). The controls go out through the action media.control (core/spec.h
//           run_action: the one checked path) as media_player services on the player picked here.
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
#include "fw.h"
#include "ha.h"

namespace fw::dev::media_source {
namespace {
constexpr const char *E = "panel.media";
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
  bool follow = true;
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
lv_timer_t *tick = nullptr;

double now_ms() { return (double) time(nullptr) * 1000; }
void put(const char *a, const std::string &v) { ha::put(E, a, v); }
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
  const bool down = m.state == "error", playing = m.state == "playing";
  put("state_label", down ? "unavailable" : playing ? "now playing" : m.state == "paused" ? "paused" : m.state == "loading" ? "loading" : "idle");
  put("state_tone", playing ? "accent-text" : "text-dim");
  put("playing", playing ? "true" : "false");
  put("play_state", down ? "unavailable" : playing ? "active" : "default");
  put("ctl_state", down ? "unavailable" : "default");
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
  put("card_sub", down ? "unavailable" : !m.artist.empty() ? m.artist : m.state == "paused" ? "paused · " + m.speaker : m.speaker);
  put("card_accent", playing ? "true" : "false");
  put("", m.state);
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

// ---- adapter: ha (a media_player)
std::string current() {  // the player shown: a pick, else Auto's target, else the setting
  St &s = S();
  if (!s.follow && !s.picked.empty()) return s.picked;
  if (!s.follow_last.empty()) return s.follow_last;
  return s.player;
}
void follow_now() {
  St &s = S();
  if (!s.follow) return;
  // the follow sensor names the player; else the first of the speakers that plays
  for (const std::string &e : s.speakers) {
    const std::string *st = ha::get(e);
    if (st && *st == "playing") { s.follow_last = e; return; }
  }
}
void rebuild() {
  St &s = S();
  if (s.src != "ha") return;
  follow_now();
  const std::string e = current();
  Shown m;
  const std::string *st = attr(e, "");
  m.speaker = e.empty() ? "" : name_of(e);
  if (e.empty() || !st) m.state = e.empty() ? "error" : "loading";
  else if (*st == "unavailable" || *st == "unknown") m.state = "error";
  else {
    m.state = *st == "playing" || *st == "paused" || *st == "off" ? *st : "idle";
    if (lv_tick_get() < s.hold_play && !s.held_state.empty()) m.state = s.held_state;  // our own tap wins for 3 s
    auto a = [&](const char *k) { const std::string *v = attr(e, k); return v ? *v : std::string(); };
    m.title = a("media_title"), m.artist = a("media_artist"), m.album = a("media_album_name");
    m.art = a("entity_picture"), m.source = a("source");
    m.dur = atof(a("media_duration").c_str());
    m.pos = atof(a("media_position").c_str());
    const double at = iso_ms(attr(e, "media_position_updated_at"));
    if (m.state == "playing" && at > 0) m.pos += std::max(0.0, (now_ms() - at) / 1000);
    if (m.dur > 0) m.pos = std::min(m.pos, m.dur);
    const std::string v = a("volume_level");
    m.vol = v.empty() ? -1 : (int) std::lround(atof(v.c_str()) * 100);
    if (lv_tick_get() < s.hold_vol && s.held_vol >= 0) m.vol = s.held_vol;
  }
  std::string l = "[" + row("auto", "Auto", s.follow_last.empty() ? "follows what's playing" : "follows what's playing · now " + name_of(s.follow_last), "auto", false, s.follow);
  for (const std::string &p : s.speakers) {
    const std::string *ps = ha::get(p), *pt = attr(p, "media_title");
    const bool on = ps && *ps == "playing";
    l += "," + row(p, name_of(p), on && pt && !pt->empty() ? "playing · " + *pt : ps ? *ps : "", "speaker", on, !s.follow && p == e);
  }
  put("speakers", l + "]");
  put_shown(m);
}
void want_all() {
  St &s = S();
  for (const char *a : ATTRS) if (!s.player.empty()) ha::want(s.player, a);
  for (const std::string &p : s.speakers)
    for (const char *a : ATTRS) ha::want(p, a);
}
void entity_put(const std::string &e, const std::string &, const std::string &) {
  St &s = S();
  if (s.src != "ha") return;
  if (e == s.player || e == current() || std::find(s.speakers.begin(), s.speakers.end(), e) != s.speakers.end()) rebuild();
}

void fresh() {
  St &s = S();
  if (!tick) tick = lv_timer_create([](lv_timer_t *) {  // the progress moves on once a second while playing
    if (S().src == "ha") rebuild(); else sample_now();
  }, 1000, nullptr);
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  const std::string &cfg = ha::feed_config(E);
  if (cfg != s.cfg_seen) {
    s.cfg_seen = cfg;
    ArduinoJson::JsonDocument c;
    deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
    s.src = c["source"] | "sample";
    s.player = c["player"] | "";
    if (s.player.rfind("media_player.", 0) != 0) s.player.clear();
    s.speakers.clear();
    const std::string l = c["speakers"] | "";
    for (size_t i = 0; i < l.size();) {
      size_t j = l.find(',', i);
      if (j == std::string::npos) j = l.size();
      std::string x = l.substr(i, j - i);
      while (!x.empty() && x[0] == ' ') x.erase(0, 1);
      while (!x.empty() && x.back() == ' ') x.pop_back();
      if (x.rfind("media_player.", 0) == 0 && s.speakers.size() < 12) s.speakers.push_back(x);
      i = j + 1;
    }
    if (s.speakers.empty() && !s.player.empty()) s.speakers.push_back(s.player);  // the player alone, never sample rows
    s.picked.clear(), s.follow_last.clear();
    s.follow = true;
    s.sheet = "closed";
    if (s.src == "ha") want_all();
  }
  if (s.src == "ha") rebuild(); else sample_now();
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.media never reach a host
}  // namespace

// a tap on the Media screen (see the header); svc / ent / data = a media_player service to call ("" = none: done here)
const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data) {
  St &s = S();
  svc->clear(), data->clear();
  const bool ha = s.src == "ha";
  *ent = ha ? current() : "";
  auto again = [&] { if (ha) rebuild(); else sample_now(); };
  if (v == "speakers" || v == "close") { s.sheet = v == "speakers" ? "open" : "closed"; put("sheet", s.sheet); return ""; }
  if (v.rfind("pick ", 0) == 0) {  // a sheet row by its id or its name (a Picker row's tap says the name)
    std::string id = v.substr(5);
    if (id == "Auto") id = "auto";
    for (int i = 0; i < 3; i++) if (id == SAMPLE_SPK[i][1]) id = SAMPLE_SPK[i][0];
    for (const std::string &e : s.speakers) if (id == name_of(e)) id = e;
    s.sheet = "closed";
    if (id == "auto") s.follow = true, s.follow_last.clear();
    else if (ha) {
      if (std::find(s.speakers.begin(), s.speakers.end(), id) == s.speakers.end()) return "not one of the speakers";
      s.follow = false, s.picked = id;
      for (const char *a : ATTRS) ha::want(id, a);
    } else {
      for (int i = 0; i < 3; i++) if (id == SAMPLE_SPK[i][0]) s.spk = i, s.follow = false;
    }
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
