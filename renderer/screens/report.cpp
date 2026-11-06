// Report a problem (Settings > Report a problem): how to send a bug report from the panel. A screen spec of real
// components only, built by the spec renderer (core/spec.h: Page > Header + list Body, C1-C3), laid out as Settings:
//   header   the Header (back + "Report a problem" + the screen chrome)
//   body     "problem report"  one Text body dim line (what to do), then a read-only List row "Report page" with the
//                              panel's own report page (http://<its address>/fw/report, firmware/report.html: the
//                              steps, Copy report, Download report, Open a GitHub issue). Not on Wi-Fi yet: no row,
//                              the line says so.
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
  x["text"] = rep.empty() ? "To report a problem, connect this panel to Wi-Fi first: its report page needs it."
                          : "To report a problem, open this address on a computer on the same Wi-Fi. It has the report and the steps.";
  if (!rep.empty()) {  // the panel's own report page, as a settings row to read and type
    ArduinoJson::JsonObject a = row(n, "Report page");
    a["trailing"] = "value", a["value"] = rep.c_str();
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
  "about": "Settings > Report a problem: the address of the panel's problem report page, to open on a computer on the same Wi-Fi. That page has the steps, the report (logs kept across restarts, connection drops, the last crash; no secrets) to copy or download, and a GitHub issue with the version and board filled in.",
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
