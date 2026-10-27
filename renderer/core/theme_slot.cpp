// The panel's look + its one sent-theme slot: core/theme_slot.h (theme engine T4).
#include "theme_slot.h"
#include <string.h>
#include "device.h"
#include "fx.h"
#include "theme_file.h"

namespace fx {
namespace {
struct Slot {
  SlotIO io;
  std::string json;           // the last-good sent theme ("" = none)
  std::string id, label;      // its id + label (from the load that accepted it)
  std::string boot = "none";  // what the boot restore found
  std::string applied;        // the look made live last ("" = none yet)
  uint32_t applied_gen = 0;   // the generation it was made live at (another apply since = select again)
  std::string why;            // why the last select showed Facet ("" = it showed what was asked)
};
Slot &S() {
  static Slot s;
  return s;
}
uint32_t crc32(const char *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint8_t) p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}
void errors(ArduinoJson::JsonObject o, const std::vector<ThemeError> &err) {
  auto a = o["errors"].to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < err.size() && i < 4; i++) {
    auto e = a.add<ArduinoJson::JsonObject>();
    e["path"] = err[i].path; e["msg"] = err[i].msg; e["line"] = err[i].line;
  }
  o["n_errors"] = (int) err.size();
}
// a load within the budgets: the parser's own checks, then the memory it held
std::shared_ptr<const Skin> load(const char *json, size_t n, std::vector<ThemeError> &err) {
  err.clear();
  if (n > SENT_MAX) { err.push_back({"", std::to_string(n) + " bytes: a sent theme is at most " + std::to_string(SENT_MAX / 1024) + " KB", 0}); return nullptr; }
  auto s = theme_load(json, n, err);
  if (s && theme_peak_bytes() > PEAK_MAX) {
    err.push_back({"", "loading it needs " + std::to_string(theme_peak_bytes()) + " bytes, a panel allows " + std::to_string(PEAK_MAX), 0});
    return nullptr;
  }
  return err.empty() ? s : nullptr;
}
// a record -> its JSON, or why it is bad
bool unpack(const std::string &rec, std::string &json, std::string &why) {
  if (rec.size() > SLOT_MAX) { why = "corrupt: " + std::to_string(rec.size()) + " bytes, more than the slot holds"; return false; }
  if (rec.size() < 8 || memcmp(rec.data(), "FXT1", 4)) { why = "corrupt: not a theme record"; return false; }
  const uint8_t *c = (const uint8_t *) rec.data() + 4;
  const uint32_t want = c[0] | (c[1] << 8) | (c[2] << 16) | ((uint32_t) c[3] << 24);
  if (crc32(rec.data() + 8, rec.size() - 8) != want) { why = "corrupt: checksum"; return false; }
  json.assign(rec, 8, std::string::npos);
  return true;
}
}  // namespace

std::string theme_record(const std::string &json) {
  const uint32_t c = crc32(json.data(), json.size());
  std::string r = "FXT1";
  for (int i = 0; i < 4; i++) r += (char) ((c >> (8 * i)) & 0xFF);
  return r + json;
}

bool theme_select(const std::string &look, std::string &why) {
  Slot &s = S();
  why.clear();
  if (look == s.applied && theme_generation() == s.applied_gen) { why = s.why; return why.empty(); }
  std::vector<ThemeError> err;
  if (look.empty() || look == "facet") theme_reset();
  else if (look == "sent") {
    if (s.json.empty()) why = "no sent theme on this panel";
    else if (!theme_apply(s.json.data(), s.json.size(), err)) why = "the sent theme was refused: " + (err.empty() ? std::string("?") : err[0].path + " " + err[0].msg);
  } else {
    int f = -1;
    for (int i = 0; i < theme_builtins(); i++) if (look == theme_builtin_id(i)) f = i;
    if (f < 0) why = "no built-in theme \"" + look + "\"";
    else if (!theme_apply(theme_builtin_json(f), strlen(theme_builtin_json(f)), err)) why = "refused: " + (err.empty() ? std::string("?") : err[0].path + " " + err[0].msg);
  }
  if (!why.empty() && strcmp(theme_live()->id, "facet")) theme_reset();  // anything that cannot show = Facet
  restyle();
  s.applied = look; s.applied_gen = theme_generation(); s.why = why;
  return why.empty();
}

void theme_send(const char *json, size_t n, ArduinoJson::JsonObject out) {
  Slot &s = S();
  if (!json) n = 0;
  if (n == 0) {  // drop the sent theme
    s.json.clear(); s.id.clear(); s.label.clear();
    if (s.io.erase) s.io.erase();
    const char *look = fw::dev::setting("look");
    if (look && !strcmp(look, "sent")) fw::dev::set("look", "facet", fw::dev::HOST);
    out["ok"] = true; out["dropped"] = true;
  } else {
    std::vector<ThemeError> err;
    auto sk = load(json, n, err);
    out["ok"] = (bool) sk;
    out["bytes"] = (int) n;
    errors(out, err);
    if (sk) {
      const bool same = s.json.size() == n && !memcmp(s.json.data(), json, n);
      s.json.assign(json, n); s.id = sk->id; s.label = sk->label;
      out["id"] = s.id; out["label"] = s.label;
      out["stored"] = same || (s.io.save && s.io.save(theme_record(s.json)));  // the same file again = no flash write
      const char *look = fw::dev::setting("look");
      s.applied.clear();  // the slot's content changed: select again
      if (look && !strcmp(look, "sent")) fw::dev::refresh_look();
      else if (look) fw::dev::set("look", "sent", fw::dev::HOST);
      else { std::string why; theme_select("sent", why); }  // no settings in this build (a bare stage)
    }
  }
  out["live"] = std::string(theme_live()->id);
  out["gen"] = theme_generation();
}

