// Sports surfaces (Foundations > Sports surfaces, tokens/surfaces.json -> surfaces_gen.h fw::sf::): how a playing
// surface is drawn, accurate to the venue but Facet-styled. A surface = run-off + court colour tokens (colour.json
// group courts, both themes, muted to sit on the canvas) + optional mowing stripes; a venue = a preset over a surface
// (Wimbledon, Roland-Garros, US Open, Australian Open, ATP Finals).
//   lines   court-line (white; dimmer on dark; a surface may set its own, the Facet court = text-dim), one weight (sf::LINE_W = stroke edge) at every size; 3:1 on every
//           surface, run-off and highlighted box (contrast section "surfaces")
//   grid    the outer box and the court's outer edge land on the 4 px grid; lines inside at true ITF proportions;
//           marks too small at true scale (the centre mark) get one grid step
//   cheap   ONE object draws everything in its draw event (run-off, stripes, court, mark, lines, net); the
//           highlighted box moves by invalidating the union of its old and new box only (live scores redraw little)
// Tennis first (tennis_court); football_pitch and the F1 track move onto the same rules later (sf::SURFACES has the
// grass + stripes they need).
#pragma once
#include "fw.h"
#include "surfaces_gen.h"

namespace fw::sf {
const Surface &find(const char *id);         // surface or venue id; unknown / empty = the Facet court (generic)
const Surface *from_data(const char *name);  // a source's surface word ("Clay", "Hard", "Grass", API-Tennis
                                             // tournament_sourface) -> a real base surface (never facet); nullptr = unknown
struct Box { int x = 0, y = 0, w = 0, h = 0; };
// a tennis court fitted to an outer width (device px, on the grid): outer height + court box on the grid
struct TennisFit {
  int w = 0, h = 0;
  Box court;
  int X(float m) const { return court.x + (int) lroundf(m * court.w / tennis::length); }  // metres from the left baseline
  int Y(float m) const { return court.y + (int) lroundf(m * court.h / tennis::width); }   // metres from the top sideline
};
TennisFit tennis_fit(int w);
lv_obj_t *court(lv_obj_t *parent, int w, const Surface &s);  // the court object (w x tennis_fit(w).h), place it yourself
const TennisFit &tennis_geo(lv_obj_t *court);
void tennis_surface(lv_obj_t *court, const Surface &s);    // another surface in place (one redraw)
// the highlighted area (px inside the court object): the surface's mark mix (court-line wash; facet: accent wash), drawn under the lines; glide = Motion "value", silent
void tennis_mark(lv_obj_t *court, Box b, bool show, bool glide);
}  // namespace fw::sf
