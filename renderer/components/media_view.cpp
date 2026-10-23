// Media view: the Media screen's body (docs screens/media.ts mediaScreen; the factory spec renderer/specs/media.json
// adds the header and binds a media_player): the disc with the track beside it, then the controls and a live volume
// slider.
//   top      the real Media disc (size 256 = lay::disc_screen) turning while it plays, sp::s9 after it the track:
//            state (type label, text-dim: playing / paused / idle), the title (headline, text, up to two lines), the
//            artist (body, text-dim)
//   controls real Icon buttons: round prev, play (style play: the play / pause glyph), round next, centred, sp::s9
//            apart, sp::s10 under the disc
//   volume   "vol" (label, text-dim, lay::m_vol_label wide), a real Slider the width left, the value (label, text)
// Feed (docs MediaState): props state (playing / paused / idle / off), title, artist, progress (0-100 of the track),
// volume (0-100): an HA media_player.* entity's state and attributes (media_title, media_artist, volume_level), or
// the sample. Taps: prev / play / next and the volume (live while dragging: at most every 200 ms + a trailing send,
// the final one on release; the docs volumeControl) emit "media" (previous | play_pause | next | volume 42) for the
// host, and show it at once (play / pause flips the state, held 3 s against updates).
//   visual   prop visual: disc (the Media disc) or cassette / cd / vinyl (the real Media visuals component in the
//            disc's place, lay::mv_h tall, title / artist / progress / playing passed on; docs MediaOptions.visual)
//   speaker  on a spec screen: the Speaker button component in the screen's Header in place of the time (docs header
//            right [spk], clock false); a tap opens the speakers sheet (Bottom sheet, its picker rows: Auto, the
//            speakers, the TV, a group; the docs story until a host lists the house's players), a pick shows on the
//            button and emits "speaker" <name>. The sheet covers the whole screen (the view bleeds up to it).
//            Real players (2026-10-23): prop speakers = a comma list of media_player entity ids (the host's: spec
//            param $speakers) lists those instead (no Auto row): each row its friendly name and state (+ the title
//            while it plays) from the HA store, read when the sheet opens; prop player = the bound entity (spec
//            $entity) = the selected row and the button's name; a pick emits "speaker" <entity id>, the spec's
//            screen.param re-binds the screen to it (now playing + controls follow, update path).
// Auto (prop mode = auto | manual, "" = none: the spec's screen.param "auto"): the sheet's first row is Auto (follows
// what's playing: HA's media target), the button reads "auto · <player>" while it follows; a pick of Auto emits
// "speaker" "auto".
// Progress (2026-10-23): props position + duration (seconds) + position_at (HA media_position_updated_at, ISO time):
// while playing the arc moves on locally once a second from that moment (the wall clock); paused = where HA left it;
// no duration = prop progress.
// Actions: the spec's media.control turns "media" into media_player.media_previous_track / media_play_pause /
// media_next_track / volume_set (core/spec.h). Not ported yet: progress moving on between updates; Auto.
// Update path: texts in place, the disc / slider / play glyph through their own updates. Contrast: "media"
// (+ media_disc, icon_button, slider, text).
// audit deps: icon_button, media_disc, media_visual, slider, speaker_button, bottom_sheet
#include "fw.h"
#include "spec.h"

