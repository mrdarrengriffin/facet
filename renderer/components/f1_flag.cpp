// F1 flag: the flag (or board) the race is under, drawn flat in Facet style: a 3:2 rectangle (snapped to the grid) in
// the flag's FIXED colours (flag-* tokens: a flag is the same flag in both themes), always inside a text-colour
// keyline (stroke::hair small, stroke::edge large) so it reads on either canvas (the keyline carries the 3:1).
//   kinds   green, yellow, double yellow (two flags, the second behind), red, blue, black-and-white (split on the
//           diagonal), black, chequered (squares of 4 px small / a quarter of the height large: whole squares only),
//           SC and VSC boards (yellow, the letters in flag-black on the large size; small = the board alone, the words
//           beside it carry the name), none (hidden)
//   sizes   sm = lay::sport_flag_sm (status lines, the header), lg = lay::sport_flag_lg (the banner's hero moment)
//   motion  a change of flag flutters once (a short damped side sway, sport-flash x 2; small object, cheap); reduced
//           motion = no sway. Silent.
// One object, drawn in its own draw event (rects, a triangle, the board letters). Pairs: section "sport".
#include "fw.h"
#include "sport.h"
#include <cmath>

namespace {
enum Kind { NONE, GREEN, YELLOW, DOUBLE_YELLOW, RED, BLUE, BW, BLACK, CHEQUERED, SC, VSC };
Kind kind_of(const char *s) {
  static const char *N[] = {"none", "green", "yellow", "double_yellow", "red", "blue", "bw", "black", "chequered", "sc", "vsc"};
  for (int i = 0; i < 11; i++) if (s && !strcmp(s, N[i])) return (Kind) i;
  if (s && (!strcmp(s, "sc_ending"))) return SC;
  if (s && (!strcmp(s, "vsc_ending"))) return VSC;
  return NONE;
}
struct Flag { Kind k = NONE; bool big = false; int key = 1; };
void rect(lv_layer_t *layer, int x1, int y1, int x2, int y2, lv_color_t c) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c, d.bg_opa = LV_OPA_COVER;
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(layer, &d, &a);
}
void draw_one(lv_layer_t *layer, const lv_area_t &a, const Flag &f, Kind k) {
  const fw::Theme &t = *fw::T;
  const int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
  lv_color_t fill = t.flag_green;
  switch (k) {
    case YELLOW: case DOUBLE_YELLOW: case SC: case VSC: fill = t.flag_yellow; break;
    case RED: fill = t.flag_red; break;
    case BLUE: fill = t.flag_blue; break;
    case BLACK: fill = t.flag_black; break;
    case BW: case CHEQUERED: fill = t.flag_white; break;
    default: break;
  }
  rect(layer, a.x1, a.y1, a.x2, a.y2, fill);
  if (k == BW) {  // black lower-left half
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = t.flag_black, d.opa = LV_OPA_COVER;
    d.p[0] = {(lv_value_precise_t) a.x1, (lv_value_precise_t) a.y1};
    d.p[1] = {(lv_value_precise_t) a.x1, (lv_value_precise_t) (a.y2 + 1)};
    d.p[2] = {(lv_value_precise_t) (a.x2 + 1), (lv_value_precise_t) (a.y2 + 1)};
    lv_draw_triangle(layer, &d);
  }
  if (k == CHEQUERED) {  // whole squares only: 4 px small, a quarter of the height large
    const int q = f.big ? std::max(4, h / 4 / 4 * 4) : 4;
    for (int y = 0; y + q <= h; y += q)
      for (int x = 0; x + q <= w; x += q)
        if (((x / q) + (y / q)) % 2 == 0) rect(layer, a.x1 + x, a.y1 + y, a.x1 + x + q - 1, a.y1 + y + q - 1, t.flag_black);
  }
  if ((k == SC || k == VSC) && f.big) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = fw::font(fw::ty::title);
    d.color = t.flag_black;
    d.align = LV_TEXT_ALIGN_CENTER;
    d.text = k == SC ? "SC" : "VSC";
    const int lh = lv_font_get_line_height(d.font);
    lv_area_t ta = {a.x1, a.y1 + (h - lh) / 2, a.x2, a.y1 + (h - lh) / 2 + lh - 1};
    lv_draw_label(layer, &d, &ta);
  }
}
void draw_cb(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_target(e);
  const Flag *f = (const Flag *) lv_obj_get_user_data(o);
  if (!f || f->k == NONE) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_content_coords(o, &a);
  if (f->k == DOUBLE_YELLOW) {  // two flags, the second offset up-right behind; each with its own keyline
    lv_obj_get_coords(o, &a);
    const int w = lv_area_get_width(&a), dx = std::max(4, w / 6 / 4 * 4), k = f->key;
    const lv_area_t back = {a.x1 + dx, a.y1, a.x2, a.y2 - dx}, front = {a.x1, a.y1 + dx, a.x2 - dx, a.y2};
    for (const lv_area_t &r : {back, front}) {
      rect(layer, r.x1, r.y1, r.x2, r.y2, fw::T->text);
      const lv_area_t in = {r.x1 + k, r.y1 + k, r.x2 - k, r.y2 - k};
      draw_one(layer, in, *f, YELLOW);
    }
    return;
  }
  draw_one(layer, a, *f, f->k);
}
void free_flag(lv_event_t *e) { delete (Flag *) lv_event_get_user_data(e); }
void sway(void *o, int32_t v) {
  const float k = v / 1024.0f;
  lv_obj_set_style_translate_x((lv_obj_t *) o, (int32_t) std::lround(fw::sp::s1 * std::sin(k * 6.2831853f * 2) * (1 - k)), 0);
}
int width_of(bool big) { return big ? fw::lay::sport_flag_lg : fw::lay::sport_flag_sm; }
int height_of(bool big) { return width_of(big) * 2 / 3 / 4 * 4; }
void set_kind(lv_obj_t *o, Kind k, bool glide) {
  Flag *f = (Flag *) lv_obj_get_user_data(o);
  if (f->k == k) return;
  const bool flutter = glide && f->k != NONE && k != NONE && !fw::mo::reduced(o);
  f->k = k;
  lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, k == NONE);
  lv_obj_set_style_border_opa(o, k == DOUBLE_YELLOW ? LV_OPA_TRANSP : LV_OPA_COVER, 0);  // it draws its own two
  lv_obj_invalidate(o);
  if (flutter) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, sway);
    lv_anim_set_values(&a, 0, 1024);
    lv_anim_set_duration(&a, fw::mo::ms::sport_flash * 2);
    lv_anim_start(&a);
  }
}
void fill(lv_obj_t *o, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool big = !strcmp(p["size"] | "sm", "lg");
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int keyline = big ? fw::stroke::edge : fw::stroke::hair;
  lv_obj_set_size(o, width_of(big), height_of(big));
  lv_obj_set_style_border_width(o, keyline, 0);
  lv_obj_set_style_border_color(o, t.text, 0);
  lv_obj_set_style_pad_all(o, keyline, 0);
  Flag *f = (Flag *) lv_obj_get_user_data(o);
  if (!f) {  // once per root (user data and callbacks survive a refill)
    f = new Flag;
    lv_obj_set_user_data(o, f);
    lv_obj_add_event_cb(o, free_flag, LV_EVENT_DELETE, f);
    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN_END, nullptr);
  }
  f->big = big, f->k = NONE, f->key = keyline;
  fw::bleed(o, fw::sp::s1);  // the flutter sways it a step either side
  fw::qa::free(o, "sways a step either side for a moment when the flag changes");
  set_kind(o, kind_of(p["flag"] | "green"), false);
  fw::shape(o, big ? "lg" : "sm");
}
}  // namespace