void theme_storage(SlotIO io) {
  Slot &s = S();
  s.io = std::move(io);
  s.json.clear(); s.id.clear(); s.label.clear();
  std::string rec, json, why;
  if (!s.io.load || !s.io.load(rec)) s.boot = "none";
  else if (!unpack(rec, json, why)) { s.boot = why; if (s.io.erase) s.io.erase(); }
  else {
    std::vector<ThemeError> err;
    auto sk = load(json.data(), json.size(), err);
    if (!sk) s.boot = "refused: " + (err.empty() ? std::string("?") : err[0].path + " " + err[0].msg);
    else { s.json = json; s.id = sk->id; s.label = sk->label; s.boot = "restored"; }
  }
  s.applied.clear();
  const char *look = fw::dev::setting("look");
  if (look && !strcmp(look, "sent")) fw::dev::refresh_look();
}

void theme_status(ArduinoJson::JsonObject out) {
  const Slot &s = S();
  const char *look = fw::dev::setting("look");
  out["look"] = look ? look : "";
  out["live"] = std::string(theme_live()->id);
  out["gen"] = theme_generation();
  out["why"] = s.why;
  out["boot"] = s.boot;
  if (s.json.empty()) out["sent"] = nullptr;
  else { auto o = out["sent"].to<ArduinoJson::JsonObject>(); o["id"] = s.id; o["label"] = s.label; o["bytes"] = (int) s.json.size(); }
  out["max_bytes"] = (int) SENT_MAX; out["peak_max"] = (int) PEAK_MAX;
}

// fw_lab theme_slot_* (WASM + native tests): a memory store standing in for NVS, so the boot, corrupt and refused paths
// run in the gate exactly as on a panel.
//   theme_slot_mem            a fresh empty store, installed (= first boot)
//   theme_slot_reboot         Facet live, the slot forgotten, the same store installed again (= a reboot)
//   theme_slot_put <json>     write a well-formed record of <json> straight into the store (no checks: a file a newer
//                             firmware accepted)       theme_slot_corrupt crc|magic|short|long   damage the stored record
//   theme_slot_failsave on|   the store refuses writes  theme_slot_send <json>  = facet_theme   theme_slot_status
//   theme_slot_select <look>  theme_select directly (no settings)   theme_slot_stored  {has, bytes, writes}
//   theme_slot_builtin <id>   {ok, json}: a built-in theme file's text
std::string theme_lab_slot(const std::string &c, const std::string &a) {
  static std::string mem;
  static bool has = false, failsave = false;
  static int writes = 0;
  auto io = [] {
    SlotIO io;
    io.load = [](std::string &out) { if (!has) return false; out = mem; return true; };
    io.save = [](const std::string &r) { if (failsave) return false; mem = r; has = true; writes++; return true; };
    io.erase = [] { mem.clear(); has = false; };
    return io;
  };
  ArduinoJson::JsonDocument d;
  auto o = d.to<ArduinoJson::JsonObject>();
  if (c == "theme_slot_mem") { mem.clear(); has = false; failsave = false; writes = 0; theme_reset(); restyle(); theme_storage(io()); theme_status(o); }
  else if (c == "theme_slot_reboot") { theme_reset(); restyle(); theme_storage(io()); theme_status(o); }
  else if (c == "theme_slot_put") { mem = theme_record(a); has = true; o["ok"] = true; }
  else if (c == "theme_slot_corrupt") {
    if (a == "crc" && mem.size() > 9) mem[mem.size() / 2 + 4] ^= 0x20;
    else if (a == "magic" && !mem.empty()) mem[0] = 'X';
    else if (a == "short") mem.resize(mem.size() > 6 ? 6 : mem.size());
    else if (a == "long") mem = theme_record(std::string(SLOT_MAX, ' '));
    has = true; o["ok"] = true;
  } else if (c == "theme_slot_failsave") { failsave = a == "on"; o["ok"] = true; }
  else if (c == "theme_slot_send") theme_send(a.data(), a.size(), o);
  else if (c == "theme_slot_status") theme_status(o);
  else if (c == "theme_slot_select") { std::string why; o["ok"] = theme_select(a, why); o["why"] = why; o["live"] = std::string(theme_live()->id); }
  else if (c == "theme_slot_stored") { o["has"] = has; o["bytes"] = (int) mem.size(); o["writes"] = writes; }
  else if (c == "theme_slot_builtin") {  // a built-in theme's file text (the workbench editor's starting point)
    for (int i = 0; i < theme_builtins(); i++) if (a == theme_builtin_id(i)) o["json"] = theme_builtin_json(i);
    o["ok"] = !o["json"].isNull();
  }
  else return "{\"error\":\"theme_slot_mem | reboot | put | corrupt | failsave | send | status | select | stored\"}";
  std::string out;
  serializeJson(d, out);
  return out;
}
}  // namespace fx
