// 3D print model (docs: Screens > 3D print, @facet/ui print3d/grid.ts + render.ts): the print as a voxel grid drawn in
// true isometric. Below the cut (the current layer) the printed part, solid, in flat tones (one per face direction: lit
// top, mid left, dark right); the last `hot` slices printed wear the accent (the one thing that matters); above the cut
// a ghost: a faint fill with hairline edges (silhouette + creases), the printed part showing through it. A finished
// print = all solid, no ghost. The model fits the frame by the WHOLE model, so it never moves while it fills up.
//   model    the docs demo lighthouse (40 x 40 x 60 mm, made up from a shape function; a panel gets its grid from the
//            printer's gcode / STL on the hub, the same Grid) at cell 1 mm x layer 0.4 mm (docs 0.5 x 0.2: an eighth of
//            the voxels, so a panel builds it in one go). Built once, kept.
//   render   software z-buffer per pass (solid, then ghost), each exposed face (+x, +y, +z) a parallelogram,
//            supersampled `ss` x ss then averaged down into one RGB565 image (fw: lv_image). Depth = uint16, so a frame
//            costs 8 bytes per sample while it renders (720 x 440 at ss 2 = 10 MB: panels use ss 1, 2.5 MB), freed after.
//   palette  colour tokens only (docs palette()): solid neutral = line .. text (dark) / text-dim .. surface-press
//            (light); steel = blue shaded; the hot layers accent-press .. accent; ghost surface .. surface-press (dark) /
//            surface-press .. line (light) at 60 %, edges text-mute (dark) / line-hi (light)
// Update path: progress (the layer) re-renders in place, at once (a new layer is a new picture; the docs redraw the
// canvas the same way): audit "snaps". solid / size / ss = re-render too. Silent, no touch. Docs "smooth" shading
// (normals from the voxel neighbourhood) is not ported: faces is the default and the only one Darren has seen.
// Contrast: tokens/colour.json section "print_model".
#include <cstring>
#include <vector>
#include "fw.h"

