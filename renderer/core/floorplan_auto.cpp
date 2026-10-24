// Floor plan generator: see floorplan_auto.h. Parameters = tokens/floorplan.json "generate" (fw::fp::gen::).
#include "floorplan_auto.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>
#include "fw.h"  // the token headers (fw::fp::gen:: needs the space tokens first)
#include "floorplan_gen.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace fw::fpa {
namespace {
using Mask = std::vector<uint8_t>;
constexpr double INF = 1e30;

// ---------------------------------------------------------------- raster tools
// squared Euclidean distance transform with the nearest site (Felzenszwalb & Huttenlocher), 1D over finite f only
void dt1(const double *f, int n, double *d, int *arg, std::vector<int> &v, std::vector<double> &z) {
  int k = -1;
  for (int q = 0; q < n; q++) {
    if (f[q] >= INF) continue;
    if (k < 0) { k = 0, v[0] = q, z[0] = -INF, z[1] = INF; continue; }
    double s;
    while (true) {  // z[0] = -INF, so k never drops below 0
      s = ((f[q] + (double) q * q) - (f[v[k]] + (double) v[k] * v[k])) / (2.0 * q - 2.0 * v[k]);
      if (s <= z[k]) k--;
      else break;
    }
    k++, v[k] = q, z[k] = s, z[k + 1] = INF;
  }
  if (k < 0) { for (int q = 0; q < n; q++) d[q] = INF, arg[q] = -1; return; }
  int j = 0;
  for (int q = 0; q < n; q++) {
    while (z[j + 1] < q) j++;
    d[q] = (double) (q - v[j]) * (q - v[j]) + f[v[j]], arg[q] = v[j];
  }
}
// d2 = squared distance to the nearest site cell; near = that cell's index (or -1)
void edt(const Mask &site, int w, int h, std::vector<double> &d2, std::vector<int> *near) {
  const int n = std::max(w, h);
  std::vector<double> f(n), d(n), col((size_t) w * h);
  std::vector<int> a(n), v(n + 1), ay((size_t) w * h);
  std::vector<double> z(n + 2);
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) f[y] = site[(size_t) y * w + x] ? 0 : INF;
    dt1(f.data(), h, d.data(), a.data(), v, z);
    for (int y = 0; y < h; y++) col[(size_t) y * w + x] = d[y], ay[(size_t) y * w + x] = a[y];
  }
  d2.assign((size_t) w * h, INF);
  if (near) near->assign((size_t) w * h, -1);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) f[x] = col[(size_t) y * w + x];
    dt1(f.data(), w, d.data(), a.data(), v, z);
    for (int x = 0; x < w; x++) {
      d2[(size_t) y * w + x] = d[x];
      if (near && a[x] >= 0) (*near)[(size_t) y * w + x] = ay[(size_t) y * w + a[x]] * w + a[x];
    }
  }
}
int rpx(double m, double res) { return (std::max(1, (int) (m / res)) | 1) / 2; }  // a disc of diameter m (odd px)
Mask dilate(const Mask &m, int w, int h, int r) {
  if (r <= 0) return m;
  std::vector<double> d;
  edt(m, w, h, d, nullptr);
  Mask o(m.size());
  for (size_t i = 0; i < m.size(); i++) o[i] = d[i] <= (double) r * r;
  return o;
}
Mask invert(const Mask &m) { Mask o(m.size()); for (size_t i = 0; i < m.size(); i++) o[i] = !m[i]; return o; }
Mask erode(const Mask &m, int w, int h, int r) { return r <= 0 ? m : invert(dilate(invert(m), w, h, r)); }
// 4-connected components: lab 1..n, sizes[i] = cells of component i (sizes[0] unused)
int components(const Mask &m, int w, int h, std::vector<int> &lab, std::vector<int> &sizes) {
  lab.assign(m.size(), 0), sizes.assign(1, 0);
  std::vector<int> st;
  int n = 0;
  for (size_t s = 0; s < m.size(); s++) {
    if (!m[s] || lab[s]) continue;
    n++, sizes.push_back(0), st.push_back((int) s), lab[s] = n;
    while (!st.empty()) {
      const int i = st.back(); st.pop_back(); sizes[n]++;
      const int x = i % w, y = i / w;
      const int nb[4] = {x > 0 ? i - 1 : -1, x < w - 1 ? i + 1 : -1, y > 0 ? i - w : -1, y < h - 1 ? i + w : -1};
      for (int j : nb) if (j >= 0 && m[j] && !lab[j]) lab[j] = n, st.push_back(j);
    }
  }
  return n;
}
Mask fill_holes(const Mask &m, int w, int h) {
  Mask out(m.size(), 1), seen(m.size(), 0);
  std::vector<int> st;
  auto push = [&](int i) { if (!m[i] && !seen[i]) seen[i] = 1, st.push_back(i); };
  for (int x = 0; x < w; x++) push(x), push((h - 1) * w + x);
  for (int y = 0; y < h; y++) push(y * w), push(y * w + w - 1);
  while (!st.empty()) {
    const int i = st.back(); st.pop_back(); out[i] = 0;
    const int x = i % w, y = i / w;
    if (x > 0) push(i - 1);
    if (x < w - 1) push(i + 1);
    if (y > 0) push(i - w);
    if (y < h - 1) push(i + w);
  }
  return out;
}
// keep components of at least min cells (largest only = keep just the biggest)
Mask keep(const Mask &m, int w, int h, int min, bool largest) {
  std::vector<int> lab, sz;
  const int n = components(m, w, h, lab, sz);
  int best = 0;
  for (int i = 1; i <= n; i++) if (!best || sz[i] > sz[best]) best = i;
  Mask o(m.size(), 0);
  for (size_t i = 0; i < m.size(); i++) if (lab[i] && (largest ? lab[i] == best && sz[best] >= min : sz[lab[i]] >= min)) o[i] = 1;
  return o;
}
// one solid outline: the largest piece, holes filled, except a hole holding a blocked cell (another room inside this
// one) is opened to the outside by a one-cell slit, so the outline goes round it and never covers it
Mask solid(Mask m, int w, int h, const Mask &blocked) {
  m = keep(m, w, h, 1, true);
  for (int pass = 0; pass < 8; pass++) {
    const Mask f = fill_holes(m, w, h);
    Mask hole(m.size());
    for (size_t i = 0; i < m.size(); i++) hole[i] = f[i] && !m[i];
    std::vector<int> cl, sz;
    const int n = components(hole, w, h, cl, sz);
    std::vector<int> top(n + 1, -1);
    bool cut = false;
    for (size_t i = 0; i < m.size(); i++) if (cl[i] && blocked[i] && top[cl[i]] < 0) top[cl[i]] = 1;
    for (size_t i = 0; i < m.size(); i++) if (cl[i] && top[cl[i]] == 1) {  // first cell of a blocked hole = its top row
      top[cl[i]] = 2, cut = true;
      for (int y = (int) (i / w) - 1, x = (int) (i % w); y >= 0 && m[(size_t) y * w + x]; y--) m[(size_t) y * w + x] = 0;
    }
    if (!cut) return f;
  }
  return fill_holes(m, w, h);
}
// the prototype's clean_mask: close (notches), fill holes, open (thin bumps), keep the room-sized parts
Mask clean(const Mask &m, int w, int h, double res, double close_m, double open_m, int min, bool largest) {
  const int rc = rpx(close_m, res), ro = rpx(open_m, res);
  Mask c = erode(dilate(m, w, h, rc), w, h, rc);
  c = fill_holes(c, w, h);
  c = dilate(erode(c, w, h, ro), w, h, ro);
  return keep(c, w, h, min, largest);
}
// opening by a k x k square: the cells covered by a square that fits inside m
Mask open_square(const Mask &m, int w, int h, int k) {
  std::vector<int> I((size_t) (w + 1) * (h + 1), 0), E((size_t) (w + 1) * (h + 1), 0);
  auto at = [&](std::vector<int> &A, int x, int y) -> int & { return A[(size_t) y * (w + 1) + x]; };
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) at(I, x + 1, y + 1) = m[(size_t) y * w + x] + at(I, x, y + 1) + at(I, x + 1, y) - at(I, x, y);
  Mask e(m.size(), 0);
  for (int y = 0; y + k <= h; y++) for (int x = 0; x + k <= w; x++)
    e[(size_t) y * w + x] = at(I, x + k, y + k) - at(I, x, y + k) - at(I, x + k, y) + at(I, x, y) == k * k;
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) at(E, x + 1, y + 1) = e[(size_t) y * w + x] + at(E, x, y + 1) + at(E, x + 1, y) - at(E, x, y);
  Mask o(m.size(), 0);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    const int x0 = std::max(0, x - k + 1), y0 = std::max(0, y - k + 1);
    o[(size_t) y * w + x] = at(E, x + 1, y + 1) - at(E, x0, y + 1) - at(E, x + 1, y0) + at(E, x0, y0) > 0;
  }
  return o;
}
// every footprint cell takes the label of the nearest labelled cell within max_px (rooms meet edge to edge over
// overlays; floor far from any room stays unlabelled)
std::vector<uint16_t> tile_fill(const std::vector<uint16_t> &lab, const Mask &fp, int w, int h, double max_px = 1e9) {
  Mask site(lab.size());
  for (size_t i = 0; i < lab.size(); i++) site[i] = lab[i] > 0;
  std::vector<double> d;
  std::vector<int> near;
  edt(site, w, h, d, &near);
  std::vector<uint16_t> o(lab.size(), 0);
  for (size_t i = 0; i < lab.size(); i++) if (fp[i] && near[i] >= 0 && d[i] <= max_px * max_px) o[i] = lab[near[i]];
  return o;
}
// outer boundary of the component holding the first set cell (raster order), on cell corners, clockwise on screen
std::vector<std::pair<int, int>> trace(const Mask &m, int w, int h) {
  size_t s = 0;
  while (s < m.size() && !m[s]) s++;
  std::vector<std::pair<int, int>> out;
  if (s == m.size()) return out;
  auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && m[(size_t) y * w + x]; };
  static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};  // E S W N
  const int sx = (int) (s % w), sy = (int) (s / w);
  int cx = sx, cy = sy, d = 0;
  out.push_back({cx, cy});
  for (int guard = 0; guard < 8 * (w + 1) * (h + 1); guard++) {
    cx += DX[d], cy += DY[d];
    if (cx == sx && cy == sy) break;
    bool L, R;  // the cells ahead-left / ahead-right of the corner
    switch (d) {
      case 0: L = in(cx, cy - 1), R = in(cx, cy); break;
      case 1: L = in(cx, cy), R = in(cx - 1, cy); break;
      case 2: L = in(cx - 1, cy), R = in(cx - 1, cy - 1); break;
      default: L = in(cx - 1, cy - 1), R = in(cx, cy - 1); break;
    }
    const int nd = !R ? (d + 1) % 4 : L ? (d + 3) % 4 : d;
    if (nd != d) out.push_back({cx, cy});
    d = nd;
  }
  return out;
}

