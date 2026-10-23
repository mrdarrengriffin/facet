// Stress / capability item (facet-153 round 3, Darren 2026-10-22: "benchmark the hardware and the screen"): one
// configurable load that the bench (renderer/bench/suite.json "stress", tools/bench.mjs, firmware/fw_bench.h, the hub's
// /wasm/bench) ramps until the frame rate drops. Not a Facet component: section "bench", so the workbench nav leaves it
// out; it uses the theme tokens so it reads in both themes, and real components where the load is a component (the
// animated weather icon).
//   kind  ramps (n instances of 48 dp in a grid, shrinking only once they no longer fit; every one moving every frame):
//           icons   n animated weather icons (the Weather icon component, feature size)
//           arcs    n arcs (lv_arc) turning
//           rects   n small boxes sliding sideways
//           labels  n labels whose number changes every frame
//         boundaries (ONE object, side = size x the screen, centred; ms = the loop's duration = speed):
//           move    slides sideways (translate_x)          fade    opacity 255 <-> 64
//           colour  steps between two colours every frame   rotate  turns (transform_rotation, layer render)
//         raw capability (the whole screen redrawn every frame):
//           refresh one 8 x 8 box changes colour every frame (the refresh ceiling)
//           fill    the whole screen, opaque, changes colour   blend  a 50 % layer over the whole screen
//           blit    a full-screen RGB565 image, moved 1 px     blit_alpha  the same as RGB565A8 (per-pixel alpha)
//           text    the screen full of glyphs at size dp (rows = H / (1.25 px), columns = W / (0.62 px), px = dp x the
//                   density), every glyph changes every frame (host: glyphs = rows x columns)
//         memory:   objects  n static rows (a box + a label), the per-object cost in RAM
//         input:    touch    a full-screen box that changes colour on press (touch-to-frame latency)
#include "fw.h"

namespace {
struct St {
  lv_timer_t *tm = nullptr;
  std::vector<lv_obj_t *> objs;
  uint32_t k = 0;
  lv_draw_buf_t *buf = nullptr;
  lv_color_t a, b;
  int mode = 0;  // timer job: 1 labels, 2 colour flip, 3 nudge, 4 text
  int rows = 0, cols = 0;
};
void free_st(lv_event_t *e) {
  auto *s = (St *) lv_event_get_user_data(e);
  if (s->tm) lv_timer_delete(s->tm);
  if (s->buf) lv_draw_buf_destroy(s->buf);
  delete s;
}
void anim(lv_obj_t *o, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, uint32_t delay = 0) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_reverse_duration(&a, ms);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_delay(&a, delay);
  lv_anim_start(&a);
}
void tx(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *) o, v, 0); }
void opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void rot(void *o, int32_t v) { lv_obj_set_style_transform_rotation((lv_obj_t *) o, v, 0); }
void arc_rot(void *o, int32_t v) { lv_arc_set_rotation((lv_obj_t *) o, v); }
lv_obj_t *box(lv_obj_t *p, int32_t w, int32_t h, lv_color_t c) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}
void tick(lv_timer_t *t) {
  auto *s = (St *) lv_timer_get_user_data(t);
  s->k++;
  if (s->mode == 1) {
    char b[16];
    for (size_t i = 0; i < s->objs.size(); i++) { snprintf(b, sizeof b, "%u", (unsigned) ((s->k + i * 7) % 1000)); lv_label_set_text(s->objs[i], b); }
  } else if (s->mode == 2) {
    for (auto *o : s->objs) lv_obj_set_style_bg_color(o, s->k & 1 ? s->a : s->b, 0);
  } else if (s->mode == 3) {
    for (auto *o : s->objs) lv_obj_set_style_translate_x(o, (int32_t) (s->k & 1), 0);
  } else if (s->mode == 4) {
    static const char G[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz0123456789";
    std::string row(s->cols, ' ');
    for (size_t r = 0; r < s->objs.size(); r++) {
      for (int c = 0; c < s->cols; c++) row[c] = G[(s->k + r * 3 + c) % (sizeof G - 1)];
      lv_label_set_text(s->objs[r], row.c_str());
    }
  }
}
lv_obj_t *grid_cell(lv_obj_t *root, int i, int cols, int32_t cell) {
  lv_obj_t *c = lv_obj_create(root);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, cell, cell);
  lv_obj_set_pos(c, (i % cols) * cell, (i / cols) * cell);
  return c;
}
}  // namespace