FW_COMPONENT(f1_flag, "F1 flag", "components", R"({
  "about": "The flag the race is under, drawn flat: green, yellow, double yellow, red, blue, black and white, black, chequered, and the safety car boards.",
  "frame": [160, 112],
  "props": [
    {"name": "flag", "about": "Which flag to show: green, yellow, double_yellow, red, blue, bw (black and white for a warning), black, chequered, sc for the safety car, vsc for the virtual safety car, or none.", "type": "select", "options": ["green", "yellow", "double_yellow", "red", "blue", "bw", "black", "chequered", "sc", "vsc", "none"], "default": "green"},
    {"name": "size", "about": "sm is a small flag for a line of text; lg is a big one for a card.", "type": "select", "options": ["sm", "lg"], "default": "lg"}
  ],
  "variants": [
    {"label": "green", "props": {}},
    {"label": "yellow", "props": {"flag": "yellow"}},
    {"label": "double yellow", "props": {"flag": "double_yellow"}},
    {"label": "red", "props": {"flag": "red"}},
    {"label": "blue", "props": {"flag": "blue"}},
    {"label": "black and white", "props": {"flag": "bw"}},
    {"label": "black", "props": {"flag": "black"}},
    {"label": "chequered", "props": {"flag": "chequered"}},
    {"label": "SC board", "props": {"flag": "sc"}},
    {"label": "VSC board", "props": {"flag": "vsc"}},
    {"label": "chequered · small", "props": {"flag": "chequered", "size": "sm"}},
    {"label": "SC · small", "props": {"flag": "sc", "size": "sm"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: another flag = in place + one flutter (silent); another size refills.
FW_UPDATE(f1_flag) {
  const bool big = !strcmp(p["size"] | "sm", "lg");
  if (!fw::shape(root, big ? "lg" : "sm")) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  set_kind(root, kind_of(p["flag"] | "green"), true);
  return true;
}
