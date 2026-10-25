// F1 car marker: one car as the F1 circuit draws it (Sport foundation marks, core/sport.h): the team colour as a disc
// fitted to 3:1 on what it sits on, cut out by a ring of that colour, the code / number in token text.
//   marker  dot (quietest) / code (dot + three-letter tag) / number (team ring, number inside)
//   lead    text-colour ring (the leader, or a car you follow); pit = hollow; label = the tag on a dot
//   on      bg (the canvas) or surface (a card): the cut-out ring and the tag take that colour, the fit uses it
// A read-out: no touch, no sound. Update path: everything restyles in place (marker kind refills). Pairs: "sport".
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
MarkLook look(fw::Props p) {
  MarkLook l;
  l.mark = mark_of(p["marker"] | "code");
  l.team = hex(p["colour"] | "#F47600", fw::T->text_dim);
  l.behind = !strcmp(p["on"] | "bg", "surface") ? fw::T->surface : fw::T->bg;
  l.code = p["code"] | "NOR";
  l.num = p["num"] | 1;
  l.lead = p["lead"] | false, l.label = p["label"] | false, l.pit = p["pit"] | false;
  return l;
}
void fill(lv_obj_t *root, fw::Props p) {
  const MarkLook l = look(p);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  // a box with room for the tag on the right (the marker sits at its left, centred vertically)
  const int d = mark_size(l.mark);
  const int h = fw::lay::sport_marker_lg + 2 * fw::sp::s2;
  // as wide as the mark + its tag (the widest code), on the 4 px grid: the card is the mark, not empty room
  MarkLook wide = l;
  wide.code = "WWW", wide.label = true;
  const int bw = (fw::sp::s3 + d + fw::sp::s1 + mark_ink(wide).tag_w + fw::sp::s3 + 3) / 4 * 4;
  lv_obj_set_size(root, bw, h);
  lv_obj_set_style_bg_color(root, l.behind, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_t *mk = marker(root, l);
  marker_place(mk, fw::sp::s3 + d / 2.0f, h / 2.0f, bw);
  fw::shape(root, std::string(p["marker"] | "code") + "|" + (p["on"] | "bg"));
}
}  // namespace

FW_COMPONENT(f1_car_marker, "F1 car marker", "components", R"({
  "about": "One car on a circuit: its team colour, its code or number, and whether it leads or is in the pits.",
  "audit": {"snaps": {"on": "what the mark sits on is where it is placed, not a state: it changes with the screen, never live"}},
  "frame": [160, 64],
  "props": [
    {"name": "marker", "about": "How the car is drawn: a plain dot, the driver's three-letter code, or the car number.", "type": "select", "options": ["dot", "code", "number"], "default": "code"},
    {"name": "code", "about": "The driver's three-letter code, such as NOR.", "type": "text", "default": "NOR"},
    {"name": "num", "about": "The car's racing number.", "type": "range", "min": 1, "max": 99, "step": 1, "default": 1},
    {"name": "colour", "about": "The team colour, as a hex colour like #F47600.", "type": "text", "default": "#F47600"},
    {"name": "lead", "about": "Marks the car as the race leader.", "type": "bool", "default": false},
    {"name": "label", "about": "Shows the driver's code in a tag beside the car.", "type": "bool", "default": false},
    {"name": "pit", "about": "Shows the car is in the pit lane.", "type": "bool", "default": false},
    {"name": "on", "about": "What the car sits on, so its edge stands out: bg for the screen background, surface for a card.", "type": "select", "options": ["bg", "surface"], "default": "bg"}
  ],
  "variants": [
    {"label": "code · McLaren", "props": {}},
    {"label": "dot · leader", "props": {"marker": "dot", "lead": true, "code": "VER", "num": 3, "colour": "#4781D7"}},
    {"label": "dot · tag", "props": {"marker": "dot", "label": true, "code": "RUS", "num": 63, "colour": "#00D7B6"}},
    {"label": "number · Ferrari", "props": {"marker": "number", "code": "LEC", "num": 16, "colour": "#ED1131"}},
    {"label": "in the pits", "props": {"pit": true, "code": "ALB", "num": 23, "colour": "#1868DB"}},
    {"label": "Cadillac grey on a card", "props": {"code": "PER", "num": 11, "colour": "#909090", "on": "surface"}},
    {"label": "Haas on a card · number", "props": {"marker": "number", "code": "OCO", "num": 31, "colour": "#9C9FA2", "on": "surface"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(f1_car_marker) {
  if (!fw::shape(root, std::string(p["marker"] | "code") + "|" + (p["on"] | "bg"))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  lv_obj_t *mk = lv_obj_get_child(root, 0);
  marker_set(mk, look(p), true);  // pit in / out, another team: the disc cross-fades (silent)
  const int d = lv_obj_get_width(mk);
  marker_place(mk, fw::sp::s3 + d / 2.0f, lv_obj_get_height(root) / 2.0f, lv_obj_get_width(root));
  return true;
}
