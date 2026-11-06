// Report a problem (Settings > Report a problem): how to send a bug report from the panel. A screen spec of real
// components only, built by the spec renderer (core/spec.h: Page > Header + list Body, C1-C3), laid out as Settings:
//   header   the Header (back + "Report a problem" + the screen chrome)
//   body     "on your phone"  one Text body dim line (what the code does), then the QR block: a new GitHub Bug report
//                             (bug.yml) with this panel's version + board filled in
//            "this panel"     read-only List rows (act none): Version, Board, Report (the address of its problem
//                             report page, http://<its address>/fw/report: logs kept across restarts, connection drops,
//                             the last crash; no secrets) or "Needs Wi-Fi"
//   The spec follows the address while the page shows (a 1 s timer compares the link page's address).
// The firmware tells it the board (report::set_board); the version is the installed firmware's (core/device.h update).
// Props (the workbench): version, board, page (the panel's address as its link page, http://<address>/link) stand in
// for the live values. Update path: none needed.
#include "fw.h"
#include "device.h"
#include "spec.h"

namespace fw::report_screen {
std::string board_;
void set_board(const std::string &b) { board_ = b; }  // the firmware, at boot (FW_DEVICE_BOARD)
}  // namespace fw::report_screen

namespace {
using Doc = ArduinoJson::JsonDocument;
namespace hl = fw::dev::ha_link;
ArduinoJson::JsonObject node(ArduinoJson::JsonArray n, const char *c) {
  ArduinoJson::JsonObject o = n.add<ArduinoJson::JsonObject>();
  o["c"] = c;
  return o;
}
void group(ArduinoJson::JsonArray n, const char *title) {
  ArduinoJson::JsonObject p = node(n, "section_header")["p"].to<ArduinoJson::JsonObject>();
  p["title"] = title, p["size"] = "label", p["badge"] = false, p["right"] = "none";
}
ArduinoJson::JsonObject row(ArduinoJson::JsonArray n, const char *title) {
  ArduinoJson::JsonObject p = node(n, "list_row")["p"].to<ArduinoJson::JsonObject>();
  p["title"] = title, p["icon"] = "none", p["act"] = "none";
  return p;
}
std::string enc(const std::string &s) {  // a URL query value
  static const char *H = "0123456789ABCDEF";
  std::string o;
  for (unsigned char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char) c;
    else o += '%', o += H[c >> 4], o += H[c & 15];
  }
  return o;
}
struct Facts { std::string version, board, page; };
Facts facts(fw::Props p) {
  Facts f;
  f.version = p["version"] | "";
  f.board = p["board"] | "";
  f.page = p["page"] | "";
  if (f.version.empty()) f.version = fw::dev::update_now().installed;
  if (f.board.empty()) f.board = fw::report_screen::board_;
  if (f.page.empty()) f.page = hl::page();
  return f;
}
// http://<address>/link -> http://<address>/fw/report ("" = not on Wi-Fi yet)
std::string report_of(const std::string &page) {
  const size_t a = page.find("://");
  if (page.empty() || a == std::string::npos) return "";
  return page.substr(0, page.find('/', a + 3)) + "/fw/report";
}
std::string spec_json(const Facts &f) {
  Doc sp;
  sp["v"] = 1, sp["id"] = "report", sp["name"] = "Report a problem", sp["layout"] = "list";
  sp["header"]["label"] = "Report a problem";
  ArduinoJson::JsonArray n = sp["nodes"].to<ArduinoJson::JsonArray>();
  const std::string rep = report_of(f.page);
  // the Bug report form (.github/ISSUE_TEMPLATE/bug.yml): its version field and report field filled in
  std::string url = "https://github.com/mrdarrengriffin/facet/issues/new?template=bug.yml";
  std::string ver = f.version.empty() ? std::string("unknown") : f.version;
  if (!f.board.empty()) ver += " on " + f.board;
  url += "&version=" + enc(ver);  // the report's address stays on the screen (a longer code is too fine to scan at 480)
  group(n, "on your phone");
  ArduinoJson::JsonObject x = node(n, "text")["p"].to<ArduinoJson::JsonObject>();
  x["text"] = "Scan to open a bug report with this panel's details filled in, then paste in its report.";
  x["style"] = "body", x["tone"] = "dim", x["lines"] = 3;
  ArduinoJson::JsonObject q = node(n, "qr_block")["p"].to<ArduinoJson::JsonObject>();
  q["text"] = url.c_str(), q["caption"] = "open on your phone";
  group(n, "this panel");
  ArduinoJson::JsonObject v = row(n, "Version");
  v["trailing"] = "value", v["value"] = f.version.empty() ? "Unknown" : f.version.c_str();
  ArduinoJson::JsonObject b = row(n, "Board");
  b["trailing"] = "value", b["value"] = f.board.empty() ? "Unknown" : f.board.c_str();
  ArduinoJson::JsonObject r = row(n, "Report");
  r["trailing"] = "value", r["value"] = rep.empty() ? "Needs Wi-Fi" : rep.c_str();
  std::string out;
  serializeJson(sp, out);
  return out;
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > Report a problem: a QR code that opens a new GitHub bug report with this panel's version and board filled in, and the address of the panel's problem report (logs kept across restarts, connection drops, the last crash; no secrets).",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "version", "about": "The firmware version shown and filled in (empty = the panel's own).", "type": "text", "default": ""},
    {"name": "board", "about": "The board shown and filled in (empty = the panel's own).", "type": "text", "default": ""},
    {"name": "page", "about": "The panel's address as its link page (empty = the panel's own: http://<its address>/link).", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "on Wi-Fi", "props": {"version": "0.5.0-beta.7", "board": "guition-4848s040", "page": "http://192.168.1.42/link"}},
    {"label": "no Wi-Fi yet", "props": {"version": "0.5.0-beta.7", "board": "waveshare-p4-86"}}
  ]
})";

void mount(lv_obj_t *parent, int32_t at, const Facts &f, bool live) {
  Doc none;
  std::string error;
  lv_obj_t *root = fw::spec::build(parent, "report", none.as<fw::Props>(), &error, spec_json(f).c_str());
  if (!root) return;
  if (at >= 0) lv_obj_move_to_index(root, at);
  if (!live) return;
  fx::every(root, 1000, [parent, root, f] {  // the address comes once the panel is on Wi-Fi
    if (hl::page() == f.page) return;
    const int32_t at = lv_obj_get_index(root);
    Facts g = f;
    g.page = hl::page();
    lv_obj_delete(root);
    mount(parent, at, g, true);
  });
}
void build_page(lv_obj_t *parent, fw::Props p) {
  const bool live = (p["page"] | "")[0] == 0;
  mount(parent, -1, facts(p), live);
}
const int reg = fw::add({"report", "Report a problem", "screens", META, build_page});
}  // namespace

FW_UPDATE(report) { return false; }