// ---------------------------------------------------------------- polygon tools (axis-aligned)
using Poly = std::vector<Pt>;
void merge_collinear(Poly &v, double eps = 1e-6) {
  bool changed = true;
  while (changed && v.size() > 4) {
    changed = false;
    const size_t n = v.size();
    for (size_t i = 0; i < n; i++) {
      const Pt &a = v[(i + n - 1) % n], &b = v[i], &c = v[(i + 1) % n];
      if ((fabs(a.x - b.x) < eps && fabs(b.x - c.x) < eps) || (fabs(a.z - b.z) < eps && fabs(b.z - c.z) < eps) ||
          (fabs(a.x - b.x) < eps && fabs(a.z - b.z) < eps)) { v.erase(v.begin() + i), changed = true; break; }
    }
  }
}
// collapse the shortest edge (< t) by moving its two neighbours to their length-weighted mean line, repeated
void rectilinear_simplify(Poly &v, double t) {
  merge_collinear(v);
  for (int it = 0; it < 2000 && v.size() > 4; it++) {
    const size_t n = v.size();
    std::vector<double> L(n);
    size_t i = 0;
    for (size_t j = 0; j < n; j++) {
      L[j] = hypot(v[(j + 1) % n].x - v[j].x, v[(j + 1) % n].z - v[j].z);
      if (L[j] < L[i]) i = j;
    }
    if (L[i] >= t) break;
    const Pt p = v[i], q = v[(i + 1) % n];
    const bool fixx = fabs(p.z - q.z) < 1e-9;  // a horizontal edge: its neighbours are vertical, move their x
    const double la = L[(i + n - 1) % n], lb = L[(i + 1) % n];
    const double val = fixx ? (p.x * la + q.x * lb) / std::max(la + lb, 1e-9) : (p.z * la + q.z * lb) / std::max(la + lb, 1e-9);
    for (size_t k : {(i + n - 1) % n, i, (i + 1) % n, (i + 2) % n}) (fixx ? v[k].x : v[k].z) = val;
    merge_collinear(v);
  }
}
// x and z values of all rooms within tol of each other become one line (shared walls line up)
void snap_lines(std::vector<Poly> &polys, double tol) {
  for (int ax = 0; ax < 2; ax++) {
    std::vector<double> vals;
    auto key = [](double v) { return std::round(v * 1e4) / 1e4; };
    for (auto &pl : polys) for (auto &p : pl) vals.push_back(key(ax ? p.z : p.x));
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
    std::map<double, double> to;
    for (size_t i = 0; i < vals.size();) {
      size_t j = i + 1;
      double sum = vals[i];
      while (j < vals.size() && vals[j] - vals[j - 1] < tol) sum += vals[j++];
      for (size_t k = i; k < j; k++) to[vals[k]] = sum / (j - i);
      i = j;
    }
    for (auto &pl : polys) for (auto &p : pl) (ax ? p.z : p.x) = to[key(ax ? p.z : p.x)];
  }
}
double area(const Poly &v) {
  double a = 0;
  for (size_t i = 0; i < v.size(); i++) a += v[i].x * v[(i + 1) % v.size()].z - v[(i + 1) % v.size()].x * v[i].z;
  return fabs(a) / 2;
}

// a lattice of grid cells over the plan (corners land on it)
struct Lat {
  double x0 = 0, z0 = 0, q = 0.05;
  int w = 0, h = 0;
  // cells whose centre is inside v (even-odd)
  Mask fill(const Poly &v) const {
    Mask m((size_t) w * h, 0);
    std::vector<double> xs;
    for (int j = 0; j < h; j++) {
      const double zc = z0 + (j + 0.5) * q;
      xs.clear();
      for (size_t i = 0; i < v.size(); i++) {
        const Pt &a = v[i], &b = v[(i + 1) % v.size()];
        if ((a.z > zc) != (b.z > zc)) xs.push_back(a.x + (zc - a.z) * (b.x - a.x) / (b.z - a.z));
      }
      std::sort(xs.begin(), xs.end());
      for (size_t k = 0; k + 1 < xs.size(); k += 2) {
        const int i0 = std::max(0, (int) ceil((xs[k] - x0) / q - 0.5)), i1 = std::min(w - 1, (int) floor((xs[k + 1] - x0) / q - 0.5));
        for (int i = i0; i <= i1; i++) m[(size_t) j * w + i] = 1;
      }
    }
    return m;
  }
  Poly poly(const Mask &m) const {
    Poly o;
    for (auto &c : trace(m, w, h)) o.push_back({x0 + c.first * q, z0 + c.second * q});
    merge_collinear(o);
    return o;
  }
};
// square erosion / dilation by r cells (Chebyshev: a mitred offset of a rectilinear outline)
Mask cheb(const Mask &m, int w, int h, int r, bool grow) {
  Mask o(m.size(), 0);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    bool all = true, any = false;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
      const int X = x + dx, Y = y + dy;
      const bool v = X >= 0 && Y >= 0 && X < w && Y < h && m[(size_t) Y * w + X];
      all = all && v, any = any || v;
    }
    o[(size_t) y * w + x] = grow ? any : all;
  }
  return o;
}
Pt centroid(const Mask &m, int w, double x0, double z0, double q) {
  double sx = 0, sz = 0, n = 0;
  for (size_t i = 0; i < m.size(); i++) if (m[i]) sx += (double) (i % w) + 0.5, sz += (double) (i / w) + 0.5, n++;
  return n ? Pt{x0 + sx / n * q, z0 + sz / n * q} : Pt{x0, z0};
}
std::string fmt(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.2f", fabs(v) < 0.005 ? 0.0 : v);
  std::string s = b;
  while (s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  return s;
}
std::string m2s(double v) { char b[24]; snprintf(b, sizeof b, "%.1f m\xc2\xb2", v); return b; }
std::string clean_name(std::string s) {
  for (char &c : s) if (c == '|' || c == ';' || c == ',' || c == ':') c = ' ';
  return s;
}
uint32_t rgb_of(JsonVariantConst v) {  // [r,g,b] or "#rrggbb"
  if (v.is<JsonArrayConst>()) return ((uint32_t) (v[0] | 0) << 16) | ((uint32_t) (v[1] | 0) << 8) | (uint32_t) (v[2] | 0);
  const char *s = v | "";
  return *s == '#' ? (uint32_t) strtoul(s + 1, nullptr, 16) : 0xFFFFFFFFu;
}
std::string hex(uint32_t c) { char b[8]; snprintf(b, sizeof b, "#%06x", c & 0xFFFFFF); return b; }
}  // namespace

