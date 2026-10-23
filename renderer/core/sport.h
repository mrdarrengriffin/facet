// Sport foundation (facet-152 / facet-157, WIP 2026-10-22): the shared language of the sport components (f1_circuit,
// f1_car_marker, f1_standings, tennis_score, tennis_court, football_pitch). claude-assistant notes/research/sports-data.md (sources,
// feed shapes) + sports-ux.md (states, moments, hierarchy).
//   data      every component takes ONE feed shape (race / tennis / match, sports-data.md), never a source's own
//             fields; adapters (hub) and the simulators here fill it. Prop "feed" = the feed as an object or JSON text.
//   colour    team colours are data, not tokens: shown only as identity marks (dots, bars, rings), each fitted to 3:1
//             on the colour it sits on (team_ink, on the RGB565 values); names, numbers and gaps are always token text.
//   states    upcoming / live / break / final / delayed / stale: one word + tone each (state_of), the same on every
//             sport; live = the only one that pulses.
//   circuits  the 23 rounds of the 2026 calendar (sport_circuits.cpp, tools/gen_circuits.mjs), fitted to a box,
//             positions as lap fractions (0 = start / finish) so cars glide along the track between feed updates.
//   tennis    the scoring rules (labels 0 / 15 / 30 / 40 / AD, tiebreak at 6-6, the final-set rule, break / set /
//             match points) shared by the score, the court and the simulator.
//   sims      seeded, repeatable race / tennis / match simulators emitting the feed shapes (sport_sim.cpp); a Sim on a
//             component root advances them on a timer (speed 1x / 10x / 60x, jump to a moment) and applies each feed
//             through the component's own update path (silent, like any external update).
#pragma once
#include "fw.h"
#include <string>
#include <vector>