FW_COMPONENT(stress, "Stress", "bench", R"({
  "about": "Bench load (not a Facet component): a ramp of moving things, one moving object of a given size, or a raw redraw of the whole screen, for the stress and capability suite.",
  "frame": [480, 480],
  "props": [
    {"name": "kind", "type": "select", "options": ["icons", "arcs", "rects", "labels", "move", "fade", "colour", "rotate", "refresh", "fill", "blend", "blit", "blit_alpha", "text", "objects", "touch"], "default": "icons"},
    {"name": "n", "type": "range", "min": 1, "max": 4096, "step": 1, "default": 16},
    {"name": "size", "type": "range", "min": 0.02, "max": 1, "step": 0.01, "default": 0.35},
    {"name": "ms", "type": "range", "min": 60, "max": 4000, "step": 10, "default": 1000},
    {"name": "dp", "type": "range", "min": 8, "max": 96, "step": 1, "default": 16}
  ],
  "variants": [
    {"label": "16 animated icons", "props": {"kind": "icons", "n": 16}},
    {"label": "64 sliding boxes", "props": {"kind": "rects", "n": 64}},
    {"label": "half-screen fade", "props": {"kind": "fade", "size": 0.5}},
    {"label": "full-screen blend", "props": {"kind": "blend"}},
    {"label": "text 16 dp", "props": {"kind": "text", "dp": 16}}
  ]
})") {
  const fw::Theme &T = *fw::T;
  lv_display_t *d = lv_obj_get_display(parent);
  const int32_t W = lv_display_get_horizontal_resolution(d), H = lv_display_get_vertical_resolution(d);
  const char *kind = p["kind"] | "icons";
  const int n = std::max(1, std::min(4096, (int) (p["n"] | 16)));
  const float size = std::max(0.02f, std::min(1.f, (float) (p["size"] | 0.35)));
  const uint32_t ms = (uint32_t) std::max(60, std::min(4000, (int) (p["ms"] | 1000)));
  const float dp = std::max(8.f, std::min(96.f, (float) (p["dp"] | 16)));

  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(root, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(root, W, H);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_color(root, T.bg, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  auto *s = new St();
  s->a = T.accent;
  s->b = T.surface;
  lv_obj_add_event_cb(root, free_st, LV_EVENT_DELETE, s);
  auto every_frame = [&](int mode) { s->mode = mode; s->tm = lv_timer_create(tick, 1, s); };
  // ramps: every instance is one 48 dp cell (the size of a Home glyph tile) while they fit on the screen; past that the
  // cells shrink to fit (the count keeps going up, the size stops being constant: the card notes where that starts)
  const int cols = (int) std::ceil(std::sqrt((double) n));
  const int32_t cell = std::max<int32_t>(4, std::min<int32_t>(fw::px(48), std::min(W, H) / cols));

  if (!strcmp(kind, "icons")) {
    static const char *IC[] = {"rain-heavy", "partly-day", "snow", "thunder-rain", "showers-day", "clear-day"};
    const fw::Component *wi = fw::find("weather_icon");
    for (int i = 0; i < n && wi; i++) {
      lv_obj_t *c = grid_cell(root, i, cols, cell);
      lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      ArduinoJson::JsonDocument q;
      q["icon"] = IC[i % 6];
      q["size"] = "feature";
      q["animated"] = true;
      wi->build(c, q.as<fw::Props>());
    }
  } else if (!strcmp(kind, "arcs")) {
    for (int i = 0; i < n; i++) {
      lv_obj_t *c = grid_cell(root, i, cols, cell);
      lv_obj_t *a = lv_arc_create(c);
      lv_obj_remove_style_all(a);
      lv_obj_set_size(a, cell - 8, cell - 8);
      lv_obj_center(a);
      lv_arc_set_bg_angles(a, 0, 360);
      lv_arc_set_angles(a, 0, 90);
      lv_obj_set_style_arc_width(a, std::max<int32_t>(2, cell / 10), LV_PART_MAIN);
      lv_obj_set_style_arc_color(a, T.line, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_arc_width(a, std::max<int32_t>(2, cell / 10), LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(a, T.accent, LV_PART_INDICATOR);
      lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
      lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
      anim(a, arc_rot, 0, 359, ms, (uint32_t) (i * 37 % ms));
    }
  } else if (!strcmp(kind, "rects")) {
    for (int i = 0; i < n; i++) {
      lv_obj_t *c = grid_cell(root, i, cols, cell);
      lv_obj_t *o = box(c, std::max<int32_t>(4, cell / 2), std::max<int32_t>(4, cell / 2), T.accent);
      lv_obj_set_style_radius(o, std::max<int32_t>(2, cell / 12), 0);
      lv_obj_align(o, LV_ALIGN_LEFT_MID, 0, 0);
      anim(o, tx, 0, cell / 2, ms, (uint32_t) (i * 37 % ms));
    }
  } else if (!strcmp(kind, "labels")) {
    for (int i = 0; i < n; i++) {
      lv_obj_t *c = grid_cell(root, i, cols, cell);
      lv_obj_t *l = lv_label_create(c);
      lv_obj_set_style_text_font(l, fw::font(std::max(8.f, std::min(32.f, cell / fw::D / 3)), true), 0);
      lv_obj_set_style_text_color(l, T.text, 0);
      lv_label_set_text(l, "000");
      lv_obj_center(l);
      s->objs.push_back(l);
    }
    every_frame(1);
  } else if (!strcmp(kind, "move") || !strcmp(kind, "fade") || !strcmp(kind, "colour") || !strcmp(kind, "rotate")) {
    const int32_t side = std::max<int32_t>(4, (int32_t) lroundf(size * std::min(W, H)));
    lv_obj_t *o = box(root, side, side, T.accent);
    lv_obj_set_style_radius(o, side / 12, 0);
    lv_obj_t *stripe = box(o, side, std::max<int32_t>(2, side / 8), T.surface);  // so a turn and a slide read
    lv_obj_align(stripe, LV_ALIGN_TOP_MID, 0, side / 8);
    lv_obj_center(o);
    if (!strcmp(kind, "move")) {
      const int32_t range = std::max<int32_t>(16, (W - side) / 2);
      lv_obj_set_style_translate_x(o, -range, 0);
      anim(o, tx, -range, range, ms);
    } else if (!strcmp(kind, "fade")) anim(o, opa, LV_OPA_COVER, 64, ms);
    else if (!strcmp(kind, "rotate")) {
      lv_obj_set_style_transform_pivot_x(o, side / 2, 0);
      lv_obj_set_style_transform_pivot_y(o, side / 2, 0);
      anim(o, rot, 0, 3599, ms);
    } else { s->objs.push_back(o); every_frame(2); }
  } else if (!strcmp(kind, "refresh")) {
    s->objs.push_back(box(root, 8, 8, T.accent));
    every_frame(2);
  } else if (!strcmp(kind, "fill")) {
    s->objs.push_back(box(root, W, H, T.accent));
    every_frame(2);
  } else if (!strcmp(kind, "blend")) {
    box(root, W, H, T.surface);
    lv_obj_t *o = box(root, W, H, T.accent);
    lv_obj_set_style_bg_opa(o, LV_OPA_50, 0);
    s->objs.push_back(o);
    every_frame(2);
  } else if (!strcmp(kind, "blit") || !strcmp(kind, "blit_alpha")) {
    const bool alpha = !strcmp(kind, "blit_alpha");
    s->buf = lv_draw_buf_create(W, H, alpha ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565, 0);
    if (s->buf) {
      uint16_t *px = (uint16_t *) s->buf->data;
      const uint32_t stride = s->buf->header.stride / 2;
      for (int32_t y = 0; y < H; y++)
        for (int32_t x = 0; x < W; x++) px[y * stride + x] = (uint16_t) (((x >> 4) ^ (y >> 4)) & 1 ? 0xF800 | (y & 31) : 0x07E0 | (x & 31));
      if (alpha) {
        uint8_t *a8 = s->buf->data + (size_t) stride * 2 * H;
        for (int32_t y = 0; y < H; y++)
          for (int32_t x = 0; x < W; x++) a8[y * W + x] = (uint8_t) (128 + ((x + y) & 127));
      }
      lv_obj_t *img = lv_image_create(root);
      lv_image_set_src(img, s->buf);
      lv_obj_set_pos(img, 0, 0);
      s->objs.push_back(img);
      every_frame(3);
    }
  } else if (!strcmp(kind, "text")) {
    const float pxs = dp * fw::D;
    s->rows = std::max(1, (int) (H / (pxs * 1.25f)));
    s->cols = std::max(1, (int) (W / (pxs * 0.62f)));
    const lv_font_t *f = fw::font(dp, false);
    for (int r = 0; r < s->rows; r++) {
      lv_obj_t *l = lv_label_create(root);
      lv_obj_set_style_text_font(l, f, 0);
      lv_obj_set_style_text_color(l, T.text, 0);
      lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_width(l, W);
      lv_obj_set_pos(l, 0, (int32_t) (r * pxs * 1.25f));
      lv_label_set_text(l, "");
      s->objs.push_back(l);
    }
    every_frame(4);
  } else if (!strcmp(kind, "objects")) {
    const int32_t rh = std::max<int32_t>(4, H / std::max(1, std::min(n, 64)));
    for (int i = 0; i < n; i++) {
      lv_obj_t *o = box(root, W / 2, rh - 2, T.surface);
      lv_obj_set_pos(o, (i / 64 % 2) * (W / 2), (i % 64) * rh);
      lv_obj_t *l = lv_label_create(o);
      lv_obj_set_style_text_color(l, T.text_dim, 0);
      lv_obj_set_style_text_font(l, fw::font(8.f), 0);
      lv_label_set_text_fmt(l, "%d", i);
    }
  } else if (!strcmp(kind, "touch")) {
    lv_obj_t *o = box(root, W, H, T.surface);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o, T.accent, LV_STATE_PRESSED);
  }
}