// ---------------------------------------------------------------- options
Options options(JsonVariantConst o, bool shape_default) {
  namespace g = fw::fp::gen;
  Options r{g::GRID, g::GAP, g::STRAIGHTEN, g::ALIGN, g::SMOOTH, g::TRIM, g::MIN_ROOM, g::DOORWAY, g::SPILL, g::SPILL_KEEP / 100.0,
            g::WALL_REACH, g::WALL_COVER / 100.0, g::SPARE_MIN, g::SPARE_TRIM, g::SEAL, g::SPECK, shape_default, true};
  auto num = [&](const char *k, double &v) { if (o[k].is<double>()) v = o[k].as<double>(); };
  num("grid", r.grid), num("gap", r.gap), num("straighten", r.straighten), num("align", r.align), num("smooth", r.smooth);
  num("trim", r.trim), num("min_room", r.min_room), num("doorway", r.doorway), num("spill", r.spill), num("reach", r.reach);
  num("spare_min", r.spare_min), num("spare_trim", r.spare_trim), num("seal", r.seal), num("speck", r.speck);
  if (o["spill_keep"].is<double>()) r.spill_keep = o["spill_keep"].as<double>() / 100;
  if (o["cover"].is<double>()) r.cover = o["cover"].as<double>() / 100;
  if (o["shape"].is<bool>()) r.shape = o["shape"];
  if (o["spare"].is<bool>()) r.spare = o["spare"];
  return r;
}

