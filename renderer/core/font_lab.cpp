// Downloaded theme fonts: workbench + test hooks (WASM only, like core/theme_lab.cpp): fw_lab(-1, "font_<cmd>", arg)
// from core/main.cpp, answers in JSON. The network and the flash are the simulation of core/font_store.h.
//   font_sim                  install the simulation, forget every font in RAM, empty the flash, online
//   font_serve <json>         {url, status, b64, delay}: the response a GET of url gets (b64 = the body, base64)
//   font_offline <1|0>        every GET fails (no network) / not
//   font_reboot               forget every font in RAM (the stored files stay), as a reboot does; looks repaint
//   font_corrupt <url>        flip bytes of url's stored file          font_store  {files}
//   font_status               [{url, family, state, msg, bytes, fetches, sizes, from_store}]
//   font_check <json>         {b64, fallback, need}: the pure file check (font_store check) -> {ok, why, missing}
//   font_which <type>         the font a type token draws now: {px, line_height, facet, download}
#include <string>
#include <vector>
#include "fw.h"
#include "fx.h"
#include "font_store.h"

namespace fx {
std::string type_font_info(const char *type);  // core/fx_core.cpp
namespace {
std::vector<uint8_t> b64(const char *s) {
  static int8_t T[256];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < 256; i++) T[i] = -1;
    const char *A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) T[(uint8_t) A[i]] = (int8_t) i;
    ready = true;
  }
  std::vector<uint8_t> out;
  uint32_t acc = 0;
  int bits = 0;
  for (; s && *s; s++) {
    const int v = T[(uint8_t) *s];
    if (v < 0) continue;
    acc = acc << 6 | (uint32_t) v; bits += 6;
    if (bits >= 8) { bits -= 8; out.push_back((uint8_t) (acc >> bits)); }
  }
  return out;
}
void relook() {
  fx::restyle();
  if (lv_display_t *d = lv_display_get_default())
    if (lv_obj_t *scr = lv_display_get_screen_active(d)) lv_obj_invalidate(scr);
}
}  // namespace

std::string font_lab(const std::string &cmd, const std::string &arg) {
  using namespace fonts;
  ArduinoJson::JsonDocument o;
  if (cmd == "font_sim") { sim_install(); sim_offline(false); sim_store_clear(); reset(); relook(); return "{}"; }
  if (cmd == "font_serve") {
    ArduinoJson::JsonDocument a;
    if (deserializeJson(a, arg)) return "{\"error\":\"bad json\"}";
    std::vector<uint8_t> body = b64(a["b64"] | "");
    if (a["size"].is<int>()) body.assign((size_t) (a["size"] | 0), 0x41);  // a body of that many bytes (the size cap)
    sim_serve(a["url"] | "", a["status"] | 200, std::move(body), a["delay"] | 50);
    return "{}";
  }
  if (cmd == "font_offline") { sim_offline(arg == "1"); return "{}"; }
  if (cmd == "font_reboot") { reset(); relook(); return "{}"; }
  if (cmd == "font_corrupt") { o["ok"] = sim_store_corrupt(arg); }
  else if (cmd == "font_store") { o["files"] = sim_store_files(); }
  else if (cmd == "font_status") {
    std::vector<Status> st;
    status(st);
    static const char *const NAMES[] = {"none", "loading", "waiting", "ready", "refused"};
    auto a = o.to<ArduinoJson::JsonArray>();
    for (auto &s : st) {
      auto r = a.add<ArduinoJson::JsonObject>();
      r["url"] = s.url; r["family"] = s.family; r["state"] = NAMES[s.state]; r["msg"] = s.msg; r["bytes"] = s.bytes;
      r["fetches"] = s.fetches; r["sizes"] = s.sizes; r["from_store"] = s.from_store;
    }
  } else if (cmd == "font_check") {
    ArduinoJson::JsonDocument a;
    if (deserializeJson(a, arg)) return "{\"error\":\"bad json\"}";
    const std::vector<uint8_t> b = b64(a["b64"] | "");
    std::vector<uint32_t> need;
    for (uint32_t c = 0x20; c < 0x7F; c++) need.push_back(c);
    for (auto v : a["need"].as<ArduinoJson::JsonArray>()) need.push_back(v.as<uint32_t>());
    std::string why;
    int miss = 0;
    o["ok"] = check(b.data(), b.size(), need, a["fallback"] | false, why, &miss);
    o["why"] = why; o["missing"] = miss;
  } else if (cmd == "font_which") return type_font_info(arg.c_str());
  else return "{\"error\":\"unknown font command\"}";
  std::string s;
  serializeJson(o, s);
  return s;
}
}  // namespace fx
