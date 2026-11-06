// Report a problem (Settings > Report a problem): how to send a bug report from the panel. A screen spec of real
// components only, built by the spec renderer (core/spec.h: Page > Header + list Body, C1-C3), laid out as Settings:
//   header   the Header (back + "Report a problem" + the screen chrome)
//   body     "problem report"  one Text body dim line (what to do), then the QR block: the panel's own report page
//                              (http://<its address>/fw/report, firmware/report.html: Copy report, Download report,
//                              Open a GitHub issue), then that address as a Text line, for a computer on the same Wi-Fi.
//                              Not on Wi-Fi yet: the QR opens a new GitHub Bug report (bug.yml) with this panel's
//                              version + board filled in instead.
//            "this panel"      read-only List rows (act none): Version, Board
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
// http://<address>/link -> http://<address>/fw/report, the report page ("" = not on Wi-Fi yet)
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
  group(n, "problem report");
  ArduinoJson::JsonObject x = node(n, "text")["p"].to<ArduinoJson::JsonObject>();
  x["style"] = "body", x["tone"] = "dim", x["lines"] = 2;
  ArduinoJson::JsonObject q = node(n, "qr_block")["p"].to<ArduinoJson::JsonObject>();
  if (!rep.empty()) {  // the panel's own report page: a short address, a coarse code that scans at 480
    x["text"] = "Scan with your phone, or open this address on a computer on the same Wi-Fi.";
    q["text"] = rep.c_str(), q["caption"] = "open on your phone";
    ArduinoJson::JsonObject a = node(n, "text")["p"].to<ArduinoJson::JsonObject>();  // the address, as typed (not caps)
    a["text"] = rep.c_str(), a["style"] = "body", a["tone"] = "default", a["lines"] = 1;
  } else {  // no Wi-Fi yet: no report page to open; the Bug report form (bug.yml) with the version field filled in
    std::string url = "https://github.com/mrdarrengriffin/facet/issues/new?template=bug.yml";
    std::string ver = f.version.empty() ? std::string("unknown") : f.version;
    if (!f.board.empty()) ver += " on " + f.board;
    url += "&version=" + enc(ver);
    x["text"] = "This panel isn't on Wi-Fi yet, so its report can't be opened. Scan to open a bug report instead.";
    q["text"] = url, q["caption"] = "open on your phone";  // a copy: url ends with this block
  }
  group(n, "this panel");
  ArduinoJson::JsonObject v = row(n, "Version");
  v["trailing"] = "value", v["value"] = f.version.empty() ? "Unknown" : f.version.c_str();
  ArduinoJson::JsonObject b = row(n, "Board");
  b["trailing"] = "value", b["value"] = f.board.empty() ? "Unknown" : f.board.c_str();
  std::string out;
  serializeJson(sp, out);
  return out;
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > Report a problem: a QR code and address that open the panel's problem report page on a phone or computer on the same Wi-Fi, to copy or download the report (logs kept across restarts, connection drops, the last crash; no secrets) and open a GitHub issue with the version and board filled in.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "version", "about": "The firmware version shown and filled in (empty = the panel's own).", "type": "text", "default": ""},
    {"name": "board", "about": "The board shown and filled in (empty = the panel's own).", "type": "text", "default": ""},
    {"name": "page", "about": "The panel's address as its link page (empty = the panel's own: http://<its address>/link).", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "on Wi-Fi", "props": {"version": "0.5.0-beta.7", "board": "guition-4848s040", "page": "http://192.168.1.50/link"}},
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