// ---------------------------------------------------------------- adapter: a picture with one colour per room
bool from_room_colour_image(const uint8_t *px, int w, int h, JsonVariantConst opt, Input &in, std::string &err) {
  if (!px || w < 8 || h < 8) return err = "no picture", false;
  in = Input{}, in.adapter = "room_colour_image";
  const size_t N = (size_t) w * h;
  constexpr uint32_t CLEAR = 0xFF000000u;
  std::vector<uint32_t> key(N);
  for (size_t i = 0; i < N; i++) key[i] = px[i * 4 + 3] < 128 ? CLEAR : ((uint32_t) px[i * 4] << 16) | ((uint32_t) px[i * 4 + 1] << 8) | px[i * 4 + 2];
  struct C { uint32_t rgb; int n = 0, edge = 0; uint8_t cls = OVERLAY; };
  std::unordered_map<uint32_t, int> ix;
  std::vector<C> cs;
  std::vector<int> ci(N);
  for (size_t i = 0; i < N; i++) {
    auto it = ix.find(key[i]);
    if (it == ix.end()) it = ix.emplace(key[i], (int) cs.size()).first, cs.push_back(C{key[i]});
    ci[i] = it->second, cs[it->second].n++;
  }
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    const size_t i = (size_t) y * w + x;
    if ((x > 0 && ci[i - 1] != ci[i]) || (x < w - 1 && ci[i + 1] != ci[i]) || (y > 0 && ci[i - w] != ci[i]) || (y < h - 1 && ci[i + w] != ci[i])) cs[ci[i]].edge++;
  }
  // background = the commonest colour on the border (or transparent)
  std::unordered_map<int, int> border;
  for (int x = 0; x < w; x++) border[ci[x]]++, border[ci[(size_t) (h - 1) * w + x]]++;
  for (int y = 0; y < h; y++) border[ci[(size_t) y * w]]++, border[ci[(size_t) y * w + w - 1]]++;
  int bg = 0, bgn = -1;
  for (auto &b : border) if (b.second > bgn) bg = b.first, bgn = b.second;
  // the drawing's upscale: transitions between major colours fall on one phase of k
  int k = 1, phx = 0, phy = 0;
  {
    std::vector<uint8_t> major(cs.size());
    for (size_t c = 0; c < cs.size(); c++) major[c] = cs[c].n >= (int) (N / 2000);
    int hist[9][8] = {}, total = 0;
    for (int y = 0; y < h; y++) for (int x = 1; x < w; x++) {
      const size_t i = (size_t) y * w + x;
      if (ci[i] == ci[i - 1] || (ci[i] != bg && ci[i - 1] != bg) || !major[ci[i]] || !major[ci[i - 1]]) continue;  // the house's outline: always on the map grid
      total++;
      for (int kk = 2; kk <= 8; kk++) hist[kk][x % kk]++;
    }
    for (int kk = 8; kk >= 2 && total >= 50; kk--) {
      const int best = *std::max_element(hist[kk], hist[kk] + kk);
      if (best >= 0.85 * total) { k = kk, phx = (int) (std::max_element(hist[kk], hist[kk] + kk) - hist[kk]); break; }
    }
    if (k > 1) {  // the rows' phase too
      int hy[8] = {};
      for (int y = 1; y < h; y++) for (int x = 0; x < w; x++) {
        const size_t i = (size_t) y * w + x;
        if (ci[i] != ci[i - w] && (ci[i] == bg || ci[i - w] == bg) && major[ci[i]] && major[ci[i - w]]) hy[y % k]++;
      }
      phy = (int) (std::max_element(hy, hy + k) - hy);
    }
  }
  const double mmpp = opt["mm_per_px"].is<double>() ? opt["mm_per_px"].as<double>() : fw::fp::gen::MAP_PX * 1000 / k;
  const double res = mmpp / 1000;
  // what each colour is: explicit lists first, then flat + thick + colourful = room, dull + dark = wall
  auto listed = [&](const char *name, uint32_t rgb) {
    for (JsonVariantConst v : opt[name].as<JsonArrayConst>()) if (rgb_of(v) == rgb) return true;
    return false;
  };
  static const uint32_t KNOWN_FLOOR[] = {0x2073B9};  // map-parser renderers' floor that belongs to no room (Roborock/Xiaomi)
  const int min_cells = (int) (fw::fp::gen::MIN_ROOM / (res * res));
  for (size_t c = 0; c < cs.size(); c++) {
    C &o = cs[c];
    const int r = (o.rgb >> 16) & 255, g = (o.rgb >> 8) & 255, b = o.rgb & 255;
    const int hi = std::max({r, g, b}), chroma = hi - std::min({r, g, b});
    const bool thick = o.edge > 0 ? (double) o.n / o.edge >= 1.2 : o.n > 0;  // 1 px lines (path, outlines) are ~1
    if ((int) c == bg || o.rgb == CLEAR) o.cls = UNKNOWN;
    else if (listed("room_colours", o.rgb)) o.cls = ROOM;
    else if (listed("wall_colours", o.rgb)) o.cls = WALL;
    else if (listed("floor_colours", o.rgb) || std::find(std::begin(KNOWN_FLOOR), std::end(KNOWN_FLOOR), o.rgb) != std::end(KNOWN_FLOOR)) o.cls = FLOOR;
    else if (listed("ignore_colours", o.rgb)) o.cls = OVERLAY;
    else if (chroma >= 40) o.cls = thick && o.n >= min_cells ? ROOM : OVERLAY;
    else if (hi < 180) o.cls = WALL;
    else o.cls = thick && o.n >= min_cells ? FLOOR : OVERLAY;
  }
  // overlays drawn on a room (path, carpet hatching, icons) touch mostly that one room: a thin room colour whose
  // commonest neighbour is a bigger room colour is an overlay of it
  {
    std::unordered_map<uint64_t, int> nb;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      const size_t i = (size_t) y * w + x;
      if (x < w - 1 && ci[i + 1] != ci[i]) nb[((uint64_t) ci[i] << 32) | (uint32_t) ci[i + 1]]++, nb[((uint64_t) ci[i + 1] << 32) | (uint32_t) ci[i]]++;
      if (y < h - 1 && ci[i + w] != ci[i]) nb[((uint64_t) ci[i] << 32) | (uint32_t) ci[i + w]]++, nb[((uint64_t) ci[i + w] << 32) | (uint32_t) ci[i]]++;
    }
    std::vector<int> order;
    for (size_t c = 0; c < cs.size(); c++) if (cs[c].cls == ROOM && !listed("room_colours", cs[c].rgb)) order.push_back((int) c);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return cs[a].n < cs[b].n; });
    for (int c : order) {
      int top = -1, topn = 0;
      for (auto &e : nb) if ((int) (e.first >> 32) == c && e.second > topn) topn = e.second, top = (int) (uint32_t) e.first;
      const double thick = cs[c].edge ? (double) cs[c].n / cs[c].edge : 99;  // rooms that share an open edge are thick
      if (top >= 0 && cs[top].cls == ROOM && cs[top].n > cs[c].n && thick < std::max(3.0, (double) k)) cs[c].cls = OVERLAY;
    }
  }
  // crop to what isn't background
  int x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (cs[ci[(size_t) y * w + x]].cls != UNKNOWN) x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
  if (x1 < 0) return err = "the picture is all background", false;
  const int pad = 2;
  x0 = std::max(0, x0 - pad), y0 = std::max(0, y0 - pad), x1 = std::min(w - 1, x1 + pad), y1 = std::min(h - 1, y1 + pad);
  Grid &G = in.g;
  G.init(x1 - x0 + 1, y1 - y0 + 1), G.res = res, G.ox = x0 * res, G.oz = y0 * res;
  std::vector<uint16_t> lab0(G.room.size(), 0);
  Mask grow(G.room.size(), 0);
  std::vector<int> cidx(G.room.size());
  for (int y = 0; y < G.h; y++) for (int x = 0; x < G.w; x++) {
    const size_t i = (size_t) y * G.w + x, s = (size_t) (y + y0) * w + x + x0;
    const C &o = cs[ci[s]];
    G.cls[i] = o.cls, cidx[i] = ci[s];
    if (o.cls == ROOM) lab0[i] = (uint16_t) (ci[s] + 1);
    grow[i] = o.cls == ROOM || o.cls == OVERLAY || o.cls == WALL;
  }
  // rooms grow back over the overlays, then each colour's separate pieces are separate rooms (renderers reuse colours)
  //    (walls inside a room join its pieces but stay walls)
  const std::vector<uint16_t> tl = tile_fill(lab0, grow, G.w, G.h, fw::fp::gen::TRIM / res);
  std::map<uint32_t, std::string> ids;
  for (JsonVariantConst v : opt["ids_by_colour"].as<JsonArrayConst>()) ids[rgb_of(v[0])] = v[1] | "";
  struct Piece { int colour, n; size_t first; std::vector<size_t> cells; };
  std::vector<Piece> pieces;
  std::vector<int> taken(G.room.size(), 0);
  int dropped = 0;
  for (size_t c = 0; c < cs.size(); c++) {
    if (cs[c].cls != ROOM) continue;
    Mask m(G.room.size());
    for (size_t i = 0; i < m.size(); i++) m[i] = tl[i] == c + 1;
    std::vector<int> lab, sz;
    const int n = components(m, G.w, G.h, lab, sz);
    std::vector<Piece> mine(n + 1);
    for (size_t i = 0; i < m.size(); i++) if (lab[i]) { Piece &p = mine[lab[i]]; if (p.cells.empty()) p.first = i; p.cells.push_back(i); }
    for (int j = 1; j <= n; j++) {
      if (sz[j] < min_cells) { dropped++; continue; }
      mine[j].colour = (int) c, mine[j].n = sz[j], pieces.push_back(std::move(mine[j]));
    }
  }
  if (pieces.empty()) return err = "no room colours found", false;
  std::sort(pieces.begin(), pieces.end(), [](const Piece &a, const Piece &b) { return a.first < b.first; });  // top-left first
  std::map<int, int> biggest;  // colour -> index of its largest piece (gets the colour's id)
  for (size_t p = 0; p < pieces.size(); p++) {
    auto it = biggest.find(pieces[p].colour);
    if (it == biggest.end() || pieces[p].n > pieces[it->second].n) biggest[pieces[p].colour] = (int) p;
  }
  JsonVariantConst names = opt["names"];
  int seq = 0;
  std::map<std::string, int> used;
  for (size_t p = 0; p < pieces.size(); p++) {
    RoomIn r;
    r.rgb = cs[pieces[p].colour].rgb;
    auto it = ids.find(r.rgb);
    if (it != ids.end()) r.id = biggest[pieces[p].colour] == (int) p ? it->second : it->second + "." + std::to_string(++used[it->second]);
    else r.id = std::to_string(++seq);
    r.name = names[r.id].is<const char *>() ? std::string(names[r.id].as<const char *>()) : "Room " + r.id;
    in.rooms.push_back(r);
    for (size_t i : pieces[p].cells) if (G.cls[i] != WALL) G.room[i] = (uint16_t) (p + 1), G.cls[i] = ROOM;
  }
  // back to the vacuum's own map pixel (each k x k block of the drawing = one cell, by majority, overlays aside):
  // the same cells a map's JSON would give, and k*k times less work
  if (k > 1 && !(opt["native"] | false)) {
    const int lx0 = ((phx - x0) % k + k) % k - k, ly0 = ((phy - y0) % k + k) % k - k;
    Grid B;
    B.init((G.w - lx0 + k - 1) / k, (G.h - ly0 + k - 1) / k), B.res = res * k, B.ox = G.ox + lx0 * res, B.oz = G.oz + ly0 * res;
    std::map<uint16_t, int> rc;
    for (int by = 0; by < B.h; by++) for (int bx = 0; bx < B.w; bx++) {
      int cnt[5] = {};
      rc.clear();
      for (int y = ly0 + by * k; y < ly0 + (by + 1) * k; y++) for (int x = lx0 + bx * k; x < lx0 + (bx + 1) * k; x++) {
        if (x < 0 || y < 0 || x >= G.w || y >= G.h) { cnt[UNKNOWN]++; continue; }
        const size_t i = (size_t) y * G.w + x;
        cnt[G.cls[i]]++;
        if (G.room[i]) rc[G.room[i]]++;
      }
      int best = OVERLAY;
      for (int c = 0; c < 4; c++) if (cnt[c] && (best == OVERLAY || cnt[c] > cnt[best])) best = c;
      const size_t j = (size_t) by * B.w + bx;
      B.cls[j] = (uint8_t) best;
      if (best == ROOM) { int m = 0; for (auto &e : rc) if (e.second > m) m = e.second, B.room[j] = e.first; }
    }
    G = std::move(B);
  }
  // the vacuum's frame: one anchor (a pixel or the centre of a colour, e.g. the robot icon) + its vacuum position
  const bool anchored = opt["anchor"]["vac"].is<JsonArrayConst>();
  double au = 0, av = 0;
  if (anchored) {
    if (opt["anchor"]["px"].is<JsonArrayConst>()) au = opt["anchor"]["px"][0] | 0.0, av = opt["anchor"]["px"][1] | 0.0;
    else {
      const uint32_t want = rgb_of(opt["anchor"]["colour"]);
      int ax0 = w, ay0 = h, ax1 = -1, ay1 = -1;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (key[(size_t) y * w + x] == want) ax0 = std::min(ax0, x), ay0 = std::min(ay0, y), ax1 = std::max(ax1, x), ay1 = std::max(ay1, y);
      if (ax1 < 0) return err = "anchor colour not in the picture", false;
      au = (ax0 + ax1) / 2.0 + 0.5, av = (ay0 + ay1) / 2.0 + 0.5;
    }
    const double X = opt["anchor"]["vac"][0] | 0.0, Y = opt["anchor"]["vac"][1] | 0.0, u = opt["vac_per_m"] | 1000.0;  // mm by default
    const bool flip = opt["flip_y"] | true;  // vacuum y up (Roborock, Xiaomi) unless told otherwise
    in.vac[0] = 1 / u, in.vac[1] = 0, in.vac[2] = res * au - X / u;
    in.vac[3] = 0, in.vac[4] = flip ? -1 / u : 1 / u, in.vac[5] = flip ? res * av + Y / u : res * av - Y / u;
    in.has_robot = true, in.robot[0] = X, in.robot[1] = Y;
  } else {
    in.vac[0] = res, in.vac[4] = res;  // vacuum units = picture pixels
  }
  char b[160];
  snprintf(b, sizeof b, "picture %dx%d, drawn at %dx, %.1f mm a pixel, %d colours", w, h, k, mmpp, (int) cs.size());
  in.notes.push_back(b);
  int nr = 0;
  for (auto &c : cs) nr += c.cls == ROOM;
  snprintf(b, sizeof b, "%d room colours -> %d rooms (%d specks left to the rooms around them)", nr, (int) pieces.size(), dropped);
  in.notes.push_back(b);
  for (auto &c : cs) {
    if (c.n < (int) (N / 1000)) continue;
    static const char *CN[] = {"background", "floor", "wall", "room", "overlay"};
    snprintf(b, sizeof b, "%s %s (%d px, %.1f thick)", hex(c.rgb).c_str(), CN[c.cls], c.n, c.edge ? (double) c.n / c.edge : 0.0);
    in.notes.push_back(b);
  }
  return true;
}

