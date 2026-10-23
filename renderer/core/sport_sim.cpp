// Sport simulators (sport.h): seeded, repeatable stand-ins for live adapters. Each plans the whole event at make()
// and feed(t) replays it at any sim time, emitting the SAME feed shape a real adapter does (sports-data.md), with
// "source": "sim" and "sample": true so every screen can say so. Sample people and clubs: the F1 grid is the 2026 one
// (codes, numbers, team colours as OpenF1 lists them, 2026-10-22); tennis players and football clubs are made up.
#include "sport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace fw::sport {
namespace {
struct Rng {  // splitmix64: the same seed = the same event, on every device
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  float uni() { return (next() >> 40) / 16777216.0f; }                 // 0..1
  float range(float a, float b) { return a + (b - a) * uni(); }
  int irange(int a, int b) { return a + (int) (uni() * (b - a + 1) - 1e-6f); }  // a..b inclusive
  float gauss() { float u = std::max(1e-6f, uni()), v = uni(); return std::sqrt(-2 * std::log(u)) * std::cos(6.2831853f * v); }
};

// ======================================================================== race
struct Driver { const char *code; int num; const char *team, *colour; float skill; };
const Driver GRID[] = {
  {"NOR", 1, "McLaren", "#F47600", 0.00f}, {"PIA", 81, "McLaren", "#F47600", 0.05f},
  {"VER", 3, "Red Bull Racing", "#4781D7", 0.02f}, {"RUS", 63, "Mercedes", "#00D7B6", 0.10f},
  {"LEC", 16, "Ferrari", "#ED1131", 0.15f}, {"HAM", 44, "Ferrari", "#ED1131", 0.25f},
  {"ANT", 12, "Mercedes", "#00D7B6", 0.30f}, {"HAD", 6, "Red Bull Racing", "#4781D7", 0.45f},
  {"ALB", 23, "Williams", "#1868DB", 0.55f}, {"SAI", 55, "Williams", "#1868DB", 0.55f},
  {"ALO", 14, "Aston Martin", "#229971", 0.60f}, {"GAS", 10, "Alpine", "#00A1E8", 0.65f},
  {"LAW", 30, "Racing Bulls", "#6C98FF", 0.70f}, {"LIN", 41, "Racing Bulls", "#6C98FF", 0.80f},
  {"OCO", 31, "Haas F1 Team", "#9C9FA2", 0.75f}, {"BEA", 87, "Haas F1 Team", "#9C9FA2", 0.75f},
  {"HUL", 27, "Audi", "#F50537", 0.85f}, {"BOR", 5, "Audi", "#F50537", 0.90f},
  {"STR", 18, "Aston Martin", "#229971", 0.95f}, {"COL", 43, "Alpine", "#00A1E8", 1.00f},
  {"PER", 11, "Cadillac", "#909090", 1.20f}, {"BOT", 77, "Cadillac", "#909090", 1.25f},
};
constexpr int GRID_N = sizeof GRID / sizeof GRID[0];
constexpr float PRE = 30, PIT_LOSS = 20, DT_LOSS = 18, PIT_AT = 0.96f;  // lights out; stop / drive-through loss; the lane
constexpr float FORMATION = 22;  // the formation lap ends; the start (grid, lights) until PRE

// a stop in the pit lane on lap `lap` (entered at PIT_AT of that lap): tyres (a pit stop) or a drive-through
struct Stop { int lap; float loss; bool tyres; };
struct RCar {
  const Driver *d;
  float grid;                 // lap fraction behind the line at the start
  std::vector<float> end;     // end[l] = time car crosses the line completing lap l (end[0] = lights out)
  std::vector<float> laptime; // laptime[l] (l >= 1)
  std::vector<Stop> stops;
  int dnf = -1;               // lap it stops on, at dnf_at
  float dnf_at = 0;
  int finish = -1;            // lap it takes the flag on
  float dsq_t = -1;           // black flag (disqualified) from this time
};
// race control: what the stewards / race director do, planned as times on the leader's clock
struct RcMsg { float t; std::string cat, flag, text; int sector = 0, car = -1; };
struct Window { float t0 = -1, t1 = -1; };
struct Pen { int car; float t_inv = -1, t_pen = -1, t_served = -1; const char *kind; };
struct Race : SimBase {
  const Circuit *c;
  int N = 0;
  float base = 90;
  int sc_lap = -1, sc_len = 3, vsc_lap = -1, red_lap = -1;
  Window sc, sc2, vsc, red, y1, y2;
  int y1_sec = 1, y2_sec = 2;
  float red_len = 150;
  std::vector<RCar> cars;
  std::vector<RcMsg> rc;
  std::vector<Pen> pens;
  int bw_car = -1;
  float bw_t = -1;
  float t_flag = 0, t_end = 0, best = 1e9f, best_t = -1;
  int best_car = -1;

  // sim time -> race time (the red flag stops the race clock: cars hold where they are, in the pit lane)
  float race_t(float t) const { return red.t0 < 0 || t < red.t0 ? t : t < red.t0 + red_len ? red.t0 : t - red_len; }
  float wall_t(float rt) const { return red.t0 < 0 || rt <= red.t0 ? rt : rt + red_len; }