namespace fw::sport {
// ---- circuits
struct Circuit {
  const char *id, *name, *place, *gp;
  uint16_t length_m, w, h, n;  // w / h = the outline's extent in 0..4095 units (the longer one is 4095)
  const uint8_t *pts;          // n x 3 bytes
  int16_t rotation = 0;        // the broadcast map: degrees (y up, counter-clockwise) applied to the north-up outline
  bool calibrated = false;     // start / finish + direction from timing data; false = the survey's own order
  uint8_t n_sectors = 0;       // marshal sectors (yellow flags are per marshal sector)
  const uint16_t *sectors = nullptr;  // their starts, lap fraction x 65535 from the line, race order
};
extern const Circuit CIRCUITS[];
extern const int N_CIRCUITS;
extern const int CIRCUIT_BYTES;
const Circuit *circuit(const char *id);  // nullptr = not in the asset
// a circuit fitted into a w x h box with `pad` px clear on every side: a smooth closed loop (centripetal Catmull-Rom
// through the dataset's points, so every surveyed point stays on the line) + cumulative length for lap fractions
struct Track {
  std::vector<StrokePt> pts;  // closed: pts.back() == pts.front()
  std::vector<float> cum;     // length up to each point
  float len = 0;
  int w = 0, h = 0;
};
// broadcast = turned like the TV / timing map (Circuit::rotation), else north-up (the survey as it lies)
Track fit(const Circuit &c, int w, int h, int pad, bool broadcast = false);
StrokePt point_at(const Track &t, float frac);    // lap fraction (any real, wraps) -> px in the box
float heading_at(const Track &t, float frac);     // direction of travel there, radians (atan2 of the tangent)

// ---- colour
lv_color_t hex(const char *s, lv_color_t fallback);
float contrast(lv_color_t a, lv_color_t b);       // WCAG ratio on the RGB565 values the panel shows
// team colour fitted to `min`:1 on bg: moved towards `toward` (the theme's text) in 1/16 steps until it passes
lv_color_t team_ink(lv_color_t team, lv_color_t bg, lv_color_t toward, float min = 3.0f);

// ---- competitor marks (a car on the circuit, a player on the pitch): a disc in the team colour, centred on its
// point, cut out of what is behind it by a ring of that colour; the code / number is always token text.
//   dot     team disc (lay::sport_marker), behind-colour ring: the quietest, 22 fit a 480 track
//   code    the dot + its three-letter code on a behind-colour tag (labels decide which cars carry one)
//   number  a behind-colour disc (lay::sport_marker_lg) with a team-colour ring and the car / shirt number inside
// lead = a text-colour ring around the disc (leader / selected); pit = the disc hollow (ring only); out = hidden.
enum class Mark : uint8_t { dot, code, number };
Mark mark_of(const char *s);
int mark_size(Mark m);                  // the disc's diameter
struct MarkLook { Mark mark = Mark::dot; lv_color_t team, behind; const char *code = ""; int num = 0;
                  bool lead = false, label = false, pit = false, out = false; };
lv_obj_t *marker(lv_obj_t *parent, const MarkLook &l);  // a box = the disc; the tag is a child (bleeds, flips side)
void marker_set(lv_obj_t *mk, const MarkLook &l, bool fade = false);  // restyle in place (same Mark); fade = the
                                                                     // disc's colours cross-fade (Motion fade), an update
void marker_place(lv_obj_t *mk, float cx, float cy, int box_w);
lv_obj_t *marker_tag(lv_obj_t *mk);  // the tag (a sibling of the mark in its parent; raise it with the mark)
// The same mark drawn straight into a layer (no objects): for a field of many moving marks (a circuit's 22 cars),
// where objects would invalidate 2 areas each per frame and overflow LVGL's 32-area buffer into a full redraw.
// mark_extent = the box the mark (+ its tag when shown) covers at (cx, cy) in the parent's px (tag flips left near
// box_w); draw_mark draws it there (offset = the parent's screen origin). Looks identical to marker() / marker_set().
struct MarkInk { lv_color_t fill, ring; bool tag; int tag_w, tag_h; };
MarkInk mark_ink(const MarkLook &l);  // colours fitted + the tag's size (measured from its text)
lv_area_t mark_extent(const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, bool with_tag);
void mark_boxes(const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, lv_area_t *disc, lv_area_t *tag);
// parts: 1 = the disc, 2 = the tag (when k.tag), 3 = both (draw every disc first, then the tags, so tags stay on top)
void draw_mark(lv_layer_t *layer, const MarkLook &l, const MarkInk &k, float cx, float cy, int box_w, int parts, lv_point_t origin);  // centre on (cx, cy); tag flips left near the right edge

// a score / word that changed lifts into place: from sp::s2 below + transparent to rest over sport-flash (ease-out);
// reduced motion = instant. Small objects only (a digit cell), silent.
void lift_in(lv_obj_t *o);

// ---- states (one word + tone, every sport)
enum class State : uint8_t { upcoming, live, brk, final, delayed, stale };
State state_of(const char *s, bool stale);
const char *state_word(State s);        // "Upcoming", "Live", "Break", "Final", "Delayed", "Stale"
const char *state_tone(State s);        // Status chip tone: idle / ok / info / idle / warn / warn

// ---- feed
// the feed of a component's props: p["feed"] as an object, or its JSON text parsed into doc; null = none given
ArduinoJson::JsonVariantConst feed_of(Props p, ArduinoJson::JsonDocument &doc);

// ---- tennis rules
struct TennisFormat { int best_of = 3; char final_set = 'T'; };  // final set: 'T' tiebreak to 10, '7' to 7, 'A' advantage
TennisFormat tennis_format(ArduinoJson::JsonVariantConst feed);
struct TennisNow {
  int set = 0;                 // index of the set in play (or the last one when final)
  int sets_won[2] = {0, 0};
  int games[2] = {0, 0};       // in the set in play
  int points[2] = {-1, -1};    // in the game / tiebreak in play, -1 = the source has no points
  bool tiebreak = false;
  int tb_to = 7;
  char label[2][4] = {"", ""}; // "0" "15" "30" "40" "AD" or tiebreak numbers; "" = no points
  int game_point = -1;         // player one point from the game (or tiebreak), -1 none
  bool break_point = false;    // game_point is the receiver
  bool set_point = false, match_point = false;  // game_point would win the set / the match
  int server = -1, winner = -1;
};
TennisNow tennis_now(ArduinoJson::JsonVariantConst feed);
bool tennis_game_won(int a, int b, bool tiebreak, int tb_to);  // a won the game / tiebreak at a-b
bool tennis_set_won(int ga, int gb, bool final_set, char rule);  // a won the set at ga-gb (after the game)
bool tennis_tiebreak_at(int ga, int gb, bool final_set, char rule, int *to);

// ---- simulators (sport_sim.cpp): the feed at sim time t (seconds since the start of the sim). Deterministic per
// seed: the whole event is planned at make(), feed() replays it at any t (so speed and jumps are free).
struct SimBase {
  virtual ~SimBase() = default;
  virtual void feed(ArduinoJson::JsonDocument &out, float t) = 0;
  virtual float duration() = 0;                       // t of the end (chequered, match point, full time)
  virtual float moment(const char *name) = 0;         // t a few seconds before the first such moment, -1 none
};
SimBase *make_race(uint32_t seed, const char *track, int cars);
SimBase *make_tennis(uint32_t seed, int best_of, char final_set);
SimBase *make_match(uint32_t seed);
// a running sim on a component root (the root's own object; owned, freed with the root): every `tick` ms of wall
// time it advances sim time by tick x speed, builds the feed and calls apply(root, feed) inside fw::Quiet.
using Apply = void (*)(lv_obj_t *root, ArduinoJson::JsonVariantConst feed);
// key = the props that pick the sim ("race|us-2012|20|7"): the same key keeps the running sim (speed / jump change
// in place), another key restarts it. Returns the feed of t now (for the first build).
void sim_run(lv_obj_t *root, const std::string &key, SimBase *(*make)(void *), void *arg, float speed,
             const char *jump, Apply apply, ArduinoJson::JsonDocument &first);
void sim_stop(lv_obj_t *root);
bool sim_on(lv_obj_t *root);
constexpr uint32_t SIM_TICK_MS = 250;
// the feed a component shows for its props: p["feed"] when given (the sim stops), else with p["sim"] (default on)
// that sport's simulator from the sim props (seed, speed 1x / 10x / 60x, jump; race: track, cars; tennis: best_of,
// final_set), started or kept on the root; null = neither
ArduinoJson::JsonVariantConst f1_race_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply);
ArduinoJson::JsonVariantConst tennis_match_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply);
ArduinoJson::JsonVariantConst football_match_feed(lv_obj_t *root, Props p, ArduinoJson::JsonDocument &doc, Apply apply);  // = a live adapter's rate (OpenF1 location ~3.7 Hz, F1 Sensor 500 ms)
}  // namespace fw::sport