namespace {
struct MV {
  std::string visual = "disc";
  lv_obj_t *spk = nullptr, *sheet = nullptr;
  int speaker = 0;
  std::string player;                // the bound media_player (prop player)
  std::vector<std::string> spk_ids;  // the host's players (prop speakers), empty = the sample list
  std::string mode;                  // auto | manual | "" (no Auto row)
  double pos = NAN, dur = NAN;       // seconds (HA media_position / media_duration)
  double pos_at = NAN;               // the epoch second pos was true at (media_position_updated_at)
  lv_timer_t *ticker = nullptr;      // moves the progress on while playing
  lv_obj_t *disc = nullptr, *state = nullptr, *title = nullptr, *artist = nullptr, *play = nullptr, *slider = nullptr, *vlab = nullptr;
  std::string st = "playing", title_s, artist_s;
  int progress = 38, volume = 42;
  uint32_t hold_until = 0, last_send = 0;
  int sent = -1, pending_v = -1;
  lv_timer_t *trail = nullptr;
};
MV *of(lv_obj_t *o) { return (MV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
std::string spk_name(const MV *m, int i);
bool held(const MV *m) { return (int32_t) (m->hold_until - lv_tick_get()) > 0; }
int vol_track_dp() {  // the slider's track: the width less the two labels, their gutters and a knob
  return (int) std::lround((fw::lay::content_w - 2 * fw::lay::m_vol_label - 2 * fw::lay::gutter - fw::touch::min / 2) / fw::D);
}
int live_progress(const MV *m) {
  if (!(m->dur > 0)) return -1;
  double p = std::isfinite(m->pos) ? m->pos : 0;
  const double now = (double) time(nullptr);
  if (m->st == "playing" && std::isfinite(m->pos_at) && now > 1e9) p += now - m->pos_at;  // a set clock only
  return (int) std::lround(LV_CLAMP(0.0, p, m->dur) / m->dur * 100);
}
// the disc / visual: progress (moved on while playing) + playing (+ the track for a visual)
void push_progress(lv_obj_t *root) {
  MV *m = of(root);
  if (!m->disc) return;
  const int lp = live_progress(m);
  ArduinoJson::JsonDocument dp;
  dp["progress"] = lp >= 0 ? lp : m->progress, dp["playing"] = m->st == "playing";
  if (m->visual == "disc") dp["size"] = "256";
  else dp["visual"] = m->visual, dp["title"] = m->title_s, dp["artist"] = m->artist_s;
  fw::update_child(fw::find(m->visual == "disc" ? "media_disc" : "media_visual"), m->disc, dp.as<fw::Props>());
}
void render(lv_obj_t *root) {
  MV *m = of(root);
  const bool playing = m->st == "playing";
  const bool known = m->st == "off" || m->st == "paused" || playing;  // on / standby / buffering / unavailable: idle
  fw::set_text(m->state, fw::ty::label, known ? m->st.c_str() : "idle");
  fw::set_text(m->title, fw::ty::headline, m->title_s.empty() ? "Nothing playing" : m->title_s.c_str());
  fw::set_text(m->artist, fw::ty::body, m->artist_s.c_str());
  push_progress(root);
  if (m->spk) {  // the speaker icon: accent while playing
    ArduinoJson::JsonDocument bp;
    bp["name"] = spk_name(m, m->speaker), bp["playing"] = playing;
    fw::update_child(fw::find("speaker_button"), m->spk, bp.as<fw::Props>());
  }
  ArduinoJson::JsonDocument pp;
  pp["style"] = "play", pp["icon"] = "play", pp["state"] = playing ? "active" : "default";
  fw::update_child(fw::find("icon_button"), m->play, pp.as<fw::Props>());
  char b[8];
  snprintf(b, sizeof b, "%d", m->volume);
  fw::set_text(m->vlab, fw::ty::label, b);
  ArduinoJson::JsonDocument sp;
  sp["value"] = m->volume, sp["width"] = vol_track_dp();
  fw::update_child(fw::find("slider"), m->slider, sp.as<fw::Props>());
}
void send_volume(lv_obj_t *root, int v) {
  MV *m = of(root);
  if (m->trail) { lv_timer_delete(m->trail); m->trail = nullptr; }
  m->hold_until = lv_tick_get() + 2500;
  if (v == m->sent) return;
  m->sent = v;
  m->last_send = lv_tick_get();
  char b[24];
  snprintf(b, sizeof b, "volume %d", v);
  fw::emit(root, "media", b);
}
void sync_speakers(lv_obj_t *root, fw::Props p);
// "2026-10-23T09:50:12.123456+00:00" -> epoch seconds (NAN when not a time)
double iso_epoch(const char *s) {
  int Y, M, D, h, m;
  double sec;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d:%lf", &Y, &M, &D, &h, &m, &sec) != 6) return NAN;
  const int y = Y - (M <= 2), era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;  // days from civil
  const int doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  double t = (double) (era * 146097 + doe - 719468) * 86400 + h * 3600 + m * 60 + sec;
  const char *z = s + 19;
  while (*z && *z != '+' && *z != '-' && *z != 'Z') z++;
  int oh = 0, om = 0;
  if ((*z == '+' || *z == '-') && sscanf(z + 1, "%d:%d", &oh, &om) >= 1) t -= (*z == '+' ? 1 : -1) * (oh * 3600 + om * 60);
  return t;
}
// the progress now (0-100): HA's position moved on by the time since it was true while playing; -1 = no duration
int live_progress(const struct MV *m);
// HA's "None" / unknown text = no value (docs clean())
std::string clean(const char *v) {
  if (!v || !strcmp(v, "None") || !strcmp(v, "unknown") || !strcmp(v, "unavailable")) return "";
  return v;
}
void take(lv_obj_t *root, fw::Props p) {
  MV *m = of(root);
  const std::string was = m->player;
  sync_speakers(root, p);
  if (m->player != was) { m->hold_until = 0; m->sent = -1; }  // another player: what it says, at once
  if (!held(m)) {
    m->st = p["state"] | "playing";
    m->volume = p["volume"] | 42;
  }
  m->title_s = clean(p["title"] | "Harbour Lights");
  m->artist_s = clean(p["artist"] | "The Lanterns");
  m->progress = p["progress"] | 38;
  m->pos = p["position"].is<double>() ? p["position"].as<double>() : NAN;
  m->dur = p["duration"].is<double>() ? p["duration"].as<double>() : NAN;
  m->pos_at = iso_epoch(p["position_at"] | "");
  const std::string mode = p["mode"] | "";
  m->mode = mode == "auto" || mode == "manual" ? mode : "";
  if (!m->spk_ids.empty() && m->mode == "auto") m->speaker = -2;  // Auto selected
  render(root);
}
// the speakers (docs demo-data players; bottom_sheet's own Speakers list): icon, name, status
const char *const SPEAKERS[][3] = {{"auto", "Auto", "follow what's playing"}, {"speaker", "Kitchen speaker", "playing \xC2\xB7 Northbound"},
                                   {"tv", "Living room TV", "idle"}, {"speaker", "Study", "paused"}, {"speakers", "Downstairs", "group \xC2\xB7 3 speakers"}};
constexpr int N_SAMPLE = sizeof SPEAKERS / sizeof SPEAKERS[0];
std::string friendly(const std::string &e) {
  const std::string *n = fw::ha::get(e, "friendly_name");
  return n && !n->empty() ? *n : e;
}
std::string spk_name(const MV *m, int i) {  // the button: "auto · study" for Auto (it follows what's playing), else the speaker
  std::string s;
  if (!m->spk_ids.empty() && m->mode == "auto") s = "auto \xC2\xB7 " + friendly(m->player);
  else if (!m->spk_ids.empty()) s = i >= 0 && i < (int) m->spk_ids.size() ? friendly(m->spk_ids[i]) : friendly(m->player);
  else s = i == 0 ? "auto \xC2\xB7 study" : SPEAKERS[i][1];
  for (char &c : s) c = (char) tolower((unsigned char) c);
  return s;
}
// props speakers / player -> the list, the selected row; wants their names, states and titles from the host
void sync_speakers(lv_obj_t *root, fw::Props p) {
  MV *m = of(root);
  m->player = p["player"] | "";
  m->spk_ids.clear();
  const std::string l = p["speakers"] | "";
  for (size_t i = 0; i < l.size();) {
    size_t j = l.find(',', i);
    if (j == std::string::npos) j = l.size();
    std::string id = l.substr(i, j - i);
    while (!id.empty() && id[0] == ' ') id.erase(0, 1);
    if (id.rfind("media_player.", 0) == 0 && m->spk_ids.size() < 12) m->spk_ids.push_back(id);
    i = j + 1;
  }
  if (m->spk_ids.empty()) return;
  m->speaker = -1;
  for (size_t i = 0; i < m->spk_ids.size(); i++) {
    if (m->spk_ids[i] == m->player) m->speaker = (int) i;
    for (const char *a : {"", "friendly_name", "media_title"}) fw::ha::want(m->spk_ids[i], a);
  }
}
void sheet_props(const MV *m, bool open, ArduinoJson::JsonDocument &d) {
  const int off = m->spk_ids.empty() || m->mode.empty() ? 0 : 1;  // the Auto row first
  d["overlay"] = true, d["title"] = "speaker", d["open"] = open ? "open" : "closed";
  d["selected"] = off && m->mode == "auto" ? 0 : m->speaker < 0 ? 0 : m->speaker + off;
  ArduinoJson::JsonArray items = d["items"].to<ArduinoJson::JsonArray>();
  if (off) {
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    it["icon"] = "auto", it["name"] = "Auto";
    it["status"] = m->mode == "auto" ? "follows what's playing \xC2\xB7 " + friendly(m->player) : std::string("follows what's playing");
  }
  if (m->spk_ids.empty()) {
    for (auto &s : SPEAKERS) { ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>(); it["icon"] = s[0], it["name"] = s[1], it["status"] = s[2]; }
    return;
  }
  for (const std::string &e : m->spk_ids) {  // the host's players: name, state (+ the title while it plays)
    ArduinoJson::JsonObject it = items.add<ArduinoJson::JsonObject>();
    const std::string *st = fw::ha::get(e, "");
    std::string status = st && !st->empty() ? *st : "unknown";
    const std::string title = clean(fw::ha::get(e, "media_title") ? fw::ha::get(e, "media_title")->c_str() : "");
    if (status == "playing" && !title.empty()) status += " \xC2\xB7 " + title;
    it["icon"] = "speaker", it["name"] = friendly(e), it["status"] = status;
  }
}
void speaker_header(lv_obj_t *root) {
  lv_obj_t *hdr = fw::spec::building_header();
  if (!hdr) return;
  MV *m = of(root);
  // docs header clock: false: the button takes the time's place
  if (lv_obj_get_child_count(hdr)) lv_obj_add_flag(lv_obj_get_child(hdr, -1), LV_OBJ_FLAG_HIDDEN);
  ArduinoJson::JsonDocument bp;
  bp["name"] = spk_name(m, m->speaker), bp["playing"] = m->st == "playing";
  m->spk = build("speaker_button", hdr, bp);
  if (!m->spk) return;
  ArduinoJson::JsonDocument sd;
  sheet_props(m, false, sd);
  m->sheet = build("bottom_sheet", root, sd);
  if (m->sheet) {
    lv_obj_set_pos(m->sheet, -fw::lay::inset, -fw::lay::top);
    lv_obj_set_size(m->sheet, fw::lay::screen_w, fw::lay::screen_h);
    fw::qa::free(root, "overlay: hosts the speakers sheet, which covers the whole screen");
    fw::bleed(root, fw::lay::top);
    lv_obj_add_event_cb(m->sheet, [](lv_event_t *e) {  // a speaker picked
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      MV *m = of(root);
      int i = (int) (intptr_t) lv_event_get_param(e);
      const int off = m->spk_ids.empty() || m->mode.empty() ? 0 : 1;
      if (off && i == 0) {  // Auto: the screen follows the target again
        m->mode = "auto";
        ArduinoJson::JsonDocument bp;
        bp["name"] = spk_name(m, m->speaker), bp["playing"] = m->st == "playing";
        fw::update_child(fw::find("speaker_button"), m->spk, bp.as<fw::Props>());
        fw::emit(root, "speaker", "auto");
        return;
      }
      i -= off;
      if (i < 0 || i >= (m->spk_ids.empty() ? N_SAMPLE : (int) m->spk_ids.size())) return;
      m->speaker = i;
      if (off) m->mode = "manual";
      ArduinoJson::JsonDocument bp;
      bp["name"] = spk_name(m, i), bp["playing"] = m->st == "playing";
      fw::update_child(fw::find("speaker_button"), m->spk, bp.as<fw::Props>());
      fw::emit(root, "speaker", m->spk_ids.empty() ? SPEAKERS[i][1] : m->spk_ids[i].c_str());
    }, LV_EVENT_VALUE_CHANGED, root);
  }
  lv_obj_add_event_cb(m->spk, [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    MV *m = of(root);
    if (!m->sheet || lv_obj_has_state(m->spk, fw::st::OFF)) return;
    ArduinoJson::JsonDocument sd;
    sheet_props(m, true, sd);
    fw::update_child(fw::find("bottom_sheet"), m->sheet, sd.as<fw::Props>());
  }, LV_EVENT_CLICKED, root);
}
}  // namespace

FW_COMPONENT(media_view, "Media view", "components", R"({
  "about": "The Media screen's body: the disc with the track beside it, the controls and a live volume slider.",
  "frame": [480, 400],
  "props": [
    {"name": "state", "type": "select", "options": ["playing", "paused", "idle", "off"], "default": "playing"},
    {"name": "title", "type": "text", "default": "Harbour Lights"},
    {"name": "artist", "type": "text", "default": "The Lanterns"},
    {"name": "progress", "type": "range", "min": 0, "max": 100, "step": 1, "default": 38},
    {"name": "volume", "type": "range", "min": 0, "max": 100, "step": 1, "default": 42},
    {"name": "visual", "type": "select", "options": ["disc", "cassette", "cd", "vinyl"], "default": "disc"},
    {"name": "player", "type": "text", "default": ""},
    {"name": "speakers", "type": "text", "default": ""},
    {"name": "mode", "type": "select", "options": ["", "auto", "manual"], "default": ""},
    {"name": "position", "type": "range", "min": 0, "max": 86400, "step": 1, "default": 0},
    {"name": "duration", "type": "range", "min": 0, "max": 86400, "step": 1, "default": 0},
    {"name": "position_at", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "playing", "props": {}},
    {"label": "paused · a long title", "props": {"state": "paused", "title": "Paper Lanterns on a Quiet Evening by the Harbour", "artist": "Marlow & Finch"}},
    {"label": "cassette", "props": {"visual": "cassette", "progress": 30}},
    {"label": "CD", "props": {"visual": "cd", "progress": 45}},
    {"label": "nothing playing", "props": {"state": "idle", "title": "", "artist": "", "progress": 0}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *m = new MV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, m);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *m = (MV *) lv_event_get_user_data(e);
    if (m->trail) lv_timer_delete(m->trail);
    if (m->ticker) lv_timer_delete(m->ticker);
    delete m;
  }, LV_EVENT_DELETE, m);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s10, 0);
  // top: disc + track
  lv_obj_t *top = box(root);
  lv_obj_set_size(top, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(top, fw::sp::s9, 0);
  const char *vis = p["visual"] | "disc";
  m->visual = !strcmp(vis, "cassette") || !strcmp(vis, "cd") || !strcmp(vis, "vinyl") ? vis : "disc";
  ArduinoJson::JsonDocument dp;
  dp["progress"] = p["progress"] | 38, dp["playing"] = !strcmp(p["state"] | "playing", "playing");
  if (m->visual == "disc") dp["size"] = "256";
  else dp["visual"] = m->visual, dp["title"] = p["title"] | "Harbour Lights", dp["artist"] = p["artist"] | "The Lanterns";
  m->disc = build(m->visual == "disc" ? "media_disc" : "media_visual", top, dp);
  lv_obj_update_layout(top);
  const int cw = fw::lay::content_w - (m->disc ? lv_obj_get_width(m->disc) : 0) - fw::sp::s9;
  lv_obj_t *col = box(top);
  lv_obj_set_size(col, cw, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, fw::sp::s2, 0);
  m->state = fw::text(col, fw::ty::label, "", t.text_dim, cw);
  m->title = fw::text(col, fw::ty::headline, "", t.text, cw);
  lv_label_set_long_mode(m->title, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(m->title, 2 * fw::style(fw::ty::headline).box);  // up to two lines (docs: clamp 2)
  lv_obj_set_style_max_height(m->title, 2 * fw::style(fw::ty::headline).box, 0);
  m->artist = fw::text(col, fw::ty::body, "", t.text_dim, cw);
  // controls
  lv_obj_t *ctl = box(root);
  lv_obj_set_size(ctl, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ctl, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(ctl, fw::sp::s9, 0);
  static const char *const CMD[3][3] = {{"round", "prev", "previous"}, {"play", "play", "play_pause"}, {"round", "next", "next"}};
  for (auto &c : CMD) {
    ArduinoJson::JsonDocument bp;
    bp["style"] = c[0], bp["icon"] = c[1];
    lv_obj_t *b = build("icon_button", ctl, bp);
    if (!b) continue;
    if (!strcmp(c[1], "play")) m->play = b;
    // the command rides on the event's own user data (an Icon button keeps its state in the object's user data)
    struct Cmd { lv_obj_t *root; const char *cmd; };
    auto *cm = new Cmd{root, c[2]};
    lv_obj_add_event_cb(b, [](lv_event_t *e) { delete (Cmd *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, cm);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *cm = (Cmd *) lv_event_get_user_data(e);
      lv_obj_t *root = cm->root;
      MV *m = of(root);
      const char *cmd = cm->cmd;
      if (!strcmp(cmd, "play_pause")) {  // flips at once, held against updates
        m->st = m->st == "playing" ? "paused" : "playing";
        m->hold_until = lv_tick_get() + 3000;
        render(root);
      }
      fw::emit(root, "media", cmd);
    }, LV_EVENT_CLICKED, cm);
  }
  // volume
  lv_obj_t *vol = box(root);
  lv_obj_set_size(vol, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(vol, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(vol, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  fw::text(vol, fw::ty::label, "vol", t.text_dim, fw::lay::m_vol_label);
  ArduinoJson::JsonDocument sp;
  sp["value"] = p["volume"] | 42, sp["width"] = vol_track_dp();
  m->slider = build("slider", vol, sp);
  m->vlab = fw::text(vol, fw::ty::label, "--", t.text, fw::lay::m_vol_label);
  lv_obj_set_style_text_align(m->vlab, LV_TEXT_ALIGN_RIGHT, 0);
  fw::listen(vol, [](lv_obj_t *, const char *name, const char *detail, void *user) {
    auto *root = (lv_obj_t *) user;
    MV *m = of(root);
    const int v = atoi(detail);
    if (!strcmp(name, "change")) { m->volume = v; send_volume(root, v); render(root); return; }
    if (strcmp(name, "input")) return;
    m->volume = v;  // live: the label follows, a send at most every 200 ms + a trailing one (docs volumeControl)
    m->hold_until = lv_tick_get() + 2500;
    char b[8];
    snprintf(b, sizeof b, "%d", v);
    fw::set_text(m->vlab, fw::ty::label, b);
    const int32_t wait = 200 - (int32_t) (lv_tick_get() - m->last_send);
    if (wait <= 0) { send_volume(root, v); return; }
    m->pending_v = v;
    if (!m->trail) {
      m->trail = lv_timer_create([](lv_timer_t *tm) {
        auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
        MV *m = of(root);
        m->trail = nullptr;
        lv_timer_delete(tm);
        send_volume(root, m->pending_v);
      }, (uint32_t) wait, root);
    }
  }, root);
  speaker_header(root);
  take(root, p);
  m->ticker = lv_timer_create([](lv_timer_t *tm) {  // the progress moves on while playing (once a second)
    auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
    MV *m = of(root);
    if (m->st == "playing" && m->dur > 0) push_progress(root);
  }, 1000, root);
}

FW_UPDATE(media_view) {
  MV *m = of(root);
  if (!m) return false;
  const char *vis = p["visual"] | "disc";
  if (strcmp(vis, m->visual.c_str()) && (!strcmp(vis, "disc") || !strcmp(vis, "cassette") || !strcmp(vis, "cd") || !strcmp(vis, "vinyl")))
    return false;  // another visual: the view again (snap)
  take(root, p);
  return true;
}