  Race(uint32_t seed, const char *track, int n) {
    Rng r(seed);
    c = circuit(track);
    if (!c) c = &CIRCUITS[0];
    N = std::max(12, (int) std::lround(305000.0f / c->length_m));
    if (!strcmp(c->id, "mc-1929")) N = 78;
    base = c->length_m / 62.0f + 8;
    n = std::max(1, std::min(n, GRID_N));
    std::vector<std::pair<float, int>> q;
    for (int i = 0; i < n; i++) q.push_back({GRID[i].skill + r.range(-0.25f, 0.25f), i});
    std::sort(q.begin(), q.end());
    // the plan: every kind of race-control moment happens once, at its own part of the race
    vsc_lap = std::max(3, N * 22 / 100), sc_lap = N * 33 / 100, red_lap = N * 55 / 100;
    const int pit_lo = N * 38 / 100, pit_hi = N * 62 / 100;
    for (int g = 0; g < n; g++) {
      RCar k;
      k.d = &GRID[q[g].second];
      k.grid = (g * 8.0f + 4) / c->length_m;
      int pl = r.irange(pit_lo, pit_hi);
      if (std::abs(pl - sc_lap) <= 4 && r.uni() < 0.7f) pl = sc_lap;  // a cheap stop under the safety car
      k.stops.push_back({pl, PIT_LOSS, true});
      cars.push_back(k);
    }
    if (n > 6) {  // a retirement and a black flag, from the back half
      RCar &k = cars[r.irange(n / 2, n - 1)];
      k.dnf = N * 48 / 100, k.dnf_at = r.range(0.2f, 0.8f);
    }
    for (auto &k : cars) k.end.assign(N + 2, 1e9f), k.laptime.assign(N + 2, 0), k.end[0] = PRE;
    std::vector<float> pace(n);
    for (int i = 0; i < n; i++) pace[i] = cars[i].d->skill * 0.6f + r.range(-0.1f, 0.1f);
    // the drive-through (a penalty served in the lane) is planned before the laps are timed
    const int dt_car = n > 4 ? r.irange(3, n - 2) : -1, dt_lap = N * 45 / 100;
    if (dt_car >= 0) cars[dt_car].stops.push_back({dt_lap, DT_LOSS, false});
    const int restart_lap = red_lap + 1;  // after the red flag: one lap behind the safety car
    for (int l = 1; l <= N + 1; l++) {
      const bool under_sc = (l >= sc_lap && l < sc_lap + sc_len) || l == restart_lap;
      const bool under_vsc = l == vsc_lap;
      std::vector<int> ord(n);
      for (int i = 0; i < n; i++) ord[i] = i;
      std::sort(ord.begin(), ord.end(), [&](int a, int b) { return cars[a].end[l - 1] < cars[b].end[l - 1]; });
      float prev = -1;
      for (int i : ord) {
        RCar &k = cars[i];
        if (k.dnf == l) k.laptime[l] = base + pace[i];
        if (k.end[l - 1] >= 1e8f || (k.dnf > 0 && l >= k.dnf)) continue;
        int last_tyres = 0;
        float stop = 0;
        for (auto &s : k.stops) { if (s.tyres && s.lap < l) last_tyres = s.lap; if (s.lap == l) stop += s.loss; }
        float lt = base + pace[i] + 0.3f * r.gauss() + 0.045f * (l - last_tyres) - 0.02f * l;
        if (l == 1) lt += 5 + k.grid * base * 1.2f + 0.25f * (&k - &cars[0]);
        if (under_vsc) lt *= 1.35f;
        lt += stop;
        if (under_sc) lt = std::max(base * 1.45f, prev > 0 ? prev + 0.8f - k.end[l - 1] : 0.0f) + stop * 0.6f;
        k.end[l] = k.end[l - 1] + lt, k.laptime[l] = lt;
        prev = k.end[l];
        if (!under_sc && !under_vsc && l > 1 && stop == 0 && lt < best) best = lt, best_t = k.end[l], best_car = i;
      }
      if (l == sc_lap) sc.t0 = cars[ord[0]].end[l - 1] + 0.35f * base;
      if (l == sc_lap + sc_len - 1) sc.t1 = cars[ord[0]].end[l];
      if (l == restart_lap) sc2.t1 = cars[ord[0]].end[l];
      if (l == vsc_lap) vsc.t0 = cars[ord[0]].end[l - 1] + 0.2f * base, vsc.t1 = cars[ord[0]].end[l] - 0.1f * base;
      if (l == red_lap) red.t0 = sc2.t0 = cars[ord[0]].end[l - 1] + 0.6f * base;  // restart behind the SC
    }
    // the race clock stops for the red flag: shift everything after it (planned on race time, shown on wall time)
    // (lap ends stay in race time; feed() converts)
    // yellow flags: a single yellow early, a double yellow later, each in a marshal sector of this circuit
    const int ns = std::max(1, (int) c->n_sectors);
    y1_sec = r.irange(1, ns), y2_sec = r.irange(1, ns);
    const RCar *lead = &cars[0];
    for (auto &k : cars) if (k.end[1] < lead->end[1]) lead = &k;
    auto lap_t = [&](int l, float f) { return lead->end[std::max(0, l - 1)] + f * base; };
    y1.t0 = lap_t(std::max(2, N * 10 / 100), 0.3f), y1.t1 = y1.t0 + 55;
    y2.t0 = lap_t(N * 17 / 100, 0.5f), y2.t1 = y2.t0 + 75;
    // stewards: an incident noted, investigated, a 5 s penalty; a 10 s penalty; a drive-through; black and white
    if (n > 8) {
      const int a = r.irange(4, 7);
      pens.push_back({a, lap_t(N * 13 / 100, 0.4f), lap_t(N * 16 / 100, 0.2f), -1, "+5s"});
      pens.push_back({r.irange(8, n - 1), lap_t(N * 28 / 100, 0.6f), lap_t(N * 30 / 100, 0.1f), -1, "+10s"});
      if (dt_car >= 0) pens.push_back({dt_car, lap_t(dt_lap - 4, 0.5f), lap_t(dt_lap - 2, 0.5f), cars[dt_car].end[dt_lap], "DT"});
      bw_car = r.irange(2, n - 1), bw_t = lap_t(N * 26 / 100, 0.7f);
      int bf = r.irange(n / 2, n - 1);
      while (bf == dt_car || cars[bf].dnf > 0) bf = (bf + 1) % n;
      cars[bf].dsq_t = lap_t(N * 70 / 100, 0.5f);
    }
    // the flag: the first car to complete N laps; everyone else finishes on their next crossing
    t_flag = 1e9f;
    for (auto &k : cars) t_flag = std::min(t_flag, k.end[N]);
    t_end = t_flag;
    for (auto &k : cars) {
      if (k.dnf > 0 || k.dsq_t > 0) continue;
      for (int l = 1; l <= N + 1; l++)
        if (k.end[l] >= t_flag - 1e-3f) { k.finish = l; t_end = std::max(t_end, k.end[l]); break; }
    }
    // the messages race control sends (OpenF1 race_control style), in race time
    // stored in WALL time (the red flag stops the race clock, not the messages)
    auto msg = [&](float t, const char *cat, const char *flag, std::string text, int sector = 0, int car = -1, bool wall = false) {
      if (t > 0 && t < 1e8f) rc.push_back({wall ? t : wall_t(t), cat, flag, std::move(text), sector, car});
    };
    char b[160];
    auto who = [&](int i) { snprintf(b, sizeof b, "CAR %d (%s)", cars[i].d->num, cars[i].d->code); return std::string(b); };
    msg(FORMATION - 18, "Other", "", "FORMATION LAP");
    msg(PRE - 0.1f, "Flag", "GREEN", "GREEN LIGHT - PIT EXIT OPEN");
    snprintf(b, sizeof b, "YELLOW IN TRACK SECTOR %d", y1_sec); msg(y1.t0, "Flag", "YELLOW", b, y1_sec);
    snprintf(b, sizeof b, "CLEAR IN TRACK SECTOR %d", y1_sec); msg(y1.t1, "Flag", "CLEAR", b, y1_sec);
    snprintf(b, sizeof b, "DOUBLE YELLOW IN TRACK SECTOR %d", y2_sec); msg(y2.t0, "Flag", "DOUBLE YELLOW", b, y2_sec);
    snprintf(b, sizeof b, "CLEAR IN TRACK SECTOR %d", y2_sec); msg(y2.t1, "Flag", "CLEAR", b, y2_sec);
    msg(vsc.t0, "SafetyCar", "VSC", "VIRTUAL SAFETY CAR DEPLOYED");
    msg(vsc.t1 - 10, "SafetyCar", "VSC_ENDING", "VIRTUAL SAFETY CAR ENDING");
    msg(sc.t0, "SafetyCar", "SC", "SAFETY CAR DEPLOYED");
    msg(sc.t1 - base * 0.8f, "SafetyCar", "SC_ENDING", "SAFETY CAR IN THIS LAP");
    msg(red.t0, "Flag", "RED", "RED FLAG", 0, -1, true);
    msg(red.t0 + red_len * 0.6f, "Other", "", "RACE WILL RESUME BEHIND THE SAFETY CAR", 0, -1, true);
    msg(red.t0 + red_len, "SafetyCar", "SC", "SAFETY CAR DEPLOYED - RESTART", 0, -1, true);
    msg(sc2.t1 - base * 0.8f, "SafetyCar", "SC_ENDING", "SAFETY CAR IN THIS LAP");
    for (auto &p : pens) {
      msg(p.t_inv, "Other", "", who(p.car) + " UNDER INVESTIGATION - CAUSING A COLLISION", 0, p.car);
      const char *what = !strcmp(p.kind, "DT") ? "DRIVE THROUGH PENALTY" : !strcmp(p.kind, "+5s") ? "5 SECOND TIME PENALTY" : "10 SECOND TIME PENALTY";
      msg(p.t_pen, "Other", "", std::string("FIA STEWARDS: ") + what + " FOR " + who(p.car), 0, p.car);
      if (p.t_served > 0) msg(p.t_served, "Other", "", who(p.car) + " PENALTY SERVED", 0, p.car);
    }
    if (bw_car >= 0) msg(bw_t, "Flag", "BLACK AND WHITE", "BLACK AND WHITE FLAG FOR " + who(bw_car) + " - TRACK LIMITS", 0, bw_car);
    for (size_t i = 0; i < cars.size(); i++) {
      if (cars[i].dsq_t > 0) msg(cars[i].dsq_t, "Flag", "BLACK", "BLACK FLAG FOR " + who((int) i) + " - DISQUALIFIED", 0, (int) i);
      if (cars[i].dnf > 0) msg(when(cars[i], cars[i].dnf - 1 + cars[i].dnf_at) + 2, "Other", "", who((int) i) + " STOPPED - RETIRED", 0, (int) i);
    }
    for (int l = 1; l < N; l++) if (best_car >= 0 && std::fabs(cars[best_car].end[l] - best_t) < 1e-3f) {
      snprintf(b, sizeof b, "FASTEST LAP %s %d:%06.3f", cars[best_car].d->code, (int) best / 60, std::fmod(best, 60.0f));
      msg(best_t + 0.5f, "Other", "", b, 0, best_car);
    }
    msg(t_flag, "Flag", "CHEQUERED", "CHEQUERED FLAG");
    std::sort(rc.begin(), rc.end(), [](const RcMsg &a, const RcMsg &b) { return a.t < b.t; });
  }
  const Stop *stop_on(const RCar &k, int l) const {
    for (auto &s : k.stops) if (s.lap == l) return &s;
    return nullptr;
  }
  bool out_of(const RCar &k, float t) const { return k.dsq_t > 0 && t >= k.dsq_t; }
  // laps completed + fraction (negative on the grid); stopped = DNF or after the cool-down
  float dist(const RCar &k, float t, bool *pit, bool *out, bool *done) const {
    *pit = *out = *done = false;
    if (t <= PRE) return -k.grid;
    for (int l = 1; l <= N + 1; l++) {
      if (t >= k.end[l]) {
        if (k.finish == l) {  // took the flag: one slow lap, then park past the line
          *done = true;
          int ahead = 0;
          for (auto &o : cars) ahead += o.finish > 0 && (o.finish > l || (o.finish == l && o.end[l] < k.end[l]));
          return l + std::min(0.32f - 0.006f * ahead, (t - k.end[l]) / (base * 1.6f));
        }
        continue;
      }
      const float t0 = k.end[l - 1], lt = k.laptime[l];
      const float from = l == 1 ? -k.grid : l - 1.0f;
      float u = (t - t0) / lt;
      if (const Stop *s = stop_on(k, l)) {  // normal speed to the lane, stopped / slow, out over the line
        const float drive = lt - s->loss, split = PIT_AT * drive, e = t - t0;
        if (e < split) u = PIT_AT * e / split;
        else if (e < split + s->loss) u = PIT_AT, *pit = true;
        else u = PIT_AT + (1 - PIT_AT) * (e - split - s->loss) / (drive - split);
      }
      if (k.dnf == l && from + u * (l - from) >= l - 1 + k.dnf_at) { *out = true; return l - 1 + k.dnf_at; }
      return from + u * (l - from);
    }
    *out = k.dnf > 0;
    return k.dnf > 0 ? k.dnf - 1 + k.dnf_at : N;
  }
  float when(const RCar &k, float d) const {  // the time k reached distance d (for gaps)
    if (d <= 0) return PRE;
    const int l = std::min(N + 1, (int) std::floor(d) + 1);
    const float f = d - (l - 1), lt = k.laptime[l];
    if (lt <= 0 || k.end[l - 1] >= 1e8f) return 1e9f;
    if (const Stop *s = stop_on(k, l)) {
      const float drive = lt - s->loss;
      return k.end[l - 1] + (f <= PIT_AT ? f * drive : PIT_AT * drive + s->loss + (f - PIT_AT) * drive);
    }
    return k.end[l - 1] + f * lt;
  }
  const char *flag_at(float t) const {
    if (t >= t_flag) return "chequered";
    if (red.t0 > 0 && t >= red.t0 && t < red.t0 + 0.01f) return "red";
    if ((sc.t0 > 0 && t >= sc.t0 && t < sc.t1) || (sc2.t0 > 0 && t >= sc2.t0 && t < sc2.t1))
      return (t >= sc.t1 - base * 0.8f && t < sc.t1) || (t >= sc2.t1 - base * 0.8f && t < sc2.t1) ? "sc_ending" : "sc";
    if (vsc.t0 > 0 && t >= vsc.t0 && t < vsc.t1) return t >= vsc.t1 - 10 ? "vsc_ending" : "vsc";
    if (y2.t0 > 0 && t >= y2.t0 && t < y2.t1) return "double_yellow";
    if (y1.t0 > 0 && t >= y1.t0 && t < y1.t1) return "yellow";
    return "green";
  }
  void feed(ArduinoJson::JsonDocument &o, float wall) override {
    const bool red_now = red.t0 > 0 && wall >= red.t0 && wall < red.t0 + red_len;
    const float t = race_t(wall);
    const int n = (int) cars.size();
    struct Row { int i; float d; bool pit, out, done; };
    std::vector<Row> rows(n);
    for (int i = 0; i < n; i++) {
      rows[i].i = i, rows[i].d = dist(cars[i], t, &rows[i].pit, &rows[i].out, &rows[i].done);
      if (out_of(cars[i], t)) rows[i].out = true;
    }
    std::sort(rows.begin(), rows.end(), [&](const Row &a, const Row &b) {
      if (a.out != b.out) return b.out;
      const float da = a.done ? cars[a.i].finish : a.d, db = b.done ? cars[b.i].finish : b.d;
      if (a.done && b.done && cars[a.i].finish == cars[b.i].finish) return cars[a.i].end[cars[a.i].finish] < cars[b.i].end[cars[b.i].finish];
      if (std::fabs(da - db) < 1e-6f) return a.done;
      return da > db;
    });
    const RCar &lead = cars[rows[0].i];
    const float lead_d = rows[0].done ? (float) lead.finish : rows[0].d;
    const int lap = std::max(0, std::min(N, (int) std::floor(rows[0].d) + 1));
    o["kind"] = "f1_race", o["source"] = "sim", o["sample"] = true, o["track"] = c->id;
    auto s = o["session"].to<ArduinoJson::JsonObject>();
    s["name"] = "Race", s["laps"] = N, s["lap"] = t <= PRE ? 0 : rows[0].done ? N : lap;
    s["state"] = wall < FORMATION ? "formation" : t <= PRE ? "start" : red_now ? "suspended" : t >= t_end + 10 ? "final" : "live";
    s["flag"] = red_now ? "red" : t <= PRE ? "green" : flag_at(t);
    if (lap == N && t < t_flag && t > PRE) s["last_lap"] = true;
    auto w = s["weather"].to<ArduinoJson::JsonObject>();
    w["air"] = 31, w["track"] = 44, w["rain"] = false;
    // marshal sectors under yellow
    auto sec = o["sectors"].to<ArduinoJson::JsonArray>();
    if (y1.t0 > 0 && t >= y1.t0 && t < y1.t1) { auto x = sec.add<ArduinoJson::JsonObject>(); x["n"] = y1_sec, x["flag"] = "yellow"; }
    if (y2.t0 > 0 && t >= y2.t0 && t < y2.t1) { auto x = sec.add<ArduinoJson::JsonObject>(); x["n"] = y2_sec, x["flag"] = "double_yellow"; }
    // the safety car: just ahead of the leader while it is out
    const char *fl = s["flag"] | "green";
    if (!strncmp(fl, "sc", 2)) o["sc_at"] = std::round(std::fmod(rows[0].d + 0.012f + 10, 1.0f) * 10000) / 10000.0f;
    auto arr = o["cars"].to<ArduinoJson::JsonArray>();
    float ahead_gap = 0;
    for (int p = 0; p < n; p++) {
      const Row &rw = rows[p];
      const RCar &k = cars[rw.i];
      auto x = arr.add<ArduinoJson::JsonObject>();
      x["code"] = k.d->code, x["num"] = k.d->num, x["team"] = k.d->team, x["colour"] = k.d->colour;
      x["pos"] = p + 1;
      x["at"] = std::round((rw.d - std::floor(rw.d)) * 10000) / 10000.0f;
      int stints = 0;
      for (auto &st : k.stops) if (st.tyres && t > k.end[st.lap - 1] + (k.laptime[st.lap] - st.loss) * PIT_AT + st.loss * 0.5f) stints++;
      x["tyre"] = stints ? "H" : "M";
      if (rw.pit || red_now) x["pit"] = true;
      if (rw.out) x["out"] = true;
      if (out_of(k, t)) x["dsq"] = true;
      if (best_car == rw.i && t >= best_t) x["fl"] = true;
      auto flags = x["flags"].to<ArduinoJson::JsonArray>();
      if (bw_car == rw.i && t >= bw_t && t < bw_t + 90) flags.add("bw");
      if (out_of(k, t) && t < k.dsq_t + 60) flags.add("black");
      for (auto &pn : pens)
        if (pn.car == rw.i) {
          if (t >= pn.t_inv && (pn.t_pen < 0 || t < pn.t_pen)) x["inv"] = true;
          if (pn.t_pen > 0 && t >= pn.t_pen && (pn.t_served < 0 || t < pn.t_served)) x["penalty"] = pn.kind;
          if (pn.t_pen > 0 && t >= pn.t_pen && strcmp(pn.kind, "DT")) x["penalty"] = pn.kind;  // time penalties stay to the end
        }
      float gap = 0;
      int down = 0;
      if (p > 0 && !rw.out) {
        down = (int) std::floor(lead_d - (rw.done ? (float) k.finish : rw.d) + 1e-4f);
        gap = rw.done ? k.end[k.finish] - lead.end[lead.finish] : t - when(lead, rw.d);
        if (rw.done) down = std::max(0, lead.finish - k.finish);
        if (t <= PRE) gap = 0, down = 0;
        if (down > 0) x["laps_down"] = down;
        else x["gap"] = std::round(gap * 1000) / 1000.0f;
        const float iv = std::max(0.0f, gap - ahead_gap);
        if (down == 0) x["int"] = std::round(iv * 1000) / 1000.0f;
        // overtake aid: within a second of the car ahead, green running, from lap 3 (DRS to 2025, overtake mode 2026)
        if (down == 0 && iv < 1.0f && lap >= 3 && !strcmp(fl, "green") && !rw.pit) x["assist"] = true;
      }
      // blue flag: a car about to be lapped (one coming up a lap ahead, within ~1.5 s)
      if (!rw.out && !rw.done && t > PRE)
        for (const Row &o2 : rows)
          if (!o2.out && !o2.done && o2.d - rw.d > 0.97f && o2.d - rw.d < 1.0f) { flags.add("blue"); break; }
      if (flags.size() == 0) x.remove("flags");
      if (p > 0 && !rw.out && down == 0) ahead_gap = gap;
    }
    // race control: the last few messages (newest last)
    auto msgs = o["race_control"].to<ArduinoJson::JsonArray>();
    int first = 0, last = 0;
    for (int i = 0; i < (int) rc.size(); i++) if (rc[i].t <= wall) last = i + 1;
    first = std::max(0, last - 4);
    for (int i = first; i < last; i++) {
      auto m = msgs.add<ArduinoJson::JsonObject>();
      m["t"] = (int) std::lround(rc[i].t * 10) / 10.0f;
      m["category"] = rc[i].cat, m["message"] = rc[i].text;
      if (!rc[i].flag.empty()) m["flag"] = rc[i].flag;
      if (rc[i].sector) m["sector"] = rc[i].sector;
      if (rc[i].car >= 0) m["driver"] = cars[rc[i].car].d->num;
    }
  }
  float duration() override { return wall_t(t_end) + 15; }
  float moment(const char *m) override {
    auto W = [&](float rt) { return rt > 0 && rt < 1e8f ? wall_t(rt) : -1; };
    if (!strcmp(m, "formation")) return 0;
    if (!strcmp(m, "start")) return PRE + 0.5f;  // just after lights out: a demo shows the field moving at once
    if (!strcmp(m, "yellow")) return W(y1.t0 - 4);
    if (!strcmp(m, "double yellow")) return W(y2.t0 - 4);
    if (!strcmp(m, "vsc")) return W(vsc.t0 - 5);
    if (!strcmp(m, "safety car")) return W(sc.t0 - 5);
    if (!strcmp(m, "red flag")) return red.t0 > 0 ? red.t0 - 5 : -1;
    if (!strcmp(m, "restart")) return red.t0 > 0 ? red.t0 + red_len - 5 : -1;
    if (!strcmp(m, "penalty")) return pens.empty() ? -1 : W(pens[0].t_inv - 3);
    if (!strcmp(m, "black and white")) return W(bw_t - 3);
    if (!strcmp(m, "final lap")) {
      float f = 1e9f;
      for (auto &k : cars) f = std::min(f, k.end[N - 1]);
      return W(f - 5);
    }
    if (!strcmp(m, "chequered")) return W(t_flag - 6);
    if (!strcmp(m, "fastest lap")) return W(best_t - 4);
    if (!strcmp(m, "pit stop")) {
      float f = 1e9f;
      for (auto &k : cars) for (auto &s : k.stops) if (s.tyres) f = std::min(f, k.end[s.lap - 1] + (k.laptime[s.lap] - s.loss) * PIT_AT);
      return W(f - 4);
    }
    if (!strcmp(m, "retirement")) {
      float f = 1e9f;
      for (auto &k : cars) if (k.dnf > 0) f = std::min(f, when(k, k.dnf - 1 + k.dnf_at));
      return f < 1e8f ? W(f - 4) : -1;
    }
    if (!strcmp(m, "blue flag")) {  // the first lapping
      for (float t = PRE + base * 5; t < t_flag; t += 2) {
        for (auto &a : cars) for (auto &b : cars) {
          bool p, o, d;
          const float da = dist(a, t, &p, &o, &d), db = dist(b, t, &p, &o, &d);
          if (da - db > 0.97f && da - db < 1.0f) return W(t - 3);
        }
      }
      return -1;
    }
    if (!strcmp(m, "overtake")) {  // the first change of order on track after lap 2 (not in the pits)
      const int n = (int) cars.size();
      std::vector<int> prev;
      for (float t = PRE + 2.2f * base; t < t_flag; t += 0.5f) {
        std::vector<std::pair<float, int>> d;
        bool pit_any = false;
        for (int i = 0; i < n; i++) {
          bool p, o, dn;
          d.push_back({-dist(cars[i], t, &p, &o, &dn), i});
          pit_any = pit_any || p || o;
        }
        std::sort(d.begin(), d.end());
        std::vector<int> ord;
        for (auto &e : d) ord.push_back(e.second);
        if (!prev.empty() && ord != prev && !pit_any) return W(t - 4);
        prev = ord;
      }
      return -1;
    }
    return -1;
  }
};

// ======================================================================== tennis
struct TennisPt { float t; uint8_t won; };
struct Tennis : SimBase {
  int best_of;
  char rule;
  std::vector<TennisPt> pts;
  float t_end = 0;
  const char *names[2][3] = {{"Mara Lindqvist", "LIN", "SWE"}, {"Ayla Moreno", "MOR", "ESP"}};
  int seeds[2] = {3, 0};
  struct S {  // replay state
    std::vector<std::array<int, 2>> sets{{0, 0}};
    int p[2] = {0, 0}, sets_won[2] = {0, 0}, server = 0, tb_first = 0, winner = -1;
    bool tb = false;
    int tb_to = 7;
  };
  Tennis(uint32_t seed, int bo, char fs) : best_of(bo == 5 ? 5 : 3), rule(fs) {
    Rng r(seed);
    const float hold[2] = {r.range(0.60f, 0.70f), r.range(0.60f, 0.70f)};
    S s;
    float t = 20;
    while (s.winner < 0 && pts.size() < 600) {
      const int srv = serving(s);
      const uint8_t w = r.uni() < hold[srv] ? srv : 1 - srv;
      const int g0 = s.sets.back()[0] + s.sets.back()[1], n_sets = (int) s.sets.size();
      t += r.range(22, 48);
      pts.push_back({t, w});
      step(s, w);
      if (s.winner >= 0) break;
      const int g1 = s.sets.back()[0] + s.sets.back()[1];
      if ((int) s.sets.size() != n_sets) t += 120;               // set break
      else if (g1 != g0) t += g1 % 2 == 1 ? 90 : 25;             // changeover after odd games
    }
    t_end = t;
  }
  static int serving(const S &s) {
    if (!s.tb) return s.server;
    const int k = s.p[0] + s.p[1];  // tiebreak: one point, then two each
    return k == 0 ? s.tb_first : (((k - 1) / 2) % 2 == 0 ? 1 - s.tb_first : s.tb_first);
  }
  void step(S &s, int w) const {
    s.p[w]++;
    const bool fin = (int) s.sets.size() == best_of;
    if (!tennis_game_won(s.p[w], s.p[1 - w], s.tb, s.tb_to)) return;
    auto &g = s.sets.back();
    g[w]++;
    s.p[0] = s.p[1] = 0;
    const bool was_tb = s.tb;
    s.tb = false;
    s.server = was_tb ? 1 - s.tb_first : 1 - s.server;  // after a tiebreak: the first receiver serves
    if (tennis_set_won(g[w], g[1 - w], fin, rule)) {
      if (++s.sets_won[w] > best_of / 2) { s.winner = w; return; }
      s.sets.push_back({0, 0});
      return;
    }
    if (tennis_tiebreak_at(g[0], g[1], fin, rule, &s.tb_to)) s.tb = true, s.tb_first = s.server;
  }
  int upto(float t) const {
    int n = 0;
    while (n < (int) pts.size() && pts[n].t <= t) n++;
    return n;
  }
  void write(ArduinoJson::JsonDocument &o, int n, float t) const {
    S s;
    for (int i = 0; i < n; i++) step(s, pts[i].won);
    o["kind"] = "tennis_match", o["source"] = "sim", o["sample"] = true, o["event"] = "Sample Open · Final";
    const float next = n < (int) pts.size() ? pts[n].t : 1e9f, last = n ? pts[n - 1].t : 0;
    o["state"] = t < 20 ? "upcoming" : s.winner >= 0 ? "final" : next - last > 60 && t - last > 15 ? "break" : "live";
    auto f = o["format"].to<ArduinoJson::JsonObject>();
    f["best_of"] = best_of, f["final_set"] = rule == 'A' ? "adv" : rule == '7' ? "tb7" : "tb10";
    auto pl = o["players"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < 2; i++) {
      auto x = pl.add<ArduinoJson::JsonObject>();
      x["name"] = names[i][0], x["short"] = names[i][1], x["country"] = names[i][2];
      if (seeds[i]) x["seed"] = seeds[i];
    }
    auto ss = o["sets"].to<ArduinoJson::JsonArray>();
    for (auto &g : s.sets) { auto a = ss.add<ArduinoJson::JsonArray>(); a.add(g[0]), a.add(g[1]); }
    if (s.winner >= 0) o["winner"] = s.winner;
    else {
      auto p = o["points"].to<ArduinoJson::JsonArray>();
      p.add(s.p[0]), p.add(s.p[1]);
      o["server"] = serving(s);
    }
  }
  void feed(ArduinoJson::JsonDocument &o, float t) override { write(o, upto(t), t); }
  float duration() override { return t_end; }
  float moment(const char *m) override {
    if (!strcmp(m, "start")) return pts.empty() ? 18 : pts[0].t - 0.5f;  // just before the first point lands
    const bool brk = !strcmp(m, "break");
    for (int n = 0; n < (int) pts.size(); n++) {
      ArduinoJson::JsonDocument d;
      write(d, n, pts[n].t - 1);
      const TennisNow now = tennis_now(d.as<ArduinoJson::JsonVariantConst>());
      const bool hit = !strcmp(m, "break point") ? now.break_point : !strcmp(m, "set point") ? now.set_point
                     : !strcmp(m, "match point") ? now.match_point : !strcmp(m, "tiebreak") ? now.tiebreak
                     : brk ? (now.break_point && pts[n].won == now.game_point) : false;
      if (hit) return (n ? pts[n - 1].t : 20) + 1;
    }
    return -1;
  }
};

// ======================================================================== match
struct Player { int num; const char *name; char pos; };
const char *SURN[] = {"Okafor", "Lindgren", "Varga", "Castell", "Brennan", "Haddad", "Novak", "Silva", "Achterberg",
                      "Mensah", "Duarte", "Kowal", "Reyes", "Fontaine", "Ibsen", "Morrow", "Tanaka", "Quinn",
                      "Ferreira", "Holt", "Adeyemi", "Sauer", "Marin", "Price", "Lund", "Costa", "Byrne", "Moretti",
                      "Petrov", "Ward", "Nkosi", "Albers", "Kerr", "Rocha"};
struct MEvent { float min; int added, team; const char *type; std::string player, detail; };
struct Match : SimBase {
  struct Team { const char *name, *shrt, *colour, *formation; std::vector<std::string> names; std::vector<std::string> grid; std::vector<int> nums; };
  Team tm[2] = {{"Harbour City", "HBC", "#1B3A6B", "4-3-3", {}, {}, {}}, {"Ashford Rovers", "ASH", "#F2F2F2", "4-2-3-1", {}, {}, {}}};
  std::vector<MEvent> ev;
  int add1 = 2, add2 = 4;
  std::vector<float> poss;  // per minute, home %
  static constexpr float PRE = 20, HT = 15 * 60;
  Match(uint32_t seed) {
    Rng r(seed);
    int si = r.irange(0, 33);
    for (int k = 0; k < 2; k++) {
      Team &T = tm[k];
      std::vector<int> rows;  // players per row after the keeper
      for (const char *f = T.formation; *f; f++) if (*f >= '0' && *f <= '9') rows.push_back(*f - '0');
      T.grid.push_back("1:1");
      for (size_t ri = 0; ri < rows.size(); ri++)
        for (int ci = 1; ci <= rows[ri]; ci++) T.grid.push_back(std::to_string(ri + 2) + ":" + std::to_string(ci));
      for (int i = 0; i < 16; i++) T.names.push_back(SURN[(si + i * 7 + k * 3) % 34]), T.nums.push_back(i < 11 ? (i == 0 ? 1 : i + 1) : 12 + i);
    }
    add1 = r.irange(1, 4), add2 = r.irange(2, 7);
    auto minute = [&](int half) { return half == 1 ? r.range(1, 45 + add1 * 0.9f) : r.range(46, 90 + add2 * 0.9f); };
    for (int k = 0; k < 2; k++) {
      const int goals = (int) std::floor(-std::log(std::max(1e-3f, r.uni())) * 1.3f);  // ~Poisson-ish 0..4
      for (int g = 0; g < std::min(goals, 4); g++) ev.push_back({minute(r.uni() < 0.45f ? 1 : 2), 0, k, "goal", tm[k].names[r.irange(6, 10)], ""});
      for (int y = 0; y < r.irange(1, 3); y++) ev.push_back({minute(r.uni() < 0.4f ? 1 : 2), 0, k, "yellow", tm[k].names[r.irange(1, 10)], ""});
      if (r.uni() < 0.12f) ev.push_back({minute(2), 0, k, "red", tm[k].names[r.irange(1, 10)], ""});
      std::vector<int> off;
      for (int s = 0; s < 5; s++) {
        int o;
        do o = r.irange(1, 10); while (std::find(off.begin(), off.end(), o) != off.end());
        off.push_back(o);
        const float m = r.range(56, 86);
        ev.push_back({m, 0, k, "sub", tm[k].names[11 + s], tm[k].names[o]});
      }
    }
    std::sort(ev.begin(), ev.end(), [](const MEvent &a, const MEvent &b) { return a.min < b.min; });
    float p = 50;
    for (int i = 0; i < 100; i++) poss.push_back(p), p = std::max(35.0f, std::min(65.0f, p + r.gauss() * 1.5f));
  }
  // sim time <-> match minute (real time at 1x: 45 + added, 15 min half time, 45 + added)
  float t_of(float min) const {  // min = elapsed match minutes (first half 0..45+add1, second 45..90+add2)
    return min <= 45 + add1 ? PRE + min * 60 : PRE + (45 + add1) * 60 + HT + (min - 45) * 60;
  }
  float t_end() const { return t_of(90 + add2); }
  void feed(ArduinoJson::JsonDocument &o, float t) override {
    o["kind"] = "football_match", o["source"] = "sim", o["sample"] = true, o["comp"] = "Sample League";
    const float h1 = PRE + (45 + add1) * 60, h2 = h1 + HT;
    float min;
    int period;
    const char *state;
    if (t < PRE) state = "upcoming", min = 0, period = 0;
    else if (t < h1) state = "live", min = (t - PRE) / 60, period = 1;
    else if (t < h2) state = "half_time", min = 45 + add1, period = 1;
    else if (t < t_end()) state = "live", min = 45 + (t - h2) / 60, period = 2;
    else state = "final", min = 90 + add2, period = 2;
    o["state"] = state;
    if (t < PRE) o["starts_in"] = (int) std::ceil(PRE - t);
    auto clk = o["clock"].to<ArduinoJson::JsonObject>();
    const int m = (int) std::floor(min) + (period ? 1 : 0);  // the minute in play (1st minute = 1')
    const int cap = period == 1 ? 45 : 90;
    clk["min"] = std::min(m, cap), clk["period"] = period;
    if (m > cap && !strcmp(state, "live")) clk["added"] = m - cap;
    clk["added_total"] = period == 1 ? add1 : (min > 89.9f ? add2 : 0);
    auto teams = o["teams"].to<ArduinoJson::JsonArray>();
    int score[2] = {0, 0};
    for (auto &e : ev) if (e.min <= min && period && !strcmp(e.type, "goal")) score[e.team]++;
    const int pm = std::max(0, std::min(99, (int) min));
    for (int k = 0; k < 2; k++) {
      const Team &T = tm[k];
      auto x = teams.add<ArduinoJson::JsonObject>();
      x["name"] = T.name, x["short"] = T.shrt, x["colour"] = T.colour, x["score"] = score[k], x["formation"] = T.formation;
      if (period) x["possession"] = (int) std::lround(k == 0 ? poss[pm] : 100 - poss[pm]);
      auto lu = x["lineup"].to<ArduinoJson::JsonArray>();
      std::vector<std::string> on(T.names.begin(), T.names.begin() + 11);
      std::vector<int> nums(T.nums.begin(), T.nums.begin() + 11);
      std::vector<bool> sent(11, false);
      for (auto &e : ev) {
        if (e.team != k || e.min > min || !period) continue;
        for (int i = 0; i < 11; i++) {
          if (!strcmp(e.type, "sub") && on[i] == e.detail) {
            on[i] = e.player;
            nums[i] = T.nums[std::find(T.names.begin(), T.names.end(), e.player) - T.names.begin()];
          }
          if (!strcmp(e.type, "red") && on[i] == e.player) sent[i] = true;
        }
      }
      for (int i = 0; i < 11; i++) {
        if (sent[i]) continue;
        auto pl = lu.add<ArduinoJson::JsonObject>();
        pl["num"] = nums[i], pl["name"] = on[i], pl["grid"] = T.grid[i], pl["pos"] = i == 0 ? "G" : "";
      }
    }
    auto evs = o["events"].to<ArduinoJson::JsonArray>();
    for (auto &e : ev) {
      if (!period || e.min > min) continue;
      auto x = evs.add<ArduinoJson::JsonObject>();
      const int em = (int) std::floor(e.min) + 1;
      const int ecap = e.min < 45 + add1 && e.min >= 45 ? 45 : e.min >= 90 ? 90 : 0;
      x["min"] = ecap ? ecap : em;
      if (ecap) x["added"] = em - ecap;
      x["type"] = e.type, x["team"] = e.team, x["player"] = e.player;
      if (!e.detail.empty()) x["off"] = e.detail;
    }
  }
  float duration() override { return t_end() + 10; }
  float moment(const char *m) override {
    if (!strcmp(m, "kickoff")) return PRE - 2;
    if (!strcmp(m, "start")) return PRE + 1;  // just after kickoff: the clock is running
    if (!strcmp(m, "half time")) return t_of(45 + add1) - 6;
    if (!strcmp(m, "added time")) return t_of(90) - 6;
    if (!strcmp(m, "full time")) return t_end() - 6;
    for (auto &e : ev)
      if ((!strcmp(m, "goal") && !strcmp(e.type, "goal")) || (!strcmp(m, "card") && (!strcmp(e.type, "yellow") || !strcmp(e.type, "red")))
          || (!strcmp(m, "sub") && !strcmp(e.type, "sub")))
        return t_of(e.min) - 5;
    return -1;
  }
};
}  // namespace

SimBase *make_race(uint32_t seed, const char *track, int cars) { return new Race(seed, track, cars); }
SimBase *make_tennis(uint32_t seed, int best_of, char final_set) { return new Tennis(seed, best_of, final_set); }
SimBase *make_match(uint32_t seed) { return new Match(seed); }
}  // namespace fw::sport