// ---------------------------------------------------------------- adapter: Valetudo map JSON
bool from_valetudo_json(JsonVariantConst map, JsonVariantConst opt, Input &in, std::string &err) {
  in = Input{}, in.adapter = "valetudo_json";
  const double ps = map["pixelSize"] | 5.0;  // cm
  JsonArrayConst layers = map["layers"];
  if (layers.isNull()) return err = "no layers (not a ValetudoMap)", false;
  auto each = [](JsonVariantConst l, auto fn) {
    JsonArrayConst c = l["compressedPixels"];
    if (c.size()) { for (size_t i = 0; i + 2 < c.size(); i += 3) for (int k = 0; k < (int) c[i + 2]; k++) fn((int) c[i] + k, (int) c[i + 1]); }
    else { JsonArrayConst p = l["pixels"]; for (size_t i = 0; i + 1 < p.size(); i += 2) fn((int) p[i], (int) p[i + 1]); }
  };
  int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
  for (JsonVariantConst l : layers) each(l, [&](int x, int y) { x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y); });
  if (x1 < 0) return err = "empty map", false;
  const int pad = 2;
  x0 -= pad, y0 -= pad, x1 += pad, y1 += pad;
  Grid &G = in.g;
  G.init(x1 - x0 + 1, y1 - y0 + 1), G.res = ps / 100, G.ox = x0 * G.res, G.oz = y0 * G.res;
  JsonVariantConst names = opt["names"];
  for (int pass = 0; pass < 3; pass++)  // floor, then walls, then segments on top
    for (JsonVariantConst l : layers) {
      const char *t = l["type"] | "";
      const int want = !strcmp(t, "floor") ? 0 : !strcmp(t, "wall") ? 1 : !strcmp(t, "segment") ? 2 : -1;
      if (want != pass) continue;
      uint16_t r = 0;
      if (pass == 2) {
        RoomIn ri;
        ri.id = l["metaData"]["segmentId"] | "";
        if (ri.id.empty()) ri.id = std::to_string(in.rooms.size() + 1);
        const char *nm = l["metaData"]["name"] | "";
        ri.name = names[ri.id].is<const char *>() ? std::string(names[ri.id].as<const char *>()) : *nm ? std::string(nm) : "Room " + ri.id;
        in.rooms.push_back(ri), r = (uint16_t) in.rooms.size();
      }
      each(l, [&](int x, int y) {
        const size_t i = (size_t) (y - y0) * G.w + (x - x0);
        if (pass == 0) G.cls[i] = FLOOR;
        else if (pass == 1) G.cls[i] = WALL;
        else G.cls[i] = ROOM, G.room[i] = r;
      });
    }
  if (in.rooms.empty()) return err = "no segments in the map (the vacuum hasn't split it into rooms)", false;
  in.vac[0] = 0.01, in.vac[4] = 0.01;  // Valetudo coordinates are cm, y down like the map
  for (JsonVariantConst e : map["entities"].as<JsonArrayConst>()) {
    const char *t = e["type"] | "";
    JsonArrayConst p = e["points"];
    if (!strcmp(t, "robot_position") && p.size() >= 2) in.has_robot = true, in.robot[0] = p[0], in.robot[1] = p[1];
    else if (!strcmp(t, "charger_location") && p.size() >= 2) in.has_dock = true, in.dock[0] = p[0], in.dock[1] = p[1];
    else if (!strcmp(t, "path")) for (JsonVariantConst v : p) in.path.push_back(v.as<double>());
  }
  char b[120];
  snprintf(b, sizeof b, "ValetudoMap: %d segments, %.0f cm pixels, %dx%d cells", (int) in.rooms.size(), ps, G.w, G.h);
  in.notes.push_back(b);
  return true;
}

// ---------------------------------------------------------------- adapter: outlines
bool from_polygons(JsonVariantConst src, JsonVariantConst opt, Input &in, std::string &err) {
  in = Input{}, in.adapter = "polygons", in.shape = false;
  const double u = src["units_per_m"] | 1.0;
  const bool flip = src["flip_y"] | false;
  const double res = opt["res"] | 0.025;
  std::vector<Poly> polys;
  double X0 = INF, Z0 = INF, X1 = -INF, Z1 = -INF;
  for (JsonVariantConst r : src["rooms"].as<JsonArrayConst>()) {
    Poly pl;
    for (JsonVariantConst p : r["points"].as<JsonArrayConst>()) {
      const Pt q{p[0].as<double>() / u, (flip ? -1 : 1) * p[1].as<double>() / u};
      pl.push_back(q), X0 = std::min(X0, q.x), Z0 = std::min(Z0, q.z), X1 = std::max(X1, q.x), Z1 = std::max(Z1, q.z);
    }
    if (pl.size() < 3) continue;
    RoomIn ri;
    ri.id = r["id"].is<const char *>() ? r["id"].as<const char *>() : r["id"].is<int>() ? std::to_string(r["id"].as<int>()) : std::to_string(polys.size() + 1);
    ri.name = r["name"] | ("Room " + ri.id).c_str();
    in.rooms.push_back(ri), polys.push_back(pl);
  }
  if (polys.empty()) return err = "no rooms with 3 or more points", false;
  Grid &G = in.g;
  const int pad = 4;
  G.res = res, G.ox = floor(X0 / res) * res - pad * res, G.oz = floor(Z0 / res) * res - pad * res;
  G.init((int) ceil((X1 - G.ox) / res) + pad, (int) ceil((Z1 - G.oz) / res) + pad);
  Lat L{G.ox, G.oz, res, G.w, G.h};
  for (size_t r = 0; r < polys.size(); r++) {
    const Mask m = L.fill(polys[r]);
    for (size_t i = 0; i < m.size(); i++) if (m[i] && !G.room[i]) G.room[i] = (uint16_t) (r + 1), G.cls[i] = ROOM;
  }
  in.vac[0] = 1 / u, in.vac[4] = (flip ? -1 : 1) / u;
  auto pt = [&](const char *k, bool &has, double *o) { if (src[k].is<JsonArrayConst>()) has = true, o[0] = src[k][0], o[1] = src[k][1]; };
  pt("robot", in.has_robot, in.robot), pt("dock", in.has_dock, in.dock);
  in.notes.push_back(std::to_string(polys.size()) + " outlines");
  return true;
}