namespace {
struct Grid {
  int nx = 0, ny = 0, nz = 0;
  float cell = 1, layer = 0.4f;
  std::vector<uint8_t> v;  // (k * ny + j) * nx + i
  float bx0 = 0, bx1 = 0, by0 = 0, by1 = 0;  // iso extents in mm (docs bounds)
};
constexpr float C30 = 0.8660254f, S30 = 0.5f;
constexpr float L[3] = {0.38f, 0.66f, 0.94f};  // light: lit top, +y (left) face, +x (right) face (docs L, indices as used)

// the docs demo: a lighthouse (hollow tapered tower with windows, gallery, lantern room, roof)
// (dx, dy = mm from the centre; r, a = its radius and angle, worked out once per column)
bool lighthouse(float dx, float dy, float r, float a, float z) {
  if (z < 3) return std::max(std::fabs(dx), std::fabs(dy)) < 17 - std::max(0.0f, z - 2) * 1.5f;
  if (z < 44) {
    const float R = 11 - (z - 3) / 41 * 3.5f;
    if (r > R) return false;
    if (z < 6) return true;
    if (r < R - 1.6f) return false;
    auto win = [&](float zc) { return std::fabs(z - zc) < 2.4f && std::fabs(std::sin(a - 0.785398f)) < 0.18f; };
    return !(win(17) || win(31));
  }
  if (z < 46) return r < 11;
  if (z < 49.5f) return (r > 10.2f && r < 11 && std::cos(a * 16) > -0.6f) || r < 6.2f;
  if (z < 53) return r < 6.2f && (r > 5 ? std::fabs(std::sin(a * 3)) > 0.35f : r < 1.2f);
  if (z < 58) return r < 7 - (z - 53) / 5 * 6.2f;
  return r < 0.9f;
}
const Grid &model() {
  static Grid g;
  if (g.nx) return g;
  g.cell = 1.0f;
  g.layer = 0.4f;
  g.nx = g.ny = 40;
  g.nz = 150;
  g.v.assign((size_t) g.nx * g.ny * g.nz, 0);
  g.bx0 = g.by0 = 1e9f;
  g.bx1 = g.by1 = -1e9f;
  std::vector<float> col((size_t) g.nx * g.ny * 4);  // per column: dx, dy, r, a
  for (int j = 0; j < g.ny; j++)
    for (int i = 0; i < g.nx; i++) {
      float *c = &col[((size_t) j * g.nx + i) * 4];
      c[0] = (i + 0.5f) * g.cell - 20;
      c[1] = (j + 0.5f) * g.cell - 20;
      c[2] = std::sqrt(c[0] * c[0] + c[1] * c[1]);
      c[3] = std::atan2(c[1], c[0]);
    }
  for (int k = 0; k < g.nz; k++)
    for (int j = 0; j < g.ny; j++)
      for (int i = 0; i < g.nx; i++) {
        const float *c = &col[((size_t) j * g.nx + i) * 4];
        if (!lighthouse(c[0], c[1], c[2], c[3], (k + 0.5f) * g.layer)) continue;
        g.v[((size_t) k * g.ny + j) * g.nx + i] = 1;
        const float x0 = i * g.cell, y0 = j * g.cell, x1 = x0 + g.cell, y1 = y0 + g.cell;
        g.bx0 = std::min(g.bx0, (x0 - y1) * C30);
        g.bx1 = std::max(g.bx1, (x1 - y0) * C30);
        g.by0 = std::min(g.by0, (x0 + y0) * S30 - (k + 1) * g.layer);
        g.by1 = std::max(g.by1, (x1 + y1) * S30 - k * g.layer);
      }
  return g;
}

struct Rgb { float r, g, b; };
Rgb rgb(lv_color_t c) { return {(float) c.red, (float) c.green, (float) c.blue}; }
Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
struct Pal { Rgb bg, lo, hi, hot, hot_lo, ghost_lo, ghost_hi, edge; float ghost_a; };
Pal palette(bool steel) {
  const fw::Theme &t = *fw::T;
  const bool dark = lv_color_luminance(t.bg) < 128;  // the theme in use (docs palette(theme))
  Pal p;
  p.bg = rgb(t.bg);
  if (steel) {
    p.lo = mix(rgb(t.blue), {0, 0, 0}, dark ? 0.7f : 0.55f);
    p.hi = mix(rgb(t.blue), {255, 255, 255}, dark ? 0.1f : 0.2f);
  } else {
    p.lo = rgb(dark ? t.line : t.text_dim);
    p.hi = rgb(dark ? t.text : t.surface_press);
  }
  p.hot = rgb(t.accent);
  p.hot_lo = rgb(t.accent_press);
  p.ghost_lo = rgb(dark ? t.surface : t.surface_press);
  p.ghost_hi = rgb(dark ? t.surface_press : t.line);
  p.edge = rgb(dark ? t.text_mute : t.line_hi);
  p.ghost_a = 0.6f;
  return p;
}

struct Opts {
  int w, h, ss, hot;
  float progress;
  bool steel;
  bool operator==(const Opts &b) const { return w == b.w && h == b.h && ss == b.ss && hot == b.hot && progress == b.progress && steel == b.steel; }
};
// renders into an RGB565 draw buffer w x h (docs render(), faces shading)
void render(lv_draw_buf_t *out, const Opts &o) {
  const Grid &g = model();
  const int ss = std::max(1, o.ss), W = o.w * ss, H = o.h * ss, pad = fw::sp::s5 * ss;
  const float sc = std::min((W - 2.0f * pad) / (g.bx1 - g.bx0), (H - 2.0f * pad) / (g.by1 - g.by0));
  const float ox = W / 2.0f - (g.bx0 + g.bx1) / 2 * sc, oy = H / 2.0f - (g.by0 + g.by1) / 2 * sc;
  auto PX = [&](float x, float y) { return ox + (x - y) * C30 * sc; };
  auto PY = [&](float x, float y, float z) { return oy + ((x + y) * S30 - z) * sc; };
  const int cut = std::clamp((int) std::lround(o.progress / 100.0f * g.nz), 0, g.nz), hot_from = cut - o.hot;
  const bool done = cut >= g.nz;
  const size_t N = (size_t) W * H;
  std::vector<uint16_t> sd(N, 0), gd(N, 0), col(N, 0);  // depth (0 = empty), solid colour index
  std::vector<uint8_t> gt(N, 0), fl(N, 0);              // ghost tone x 255, flags: 1 hot, 2 ghost edge
  std::vector<Rgb> tones;                               // the few solid colours (per face x hot)
  const Pal pal = palette(o.steel);
  auto occ = [&](int i, int j, int k, int lim) {
    return i >= 0 && j >= 0 && k >= 0 && i < g.nx && j < g.ny && k < lim && g.v[((size_t) k * g.ny + j) * g.nx + i];
  };
  auto quad = [&](float x0, float y0, float ux, float uy, float vx, float vy, float d, std::vector<uint16_t> &buf, auto put) {
    const float xs[4] = {x0, x0 + ux, x0 + vx, x0 + ux + vx}, ys[4] = {y0, y0 + uy, y0 + vy, y0 + uy + vy};
    const int minX = std::max(0, (int) std::floor(std::min({xs[0], xs[1], xs[2], xs[3]})));
    const int maxX = std::min(W - 1, (int) std::ceil(std::max({xs[0], xs[1], xs[2], xs[3]})));
    const int minY = std::max(0, (int) std::floor(std::min({ys[0], ys[1], ys[2], ys[3]})));
    const int maxY = std::min(H - 1, (int) std::ceil(std::max({ys[0], ys[1], ys[2], ys[3]})));
    const float det = ux * vy - uy * vx;
    if (det == 0) return;
    const uint16_t dq = (uint16_t) (1 + std::lround(d * 256));
    for (int py = minY; py <= maxY; py++)
      for (int px = minX; px <= maxX; px++) {
        const float dx = px + 0.5f - x0, dy = py + 0.5f - y0;
        const float a = (dx * vy - dy * vx) / det, b = (ux * dy - uy * dx) / det;
        if (a < 0 || a > 1 || b < 0 || b > 1) continue;
        const size_t p = (size_t) py * W + px;
        if (dq <= buf[p]) continue;
        buf[p] = dq;
        put(p);
      }
  };
  // solid colours: index 1 + face (0 top, 1 left, 2 right) + 3 x hot
  for (int h = 0; h < 2; h++)
    for (float l : {L[2], L[1], L[0]}) tones.push_back(h ? mix(pal.hot_lo, pal.hot, l) : mix(pal.lo, pal.hi, l));
  auto pass = [&](bool ghost) {
    const int lim = ghost ? g.nz : cut, k0 = ghost ? cut : 0;
    std::vector<uint16_t> &buf = ghost ? gd : sd;
    for (int k = k0; k < lim; k++) {
      const float z0 = k * g.layer, z1 = z0 + g.layer;
      for (int j = 0; j < g.ny; j++)
        for (int i = 0; i < g.nx; i++) {
          if (!g.v[((size_t) k * g.ny + j) * g.nx + i]) continue;
          const bool top = !occ(i, j, k + 1, ghost ? g.nz : lim), right = !occ(i + 1, j, k, lim), left = !occ(i, j + 1, k, lim);
          if (!top && !right && !left) continue;
          const float x0 = i * g.cell, x1 = x0 + g.cell, y0 = j * g.cell, y1 = y0 + g.cell;
          const bool hot = !ghost && !done && k >= hot_from;
          auto put = [&](int face, float tone) {
            return [&, face, tone](size_t p) {
              if (ghost) gt[p] = (uint8_t) std::lround(tone * 255);
              else { col[p] = (uint16_t) (1 + face + (hot ? 3 : 0)); fl[p] = hot ? 1 : 0; }
            };
          };
          if (top) quad(PX(x0, y0), PY(x0, y0, z1), PX(x1, y0) - PX(x0, y0), PY(x1, y0, z1) - PY(x0, y0, z1),
                        PX(x0, y1) - PX(x0, y0), PY(x0, y1, z1) - PY(x0, y0, z1), x0 + y0 + g.cell + z1, buf, put(0, L[2]));
          if (right) quad(PX(x1, y0), PY(x1, y0, z1), PX(x1, y1) - PX(x1, y0), PY(x1, y1, z1) - PY(x1, y0, z1), 0,
                          (z1 - z0) * sc, x1 + y1 + (z0 + z1) / 2 - g.cell / 2, buf, put(2, L[0]));
          if (left) quad(PX(x0, y1), PY(x0, y1, z1), PX(x1, y1) - PX(x0, y1), PY(x1, y1, z1) - PY(x0, y1, z1), 0,
                         (z1 - z0) * sc, x1 + y1 + (z0 + z1) / 2 - g.cell / 2, buf, put(1, L[1]));
        }
    }
  };
  pass(false);
  if (!done) pass(true);
  if (!done) {  // ghost edges: silhouette + creases (depth jumps) at ~1 px of the final image
    const int jump = (int) std::lround(2.5f * g.cell * 256), r = std::max(1, (int) std::lround(ss * 0.75f));
    const int nb[4][2] = {{r, 0}, {-r, 0}, {0, r}, {0, -r}};
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t p = (size_t) y * W + x;
        if (!gd[p]) continue;
        for (auto &n : nb) {
          const int X = x + n[0], Y = y + n[1];
          if (X < 0 || Y < 0 || X >= W || Y >= H) { fl[p] |= 2; break; }
          const size_t q = (size_t) Y * W + X;
          if (!gd[q] || (int) gd[q] - (int) gd[p] > jump) { fl[p] |= 2; break; }
        }
      }
  }
  // compose + average down
  const uint32_t stride = out->header.stride;
  for (int y = 0; y < o.h; y++) {
    auto *row = (uint16_t *) (out->data + (size_t) y * stride);
    for (int x = 0; x < o.w; x++) {
      Rgb acc{0, 0, 0};
      for (int sy = 0; sy < ss; sy++)
        for (int sx = 0; sx < ss; sx++) {
          const size_t p = (size_t) (y * ss + sy) * W + x * ss + sx;
          Rgb c = sd[p] ? tones[col[p] - 1] : pal.bg;
          if (gd[p] && gd[p] > sd[p]) {
            const Rgb gc = mix(pal.ghost_lo, pal.ghost_hi, gt[p] / 255.0f);
            c = sd[p] ? mix(c, gc, (fl[p] & 1) ? pal.ghost_a * 0.3f : pal.ghost_a) : gc;
            if (fl[p] & 2) c = mix(c, pal.edge, sd[p] ? 0.6f : 1.0f);
          }
          acc.r += c.r;
          acc.g += c.g;
          acc.b += c.b;
        }
      const float n = (float) (ss * ss);
      row[x] = lv_color_to_u16(lv_color_make((uint8_t) (acc.r / n), (uint8_t) (acc.g / n), (uint8_t) (acc.b / n)));
    }
  }
}

