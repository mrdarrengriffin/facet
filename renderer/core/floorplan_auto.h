// Floor plan generator (Floor plans foundation, tokens/floorplan.json "generate" -> fw::fp::gen::): a vacuum's map ->
// Facet floor plan data (the iso_floorplan `plan` + `transform`). Plain C++ (no LVGL calls), so the same code runs in the
// editor's browser (WASM, fw_fpgen) and on the panel.
//
//   source --adapter--> Input (one intermediate for every brand) --generate()--> Plan (+ a report of what was cut)
//
// Adapters (each fills the same Input: a labelled raster in the map frame, metres, x right, z down; the room ids +
// names; the affine from the vacuum's own units to that frame; robot / dock / path in the vacuum's units):
//   room_colour_image  a map picture where every room has its own flat colour (Roborock core image entity, Xiaomi /
//                      Dreame / Viomi style renderers). The palette is found from the picture: flat, thick, colourful
//                      areas are rooms; dull dark ones are walls; thin or small marks (path, icons, text) are overlays
//                      that the rooms grow back over. Pixel size from the drawing's upscale (a 5 cm map pixel), one
//                      anchor (a pixel + its vacuum position) ties it to the vacuum's coordinates.
//   valetudo_json      Valetudo's ValetudoMap JSON (layers floor / wall / segment as pixel run lengths, entities in cm):
//                      exact room masks, names from the segments.
//   polygons           rooms already given as outlines (Ecovacs-style outline data, or rectangles from the editor).
// generate(): per room clean (close notches, cut islands and thin bumps), cut floor that spilled through a doorway,
// move sides out to the walls the vacuum saw, trace, straighten (rectilinear), line shared walls up, put every corner
// on the grid with a gap between rooms (no overlaps by construction), propose the leftover space inside the house as
// rooms it can't clean, label points, quality checks. Every cut is reported (kind, m2, where), never silent.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ArduinoJson.h"

namespace fw::fpa {
enum Cls : uint8_t { UNKNOWN = 0, FLOOR = 1, WALL = 2, ROOM = 3, OVERLAY = 4 };
struct Grid {                       // the map frame: metres, x right, z down
  int w = 0, h = 0;
  double res = 0.05, ox = 0, oz = 0;  // metres per cell; metres of cell (0,0)'s top-left corner
  std::vector<uint16_t> room;         // 0 = none, else 1 + index into Input::rooms
  std::vector<uint8_t> cls;           // Cls per cell
  void init(int W, int H) { w = W, h = H, room.assign((size_t) W * H, 0), cls.assign((size_t) W * H, UNKNOWN); }
};
struct RoomIn { std::string id, name; uint32_t rgb = 0; };
struct Input {
  std::string adapter;
  Grid g;
  std::vector<RoomIn> rooms;
  double vac[6] = {1, 0, 0, 0, 1, 0};  // vacuum units -> map metres: x = a X + b Y + c, z = d X + e Y + f
  bool has_robot = false, has_dock = false, shape = true;  // shape = spill cut + wall snapping (maps; off for outlines)
  double robot[2] = {0, 0}, dock[2] = {0, 0};             // vacuum units
  std::vector<double> path;                               // vacuum units, x,y pairs
  std::vector<std::string> notes;                         // what the adapter decided (palette, scale, ids)
};
struct Options {
  double grid, gap, straighten, align, smooth, trim, min_room, doorway, spill, spill_keep, reach, cover, spare_min, spare_trim, seal, speck;
  bool shape, spare;
};
Options options(JsonVariantConst o, bool shape_default);  // token defaults, any key overridden by o
struct Pt { double x, z; };
struct Cut { std::string room, kind; double m2; Pt at; };  // kind: spill | island | trim | small
struct Room { std::string id, name; std::vector<Pt> poly; Pt label{0, 0}; double m2 = 0; };
struct Step { std::string name, detail; };
struct Plan {
  std::vector<Room> rooms, spare, raw;  // raw = the traced masks before cleanup (own origin, for "before" views)
  std::vector<Cut> cuts;
  std::vector<Step> steps;
  std::vector<std::string> checks;  // failed quality checks (empty = all good)
  double tf[6] = {1, 0, 0, 0, 1, 0};  // vacuum units -> plan metres (the iso_floorplan transform)
  double tf_raw[6] = {1, 0, 0, 0, 1, 0};  // the same for the raw outlines' own origin
  double W = 0, D = 0;
  bool has_robot = false, has_dock = false;
  Pt robot{0, 0}, dock{0, 0};  // plan metres
};
bool from_room_colour_image(const uint8_t *rgba, int w, int h, JsonVariantConst opt, Input &in, std::string &err);
bool from_valetudo_json(JsonVariantConst map, JsonVariantConst opt, Input &in, std::string &err);
bool from_polygons(JsonVariantConst src, JsonVariantConst opt, Input &in, std::string &err);
Plan generate(const Input &in, const Options &o);
std::string plan_string(const std::vector<Room> &rooms, const std::vector<Room> &spare);  // iso_floorplan `plan`
std::string transform_string(const double tf[6]);
void to_json(const Input &in, const Plan &p, JsonObject out);
// one call: opts {"adapter": "room_colour_image" | "valetudo_json" | "polygons", ...}; text = the JSON source
// (valetudo_json / polygons), rgba w x h = the picture (room_colour_image). Returns the report JSON ({"error"} on failure).
std::string run(const char *opts_json, const char *text, const uint8_t *rgba, int w, int h);
}  // namespace fw::fpa