// ---------------------------------------------------------------- the shared cleanup
Plan generate(const Input &in, const Options &o) {
  Plan P;
  // a margin of empty cells round the map, so the morphology near its edge works as it does inside
  const int M = rpx(std::max({o.smooth, o.trim, o.seal}), in.g.res) + (int) (o.reach / in.g.res) + 2;
  Grid G;
  G.init(in.g.w + 2 * M, in.g.h + 2 * M), G.res = in.g.res, G.ox = in.g.ox - M * in.g.res, G.oz = in.g.oz - M * in.g.res;
  for (int y = 0; y < in.g.h; y++) for (int x = 0; x < in.g.w; x++) {
    const size_t i = (size_t) y * in.g.w + x, j = (size_t) (y + M) * G.w + x + M;
    G.room[j] = in.g.room[i], G.cls[j] = in.g.cls[i];
  }
  const int w = G.w, h = G.h, R = (int) in.rooms.size();
  const double res = G.res, cell = res * res;
  const size_t N = (size_t) w * h;
  auto m2 = [&](size_t cells) { return cells * cell; };
  auto gpt = [&](double gx, double gy) { return Pt{G.ox + gx * res, G.oz + gy * res}; };
  Mask wall(N);
  bool walls = false;
  for (size_t i = 0; i < N; i++) wall[i] = G.cls[i] == WALL, walls = walls || wall[i];
  const bool shape = o.shape && in.shape;
  // 0. raw: each room's traced mask as it came in (the "before" view)
  for (int r = 1; r <= R; r++) {
    Mask m(N);
    for (size_t i = 0; i < N; i++) m[i] = G.room[i] == r;
    m = keep(m, w, h, 1, true);
    Room rr{in.rooms[r - 1].id, in.rooms[r - 1].name};
    for (auto &c : trace(m, w, h)) rr.poly.push_back(gpt(c.first, c.second));
    merge_collinear(rr.poly);
    if (rr.poly.size() < 4) continue;
    std::vector<double> dd;  // label point: the cell farthest from the edge
    edt(invert(m), w, h, dd, nullptr);
    size_t best = 0;
    for (size_t i = 0; i < N; i++) if (m[i] && (!m[best] || dd[i] > dd[best])) best = i;
    rr.label = gpt(best % w + 0.5, best / w + 0.5), rr.m2 = area(rr.poly), P.raw.push_back(rr);
  }
  // 1. the house: everything seen that isn't wall, sealed and de-specked; rooms grow over it to meet
  Mask fp(N);
  for (size_t i = 0; i < N; i++) fp[i] = (G.cls[i] != UNKNOWN && G.cls[i] != WALL) || G.room[i];
  fp = clean(fp, w, h, res, o.seal, o.speck, (int) (o.min_room / cell), false);
  for (size_t i = 0; i < N; i++) fp[i] = fp[i] && G.cls[i] != UNKNOWN;
  std::vector<uint16_t> lab = tile_fill(G.room, fp, w, h, o.trim / res);
  int spills = 0, snapped = 0;
  double spill_m2 = 0, snap_max = 0;
  Mask spilled(N, 0);  // cut floor never comes back as a pocket
  if (shape) {
    // 2. doorway spill: floor that leaked through an opening narrower than `doorway` (a patio, the next room)
    const int kk = std::max(2, (int) (o.doorway / res));
    for (int r = 1; r <= R; r++) {
      Mask m(N);
      size_t n = 0;
      for (size_t i = 0; i < N; i++) m[i] = lab[i] == r, n += m[i];
      if (!n) continue;
      const Mask op = open_square(m, w, h, kk);
      size_t no = 0;
      for (size_t i = 0; i < N; i++) no += op[i];
      if (no < o.spill_keep * n) continue;
      Mask rest(N);
      for (size_t i = 0; i < N; i++) rest[i] = m[i] && !op[i];
      std::vector<int> cl, sz;
      const int nc = components(rest, w, h, cl, sz);
      for (int c = 1; c <= nc; c++) {
        if (m2(sz[c]) < o.spill) continue;
        Mask piece(N);
        for (size_t i = 0; i < N; i++) if (cl[i] == c) piece[i] = 1, lab[i] = 0, spilled[i] = 1;
        const Pt at = centroid(piece, w, G.ox, G.oz, res);
        P.cuts.push_back({in.rooms[r - 1].name, "spill", m2(sz[c]), at}), spills++, spill_m2 += m2(sz[c]);
      }
    }
    // 3. walls: each side of a room's box moves out (within reach) to the first wall line along >= cover of it;
    //    never-seen cells inside the new box join the room (furniture hides floor, not walls)
    if (walls) {
      const int reach = (int) (o.reach / res);
      std::vector<uint16_t> out = lab;
      for (int r = 1; r <= R; r++) {
        int bx0 = w, by0 = h, bx1 = -1, by1 = -1;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (lab[(size_t) y * w + x] == r) bx0 = std::min(bx0, x), by0 = std::min(by0, y), bx1 = std::max(bx1, x), by1 = std::max(by1, y);
        if (bx1 < 0) continue;
        auto colfrac = [&](int x) { int n = 0; for (int y = by0; y <= by1; y++) n += wall[(size_t) y * w + x]; return (double) n / (by1 - by0 + 1); };
        auto rowfrac = [&](int y) { int n = 0; for (int x = bx0; x <= bx1; x++) n += wall[(size_t) y * w + x]; return (double) n / (bx1 - bx0 + 1); };
        auto find = [&](int from, int dir, int lim, bool col) {
          for (int k = 0, v = from; k < reach && v >= 0 && v < lim; k++, v += dir) if ((col ? colfrac(v) : rowfrac(v)) >= o.cover) return k;
          return -1;
        };
        int k;
        const int nbx1 = (k = find(bx1, 1, w, true)) > 0 ? bx1 + k - 1 : bx1;
        const int nbx0 = (k = find(bx0, -1, w, true)) > 0 ? bx0 - k + 1 : bx0;
        const int nby1 = (k = find(by1, 1, h, false)) > 0 ? by1 + k - 1 : by1;
        const int nby0 = (k = find(by0, -1, h, false)) > 0 ? by0 - k + 1 : by0;
        const int moved = std::max({bx0 - nbx0, nbx1 - bx1, by0 - nby0, nby1 - by1});
        if (moved > 0) snapped++, snap_max = std::max(snap_max, moved * res);
        // never-seen pockets in the box join the room only when walls and rooms close them in (a sofa's shadow),
        // not open space beside an L-shaped room
        const int bw = nbx1 - nbx0 + 1, bh = nby1 - nby0 + 1;
        Mask fr((size_t) bw * bh);
        for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) { const size_t i = (size_t) (y + nby0) * w + x + nbx0; fr[(size_t) y * bw + x] = !lab[i] && !wall[i] && !spilled[i]; }
        // a side of the box that runs along a wall the vacuum saw closes the pockets against it (a sofa in a corner)
        auto wallish = [&](bool col, int v) { double m = 0; for (int d = -2; d <= 2; d++) { const int u = v + d; if (u >= 0 && u < (col ? w : h)) m = std::max(m, col ? colfrac(u) : rowfrac(u)); } return m >= o.cover; };
        const bool side[4] = {wallish(true, nbx0 - 1), wallish(true, nbx1 + 1), wallish(false, nby0 - 1), wallish(false, nby1 + 1)};  // left right top bottom
        std::vector<int> cl, sz;
        const int nc = components(fr, bw, bh, cl, sz);
        std::vector<int> closed(nc + 1, 0), open(nc + 1, 0);
        size_t rn = 0;
        for (size_t i = 0; i < N; i++) rn += lab[i] == r;
        for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
          const int c = cl[(size_t) y * bw + x];
          if (!c) continue;
          const int X = x + nbx0, Y = y + nby0;
          const int nb[4][2] = {{X - 1, Y}, {X + 1, Y}, {X, Y - 1}, {X, Y + 1}};
          for (auto &q : nb) {
            const int lx = q[0] - nbx0, ly = q[1] - nby0;
            if (lx >= 0 && ly >= 0 && lx < bw && ly < bh && cl[(size_t) ly * bw + lx] == c) continue;
            const bool ok = (q[0] < nbx0 && side[0]) || (q[0] > nbx1 && side[1]) || (q[1] < nby0 && side[2]) || (q[1] > nby1 && side[3]) ||
                            (q[0] >= 0 && q[1] >= 0 && q[0] < w && q[1] < h && (lab[(size_t) q[1] * w + q[0]] == r || wall[(size_t) q[1] * w + q[0]]));  // another room = open: a gap between rooms isn't furniture
            (ok ? closed : open)[c]++;
          }
        }
        for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
          const int c = cl[(size_t) y * bw + x];
          if (c && open[c] <= 0.2 * (open[c] + closed[c]) && sz[c] <= 0.25 * rn) out[(size_t) (y + nby0) * w + x + nbx0] = (uint16_t) r;
        }
      }
      lab = out;
    }
    Mask any(N);
    for (size_t i = 0; i < N; i++) any[i] = lab[i] > 0;
    any = clean(any, w, h, res, o.seal, o.speck, (int) (o.min_room / cell), false);
    lab = tile_fill(lab, any, w, h, o.trim / res);
  }
  // 4. each room: close notches, fill holes, cut thin bumps + islands (reported), trace, straighten
  std::vector<Poly> polys;
  std::vector<int> pr;  // polys -> room index
  int islands = 0, trims = 0, raw_corners = 0;
  double island_m2 = 0, trim_m2 = 0;
  for (const Room &r : P.raw) raw_corners += (int) r.poly.size();
  for (int r = 1; r <= R; r++) {
    Mask m(N);
    size_t n = 0;
    for (size_t i = 0; i < N; i++) m[i] = lab[i] == r, n += m[i];
    const Mask c = clean(m, w, h, res, o.smooth, o.trim, 1, true);
    size_t nc = 0;
    for (size_t i = 0; i < N; i++) nc += c[i];
    if (m2(nc) < o.min_room) {
      if (n) P.cuts.push_back({in.rooms[r - 1].name, "small", m2(n), centroid(m, w, G.ox, G.oz, res)});
      continue;
    }
    Mask rest(N);
    for (size_t i = 0; i < N; i++) rest[i] = m[i] && !c[i];
    std::vector<int> cl, sz;
    const int k = components(rest, w, h, cl, sz);
    const Mask near = dilate(c, w, h, 1);
    for (int j = 1; j <= k; j++) {
      if (m2(sz[j]) < 0.25) continue;  // crumbs: not worth a line in the report
      Mask piece(N);
      bool touches = false;
      for (size_t i = 0; i < N; i++) if (cl[i] == j) piece[i] = 1, touches = touches || near[i];
      const bool isl = !touches;
      P.cuts.push_back({in.rooms[r - 1].name, isl ? "island" : "trim", m2(sz[j]), centroid(piece, w, G.ox, G.oz, res)});
      (isl ? islands : trims)++, (isl ? island_m2 : trim_m2) += m2(sz[j]);
    }
    Poly pl;
    for (auto &q : trace(c, w, h)) pl.push_back(gpt(q.first, q.second));
    rectilinear_simplify(pl, o.straighten);
    polys.push_back(pl), pr.push_back(r - 1);
  }
  int straight_corners = 0;
  for (auto &pl : polys) straight_corners += (int) pl.size();
  // 5. shared walls line up
  if (!polys.empty()) snap_lines(polys, o.align);
  // 6. on the grid, a gap between rooms: cells owned by one room each (a disputed cell goes to the room the map had
  //    there), each room shrunk by half the gap (square = a mitred offset), traced again
  double X0 = INF, Z0 = INF, X1 = -INF, Z1 = -INF;
  for (auto &pl : polys) for (auto &p : pl) X0 = std::min(X0, p.x), Z0 = std::min(Z0, p.z), X1 = std::max(X1, p.x), Z1 = std::max(Z1, p.z);
  // the house box (for spare space): rooms grown sideways to walls seen within reach, in the rooms' rows
  double hx0 = X0, hx1 = X1, hz0 = Z0, hz1 = Z1;
  if (shape && walls && !polys.empty()) {
    int gx0 = w, gx1 = -1, gy0 = h, gy1 = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (lab[(size_t) y * w + x]) gx0 = std::min(gx0, x), gx1 = std::max(gx1, x), gy0 = std::min(gy0, y), gy1 = std::max(gy1, y);
    const int g = (int) (o.reach / res);
    int cx0 = gx0, cx1 = gx1;
    for (int y = gy0; y <= gy1; y++) for (int x = std::max(0, gx0 - g); x < std::min(w, gx1 + g); x++) if (wall[(size_t) y * w + x]) cx0 = std::min(cx0, x), cx1 = std::max(cx1, x);
    hx0 = std::min(X0, G.ox + cx0 * res), hx1 = std::max(X1, G.ox + (cx1 + 1) * res);
  }
  const double q = o.grid;
  Lat L;
  L.q = q, L.x0 = floor(std::min(X0, hx0) / q) * q - 2 * q, L.z0 = floor(std::min(Z0, hz0) / q) * q - 2 * q;
  L.w = (int) ceil((std::max(X1, hx1) - L.x0) / q) + 3, L.h = (int) ceil((std::max(Z1, hz1) - L.z0) / q) + 3;
  if (polys.empty()) L.w = L.h = 1;
  const size_t LN = (size_t) L.w * L.h;
  std::vector<int> own(LN, -1);
  auto map_room = [&](size_t li) {
    const double x = L.x0 + (li % L.w + 0.5) * q, z = L.z0 + (li / L.w + 0.5) * q;
    const int gx = (int) floor((x - G.ox) / res), gy = (int) floor((z - G.oz) / res);
    return gx >= 0 && gy >= 0 && gx < w && gy < h ? (int) lab[(size_t) gy * w + gx] - 1 : -1;
  };
  for (size_t k = 0; k < polys.size(); k++) {
    const Mask m = L.fill(polys[k]);
    for (size_t i = 0; i < LN; i++) {
      if (!m[i]) continue;
      if (own[i] < 0 || map_room(i) == pr[k]) own[i] = (int) k;
    }
  }
  const int half = std::max(0, (int) lround(o.gap / 2 / q));
  std::vector<Mask> fin(polys.size());
  Mask all(LN, 0);
  for (size_t k = 0; k < polys.size(); k++) {
    Mask m(LN);
    for (size_t i = 0; i < LN; i++) m[i] = own[i] == (int) k;
    Mask other(LN);
    for (size_t i = 0; i < LN; i++) other[i] = own[i] >= 0 && own[i] != (int) k;
    m = solid(cheb(m, L.w, L.h, half, false), L.w, L.h, other);
    Room rr{in.rooms[pr[k]].id, in.rooms[pr[k]].name};
    rr.poly = L.poly(m);
    if (rr.poly.size() < 4 || area(rr.poly) < o.min_room) {  // shrunk to nothing on the grid: reported, not a room
      P.cuts.push_back({rr.name, "small", rr.poly.size() >= 4 ? area(rr.poly) : 0, centroid(m, L.w, L.x0, L.z0, q)});
      continue;
    }
    // label point: the cell farthest from the edge
    std::vector<double> d;
    edt(invert(m), L.w, L.h, d, nullptr);
    size_t best = 0;
    for (size_t i = 0; i < LN; i++) if (m[i] && (!m[best] || d[i] > d[best])) best = i;
    rr.label = {L.x0 + (best % L.w + 0.5) * q, L.z0 + (best / L.w + 0.5) * q};
    rr.m2 = area(rr.poly);
    for (size_t i = 0; i < LN; i++) all[i] = all[i] || m[i];
    fin[k] = m;
    P.rooms.push_back(rr);
  }
  // 7. spare space: inside the house box, not a room (+ the gap), slivers dropped -> proposed rooms it can't clean
  double spare_m2 = 0;
  if (o.spare && !P.rooms.empty()) {
    const int g2 = std::max(1, (int) lround(o.gap / q));
    const Mask U = cheb(all, L.w, L.h, g2, true);
    Mask rest(LN, 0);
    const int i0 = (int) lround((hx0 - L.x0) / q) + g2, i1 = (int) lround((hx1 - L.x0) / q) - g2;
    const int j0 = (int) lround((hz0 - L.z0) / q) + g2, j1 = (int) lround((hz1 - L.z0) / q) - g2;
    for (int j = std::max(0, j0); j < std::min(L.h, j1); j++) for (int i = std::max(0, i0); i < std::min(L.w, i1); i++) rest[(size_t) j * L.w + i] = !U[(size_t) j * L.w + i];
    rest = open_square(rest, L.w, L.h, std::max(1, (int) lround(2 * o.spare_trim / q)));
    std::vector<int> cl, sz;
    const int n = components(rest, L.w, L.h, cl, sz);
    // a proposal needs evidence: rooms or walls the vacuum saw close it in (not just an empty corner of the box)
    Mask wl(LN, 0);
    if (walls) {
      const Mask wn = dilate(wall, w, h, std::max(1, (int) ceil(o.gap * 1.5 / res)));
      for (size_t i = 0; i < LN; i++) {
        const double x = L.x0 + (i % L.w + 0.5) * q, z = L.z0 + (i / L.w + 0.5) * q;
        const int gx = (int) floor((x - G.ox) / res), gy = (int) floor((z - G.oz) / res);
        wl[i] = gx >= 0 && gy >= 0 && gx < w && gy < h && wn[(size_t) gy * w + gx];
      }
    }
    char nm[16];
    for (int c = 1; c <= n; c++) {
      if (sz[c] * q * q < o.spare_min) continue;
      Mask m(LN);
      for (size_t i = 0; i < LN; i++) m[i] = cl[i] == c;
      const Mask ring = cheb(m, L.w, L.h, g2 + 1, true);
      int shut = 0, all_n = 0;
      for (size_t i = 0; i < LN; i++) if (ring[i] && !m[i]) all_n++, shut += U[i] || wl[i];
      if (shut < 0.7 * all_n) continue;
      Poly pl = L.poly(m);
      rectilinear_simplify(pl, o.straighten);
      Mask s = L.fill(pl);
      for (size_t i = 0; i < LN; i++) s[i] = s[i] && rest[i];
      s = solid(s, L.w, L.h, U);
      Room rr;
      snprintf(nm, sizeof nm, "Room %c", (char) ('A' + std::min<size_t>(P.spare.size(), 25)));
      rr.name = nm, rr.poly = L.poly(s);
      if (rr.poly.size() < 4) continue;
      rr.m2 = area(rr.poly), spare_m2 += rr.m2;
      P.spare.push_back(rr);
    }
  }
  // 8. checks: valid, on the grid, no overlaps, every room there, plausible sizes
  {
    std::vector<int> cnt(LN, 0);
    std::vector<const Room *> every;
    for (auto &r : P.rooms) every.push_back(&r);
    for (auto &r : P.spare) every.push_back(&r);
    for (const Room *r : every) {
      const Mask m = L.fill(r->poly);
      for (size_t i = 0; i < LN; i++) cnt[i] += m[i];
      for (size_t i = 0; i < r->poly.size(); i++) {
        const Pt &a = r->poly[i], &b = r->poly[(i + 1) % r->poly.size()];
        if (fabs(a.x - b.x) > 1e-9 && fabs(a.z - b.z) > 1e-9) { P.checks.push_back(r->name + ": a slanted edge"); break; }
        if (fabs(a.x / q - lround(a.x / q)) > 1e-6 || fabs(a.z / q - lround(a.z / q)) > 1e-6) { P.checks.push_back(r->name + ": off the grid"); break; }
      }
    }
    for (size_t i = 0; i < LN; i++) if (cnt[i] > 1) { P.checks.push_back("rooms overlap"); break; }
    for (auto &r : P.rooms) if (r.m2 < o.min_room || r.m2 > 400) P.checks.push_back(r.name + ": implausible size " + m2s(r.m2));
    for (const RoomIn &ri : in.rooms) {
      bool found = false;
      for (auto &r : P.rooms) found = found || r.id == ri.id;
      bool small = false;
      for (auto &c : P.cuts) small = small || (c.kind == "small" && c.room == ri.name);
      if (!found && !small) P.checks.push_back(ri.name + ": missing");
    }
  }
  // 9. the plan's own origin = its rear-left corner; transform = vacuum units -> plan metres
  double gx = INF, gz = INF, ex = -INF, ez = -INF;
  for (auto *v : {&P.rooms, &P.spare}) for (auto &r : *v) for (auto &p : r.poly) gx = std::min(gx, p.x), gz = std::min(gz, p.z), ex = std::max(ex, p.x), ez = std::max(ez, p.z);
  if (gx >= INF) gx = gz = ex = ez = 0;
  auto sh = [&](Pt &p) { p.x = std::round((p.x - gx) * 1e4) / 1e4, p.z = std::round((p.z - gz) * 1e4) / 1e4; };
  for (auto *v : {&P.rooms, &P.spare}) for (auto &r : *v) { for (auto &p : r.poly) sh(p); sh(r.label); }
  for (auto &c : P.cuts) sh(c.at);
  P.W = ex - gx, P.D = ez - gz;
  double rx = INF, rz = INF;
  for (auto &r : P.raw) for (auto &p : r.poly) rx = std::min(rx, p.x), rz = std::min(rz, p.z);
  for (auto &r : P.raw) { for (auto &p : r.poly) p.x -= rx, p.z -= rz; r.label.x -= rx, r.label.z -= rz; }
  for (int i = 0; i < 6; i++) P.tf_raw[i] = in.vac[i];
  P.tf_raw[2] -= rx, P.tf_raw[5] -= rz;
  for (int i = 0; i < 6; i++) P.tf[i] = in.vac[i];
  P.tf[2] -= gx, P.tf[5] -= gz;
  auto tp = [&](const double *v) { return Pt{P.tf[0] * v[0] + P.tf[1] * v[1] + P.tf[2], P.tf[3] * v[0] + P.tf[4] * v[1] + P.tf[5]}; };
  if (in.has_robot) P.has_robot = true, P.robot = tp(in.robot);
  if (in.has_dock) P.has_dock = true, P.dock = tp(in.dock);
  // the steps, in words (the foundation page and the editor's check step show them)
  char b[200];
  snprintf(b, sizeof b, "%d rooms, %d corners as the map draws them", (int) P.raw.size(), raw_corners);
  P.steps.push_back({"Traced", b});
  if (shape) {
    snprintf(b, sizeof b, spills ? "%d piece%s that leaked through a doorway, %s" : "nothing leaked through a doorway", spills, spills == 1 ? "" : "s", m2s(spill_m2).c_str());
    P.steps.push_back({"Doorway spill cut", b});
    snprintf(b, sizeof b, walls ? (snapped ? "%d room%s moved out to the walls it saw, up to %.2f m" : "already on its walls") : "no walls in the map", snapped, snapped == 1 ? "" : "s", snap_max);
    P.steps.push_back({"Out to the walls", b});
  }
  snprintf(b, sizeof b, "%d island%s (%s) and %d thin bump%s (%s) cut", islands, islands == 1 ? "" : "s", m2s(island_m2).c_str(), trims, trims == 1 ? "" : "s", m2s(trim_m2).c_str());
  P.steps.push_back({"Islands and bumps", b});
  int corners = 0;
  for (auto &r : P.rooms) corners += (int) r.poly.size();
  snprintf(b, sizeof b, "%d corners -> %d (edges under %.0f cm folded into straight walls)", raw_corners, straight_corners, o.straighten * 100);
  P.steps.push_back({"Straightened", b});
  snprintf(b, sizeof b, "walls within %.0f cm lined up, corners on a %.0f cm grid, %.0f cm between rooms: %d corners", o.align * 100, q * 100, o.gap * 100, corners);
  P.steps.push_back({"Lined up", b});
  if (o.spare) {
    snprintf(b, sizeof b, P.spare.empty() ? "no space left over" : "%d proposed room%s it can't clean, %s", (int) P.spare.size(), P.spare.size() == 1 ? "" : "s", m2s(spare_m2).c_str());
    P.steps.push_back({"Spare space", b});
  }
  P.steps.push_back({"Checks", P.checks.empty() ? "valid outlines, no overlaps, every room there" : P.checks.front()});
  return P;
}