struct PM { lv_draw_buf_t *buf = nullptr; lv_obj_t *img = nullptr; Opts o{}; const fw::Theme *theme = nullptr; };
Opts opts_of(fw::Props p) {
  Opts o;
  o.w = fw::px((float) (p["width"] | 288));
  o.h = fw::px((float) (p["height"] | 288));
  o.ss = p["ss"] | 2;
  o.hot = p["hot"] | 4;
  o.progress = (float) (p["progress"] | 45);
  o.steel = !strcmp(p["solid"] | "neutral", "steel");
  return o;
}
void draw(PM &m) {
  if (!m.buf || (int) m.buf->header.w != m.o.w || (int) m.buf->header.h != m.o.h) {
    if (m.buf) lv_draw_buf_destroy(m.buf);
    m.buf = lv_draw_buf_create(m.o.w, m.o.h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    lv_obj_set_size(m.img, m.o.w, m.o.h);
  }
  render(m.buf, m.o);
  m.theme = fw::T;
  lv_image_set_src(m.img, m.buf);
  lv_obj_invalidate(m.img);
}
}  // namespace

FW_COMPONENT(print_model, "3D print model", "components", R"({
  "about": "The print as a voxel model in isometric: the printed part solid, the last layers in the accent, the rest a ghost with hairline edges. Fits by the whole model, so it never moves while it fills up.",
  "frame": [288, 288],
  "props": [
    {"name": "progress", "type": "range", "min": 0, "max": 100, "step": 1, "default": 45},
    {"name": "solid", "type": "select", "options": ["neutral", "steel"], "default": "neutral"},
    {"name": "hot", "type": "range", "min": 0, "max": 12, "step": 1, "default": 4},
    {"name": "width", "type": "range", "min": 160, "max": 440, "step": 8, "default": 288},
    {"name": "height", "type": "range", "min": 160, "max": 296, "step": 8, "default": 288},
    {"name": "ss", "type": "range", "min": 1, "max": 3, "step": 1, "default": 2}
  ],
  "audit": {"snaps": {"progress": "a new layer is a new picture (a re-render, like the docs canvas); the print moves one layer at a time"}},
  "variants": [
    {"label": "printing · 45 %", "props": {}},
    {"label": "just started", "props": {"progress": 8}},
    {"label": "nearly done", "props": {"progress": 92}},
    {"label": "done", "props": {"progress": 100}},
    {"label": "steel", "props": {"solid": "steel"}},
    {"label": "panel quality: ss 1", "props": {"ss": 1}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  auto *m = new PM;
  m->o = opts_of(p);
  lv_obj_set_size(root, m->o.w, m->o.h);
  m->img = lv_image_create(root);
  lv_obj_set_user_data(root, m);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *m = (PM *) lv_event_get_user_data(e);
    if (m->buf) lv_draw_buf_destroy(m->buf);
    delete m;
  }, LV_EVENT_DELETE, m);
  draw(*m);
}

FW_UPDATE(print_model) {
  auto *m = (PM *) lv_obj_get_user_data(root);
  if (!m) return false;
  const Opts o = opts_of(p);
  if (o == m->o && m->theme == fw::T) return true;
  m->o = o;
  lv_obj_set_size(root, o.w, o.h);
  draw(*m);
  return true;
}