std::string plan_string(const std::vector<Room> &rooms, const std::vector<Room> &spare) {
  std::string s;
  auto add = [&](const Room &r, bool lab) {
    if (!s.empty()) s += ';';
    s += clean_name(r.name) + '|' + (lab ? clean_name(r.id) : "") + '|';
    for (size_t i = 0; i < r.poly.size(); i++) s += (i ? " " : "") + fmt(r.poly[i].x) + ',' + fmt(r.poly[i].z);
    s += '|';
    if (lab) s += fmt(r.label.x) + ',' + fmt(r.label.z);
  };
  for (auto &r : rooms) add(r, true);
  for (auto &r : spare) add(r, false);
  return s;
}
std::string transform_string(const double tf[6]) {
  char b[160];
  snprintf(b, sizeof b, "%.6g,%.6g,%.6g,%.6g,%.6g,%.6g", tf[0], tf[1], tf[2], tf[3], tf[4], tf[5]);
  return b;
}
void to_json(const Input &in, const Plan &p, JsonObject o) {
  o["adapter"] = in.adapter;
  o["plan"] = plan_string(p.rooms, p.spare);
  o["plan_raw"] = plan_string(p.raw, {});
  o["transform"] = transform_string(p.tf);
  JsonArray sz = o["size"].to<JsonArray>();
  sz.add(std::round(p.W * 100) / 100), sz.add(std::round(p.D * 100) / 100);
  JsonArray rs = o["rooms"].to<JsonArray>();
  for (auto &r : p.rooms) {
    JsonObject x = rs.add<JsonObject>();
    x["id"] = r.id, x["name"] = r.name, x["m2"] = std::round(r.m2 * 100) / 100, x["corners"] = r.poly.size();
    JsonArray pts = x["poly"].to<JsonArray>();
    for (auto &q : r.poly) { JsonArray a = pts.add<JsonArray>(); a.add(q.x), a.add(q.z); }
  }
  JsonArray sp = o["spare"].to<JsonArray>();
  for (auto &r : p.spare) {
    JsonObject x = sp.add<JsonObject>();
    x["name"] = r.name, x["m2"] = std::round(r.m2 * 100) / 100;
    JsonArray pts = x["poly"].to<JsonArray>();
    for (auto &q : r.poly) { JsonArray a = pts.add<JsonArray>(); a.add(q.x), a.add(q.z); }
  }
  JsonArray cs = o["cuts"].to<JsonArray>();
  for (auto &c : p.cuts) {
    JsonObject x = cs.add<JsonObject>();
    x["room"] = c.room, x["kind"] = c.kind, x["m2"] = std::round(c.m2 * 100) / 100;
    JsonArray a = x["at"].to<JsonArray>();
    a.add(std::round(c.at.x * 100) / 100), a.add(std::round(c.at.z * 100) / 100);
  }
  auto pt = [&](const char *k, bool has, Pt v) { if (has) { JsonArray a = o[k].to<JsonArray>(); a.add(std::round(v.x * 1000) / 1000), a.add(std::round(v.z * 1000) / 1000); } };
  pt("robot", p.has_robot, p.robot), pt("dock", p.has_dock, p.dock);
  o["transform_raw"] = transform_string(p.tf_raw);
  if (p.has_dock) pt("dock_raw", true, Pt{p.tf_raw[0] * in.dock[0] + p.tf_raw[1] * in.dock[1] + p.tf_raw[2], p.tf_raw[3] * in.dock[0] + p.tf_raw[4] * in.dock[1] + p.tf_raw[5]});
  if (p.has_robot) { char b[64]; snprintf(b, sizeof b, "%.6g,%.6g", in.robot[0], in.robot[1]); o["robot_src"] = b; }
  std::string trail;
  for (size_t i = 0; i + 1 < in.path.size(); i += 2) { char b[48]; snprintf(b, sizeof b, "%s%.6g,%.6g", i ? " " : "", in.path[i], in.path[i + 1]); trail += b; }
  o["trail"] = trail;
  JsonArray st = o["steps"].to<JsonArray>();
  for (auto &s : p.steps) { JsonObject x = st.add<JsonObject>(); x["name"] = s.name, x["detail"] = s.detail; }
  JsonArray ck = o["checks"].to<JsonArray>();
  for (auto &c : p.checks) ck.add(c);
  JsonArray nt = o["notes"].to<JsonArray>();
  for (auto &n : in.notes) nt.add(n);
}

std::string run(const char *opts_json, const char *text, const uint8_t *rgba, int w, int h) {
  JsonDocument od, src, out;
  std::string err;
  if (opts_json && *opts_json && deserializeJson(od, opts_json)) err = "options are not JSON";
  JsonVariantConst opt = od.as<JsonVariantConst>();
  const std::string ad = opt["adapter"] | "";
  Input in;
  bool ok = false;
  if (err.empty()) {
    if (ad == "room_colour_image") ok = from_room_colour_image(rgba, w, h, opt, in, err);
    else if (ad == "valetudo_json" || ad == "polygons") {
      if (!text || deserializeJson(src, text)) err = "the source is not JSON";
      else ok = ad == "polygons" ? from_polygons(src.as<JsonVariantConst>(), opt, in, err) : from_valetudo_json(src.as<JsonVariantConst>(), opt, in, err);
    } else err = "unknown adapter '" + ad + "'";
  }
  if (!ok) out["error"] = err;
  else to_json(in, generate(in, options(opt, in.shape)), out.to<JsonObject>());
  std::string s;
  serializeJson(out, s);
  return s;
}
}  // namespace fw::fpa

extern "C" EMSCRIPTEN_KEEPALIVE const char *fw_fpgen(const char *opts, const char *text, const uint8_t *rgba, int w, int h) {
  static std::string out;
  out = fw::fpa::run(opts, text, rgba, w, h);
  return out.c_str();
}
