// theme_file.cpp: theme files -> Skin (theme_file.h). Three steps, each one function below:
//   read()     strict JSON (RFC 8259: no comments, no trailing commas, no duplicate keys, valid UTF-8, \u escapes),
//              size, depth and value caps first; a flat node array + one string arena, both bounded by the file size
//   phase1()   every key checked against the schema (theme_gen.h, from tokens/theme.json): names, types, ranges,
//              floors, caps; every error collected with its path and line (up to MAX_ERRORS)
//   compile()  Facet's defaults <- the file (colours, accents, sizes, type, fonts, shapes) <- presets <- parts, state
//              blocks merged per selector, lengths to px at both densities, the palettes per mode x accent set
//   guard()    the static guards on the compiled Skin (C47): contrast (declared pairs + part pairs, RGB565 and exact,
//              the worse decides), press feedback (C36), geometry (radius, edges inside padding, patterned corners)
// Nothing here throws, aborts or touches LVGL; a refused file returns null and leaves no trace.
#include "theme_file.h"
#include "theme_gen.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <ctype.h>
#include <atomic>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace fx {
namespace {
namespace G = fx::tgen;
using namespace fx::skin;

static_assert(G::N_COLOURS == N_COLOURS && G::N_ROLES == N_ROLES && G::N_SIZES == N_SIZES && G::N_TYPES == N_TYPES &&
              G::N_FONT_ROLES == N_FONT_ROLES && G::N_PARTS == N_PARTS && G::N_STATES == N_STATES &&
              G::ST_ON == 0 && G::ST_FOCUS == 1 && G::ST_PRESSED == 2 && G::ST_DISABLED == 3 && G::ACCENTS_MAX <= MAX_SETS && G::MAX_EDGES_TOTAL <= MAX_EDGES &&
              G::MAX_BUTTONS <= MAX_BUTTONS && G::N_SETS <= MAX_SETS,
              "theme_file.h sizes no longer match the generated theme schema (tools/gen_theme.mjs): update theme_file.h");
static_assert(G::N_KEYS == 14, "theme_file.h Key bits no longer match tokens/theme.json keys");
static_assert(std::is_trivially_copyable<Skin>::value, "Skin must stay plain data (digest, memset)");
constexpr int MAX_NODES = G::MAX_VALUES;  // tokens/theme.json limits.values: nodes x 24 B + the arena (<= the file + 1 per string) + the model < 64 KB

// ---------------------------------------------------------------- memory: PSRAM on the panel (theme engine S6)
size_t g_now = 0, g_peak = 0, g_base = 0;  // bytes the parser holds now / at most during the last load (theme_peak_bytes)
template <class T, bool COUNT = true> struct Alloc {  // COUNT = transient (the parse); the Skin is not counted (S6 budgets it apart)
  using value_type = T;
  template <class U> struct rebind { using other = Alloc<U, COUNT>; };
  Alloc() = default;
  template <class U> Alloc(const Alloc<U, COUNT> &) {}
  T *allocate(size_t n) {
    if (COUNT) { g_now += n * sizeof(T); if (g_now > g_peak) g_peak = g_now; }
#ifdef ESP_PLATFORM
    if (void *p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) return static_cast<T *>(p);
#endif
    return std::allocator<T>().allocate(n);
  }
  void deallocate(T *p, size_t n) {
    if (COUNT) g_now -= n * sizeof(T);
#ifdef ESP_PLATFORM
    if (heap_caps_get_allocated_size(p)) { heap_caps_free(p); return; }
#endif
    std::allocator<T>().deallocate(p, n);
  }
  template <class U> bool operator==(const Alloc<U, COUNT> &) const { return true; }
  template <class U> bool operator!=(const Alloc<U, COUNT> &) const { return false; }
};

// ---------------------------------------------------------------- strict JSON reader
enum T : uint8_t { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };
struct Node {
  uint32_t off = 0;           // source offset of the value (numbers are read again from here; errors give its line)
  uint32_t key = 0;           // arena offset of the key (in an object)
  union { uint32_t s; int32_t first; };  // a string: its arena offset; a list / object: its first child (-1 = none)
  int32_t next = -1;          // next sibling
  uint16_t kl = 0, sl = 0;
  uint8_t t = J_NULL, b = 0;
};
static_assert(sizeof(Node) == 24, "Node size (the transient memory cap assumes 24 B)");

const char *const TOO_MANY = "more values than a theme may hold (memory cap, C48)";
struct Doc {
  const char *src = nullptr;
  size_t n = 0;
  std::vector<Node, Alloc<Node>> nodes;
  std::vector<char, Alloc<char>> arena;
  size_t p = 0;
  std::string err;  // the first syntax error, with line and column
  uint32_t err_off = 0;
  bool fail(const char *m) {
    if (err.empty()) { err = m; err_off = (uint32_t) p; }
    return false;
  }
  void ws() { while (p < n && (src[p] == ' ' || src[p] == '\t' || src[p] == '\n' || src[p] == '\r')) p++; }
  int add(uint8_t t) {
    if ((int) nodes.size() >= MAX_NODES) return -1;
    Node x; x.t = t; x.off = (uint32_t) p; x.first = -1;
    nodes.push_back(x);
    return (int) nodes.size() - 1;
  }
  void put_utf8(uint32_t c) {
    if (c < 0x80) arena.push_back((char) c);
    else if (c < 0x800) { arena.push_back((char) (0xC0 | c >> 6)); arena.push_back((char) (0x80 | (c & 63))); }
    else if (c < 0x10000) { arena.push_back((char) (0xE0 | c >> 12)); arena.push_back((char) (0x80 | (c >> 6 & 63))); arena.push_back((char) (0x80 | (c & 63))); }
    else { arena.push_back((char) (0xF0 | c >> 18)); arena.push_back((char) (0x80 | (c >> 12 & 63))); arena.push_back((char) (0x80 | (c >> 6 & 63))); arena.push_back((char) (0x80 | (c & 63))); }
  }
  bool hex4(uint32_t &v) {
    v = 0;
    for (int i = 0; i < 4; i++) {
      if (p >= n) return fail("unterminated \\u escape");
      const char c = src[p++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= (uint32_t) (c - '0');
      else if (c >= 'a' && c <= 'f') v |= (uint32_t) (c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= (uint32_t) (c - 'A' + 10);
      else { p--; return fail("\\u needs 4 hex digits"); }
    }
    return true;
  }
  // a string at p (the opening quote): unescaped into the arena; out = arena offset, len
  bool str(uint32_t &out, uint16_t &len) {
    p++;  // "
    const size_t start = arena.size();
    for (;;) {
      if (p >= n) return fail("unterminated string");
      const unsigned char c = (unsigned char) src[p];
      if (c == '"') { p++; break; }
      if (c < 0x20) return fail("a control character in a string (write it as an escape)");
      if (c == '\\') {
        p++;
        if (p >= n) return fail("unterminated string");
        const char e = src[p++];
        switch (e) {
          case '"': arena.push_back('"'); break;
          case '\\': arena.push_back('\\'); break;
          case '/': arena.push_back('/'); break;
          case 'b': arena.push_back('\b'); break;
          case 'f': arena.push_back('\f'); break;
          case 'n': arena.push_back('\n'); break;
          case 'r': arena.push_back('\r'); break;
          case 't': arena.push_back('\t'); break;
          case 'u': {
            uint32_t v;
            if (!hex4(v)) return false;
            if (v >= 0xD800 && v <= 0xDBFF) {  // a surrogate pair
              if (p + 1 >= n || src[p] != '\\' || src[p + 1] != 'u') return fail("a lone surrogate in a \\u escape");
              p += 2;
              uint32_t lo;
              if (!hex4(lo)) return false;
              if (lo < 0xDC00 || lo > 0xDFFF) return fail("a lone surrogate in a \\u escape");
              v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
            } else if (v >= 0xDC00 && v <= 0xDFFF) return fail("a lone surrogate in a \\u escape");
            if (v == 0) return fail("\\u0000 is not allowed in a theme");
            put_utf8(v);
            break;
          }
          default: p--; return fail("an unknown escape (\\\" \\\\ \\/ \\b \\f \\n \\r \\t \\uXXXX)");
        }
        continue;
      }
      if (c < 0x80) { arena.push_back((char) c); p++; continue; }
      // UTF-8: the shortest form only, no surrogates, at most U+10FFFF
      int k = c >= 0xF0 && c <= 0xF4 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 && c <= 0xDF ? 1 : -1;
      if (k < 0 || (c >= 0xF5)) return fail("invalid UTF-8 in a string");
      if (p + (size_t) k >= n) return fail("invalid UTF-8 in a string");
      uint32_t v = c & (k == 1 ? 0x1F : k == 2 ? 0x0F : 0x07);
      for (int i = 1; i <= k; i++) {
        const unsigned char d = (unsigned char) src[p + i];
        if ((d & 0xC0) != 0x80) return fail("invalid UTF-8 in a string");
        v = v << 6 | (d & 63);
      }
      if ((k == 2 && (v < 0x800 || (v >= 0xD800 && v <= 0xDFFF))) || (k == 3 && (v < 0x10000 || v > 0x10FFFF)))
        return fail("invalid UTF-8 in a string");
      for (int i = 0; i <= k; i++) arena.push_back(src[p + i]);
      p += (size_t) k + 1;
    }
    const size_t l = arena.size() - start;
    if (l > 0xFFFF) return fail("a string longer than 65535 bytes");
    out = (uint32_t) start; len = (uint16_t) l;
    arena.push_back(0);  // each string NUL-terminated in the arena (never part of len)
    return true;
  }
  bool num() {  // RFC 8259 number grammar; the value is read again from the source when used
    const size_t a = p;
    if (p < n && src[p] == '-') p++;
    if (p >= n) return fail("a number needs digits");
    if (src[p] == '0') p++;
    else if (src[p] >= '1' && src[p] <= '9') while (p < n && src[p] >= '0' && src[p] <= '9') p++;
    else return fail("unexpected character");
    if (p < n && src[p] == '.') { p++; if (p >= n || src[p] < '0' || src[p] > '9') return fail("a number needs digits after '.'"); while (p < n && src[p] >= '0' && src[p] <= '9') p++; }
    if (p < n && (src[p] == 'e' || src[p] == 'E')) {
      p++;
      if (p < n && (src[p] == '+' || src[p] == '-')) p++;
      if (p >= n || src[p] < '0' || src[p] > '9') return fail("a number needs digits in its exponent");
      while (p < n && src[p] >= '0' && src[p] <= '9') p++;
    }
    if (p - a > 24) { p = a; return fail("a number longer than 24 characters"); }
    return true;
  }
  bool lit(const char *w, uint8_t t, uint8_t b, int &id) {
    const size_t l = strlen(w);
    if (n - p < l || memcmp(src + p, w, l)) return fail("unexpected character");
    if ((id = add(t)) < 0) return fail(TOO_MANY);
    nodes[id].b = b;
    p += l;
    return true;
  }
  bool val(int depth, int &id) {
    ws();
    id = -1;
    if (p >= n) return fail("unexpected end of the file");
    const char c = src[p];
    if (c == '{' || c == '[') {
      if (depth >= G::MAX_DEPTH) return fail("nested deeper than 8 levels");
      const bool obj = c == '{';
      if ((id = add(obj ? J_OBJ : J_ARR)) < 0) return fail(TOO_MANY);
      p++;
      ws();
      if (p < n && src[p] == (obj ? '}' : ']')) { p++; return true; }
      int last = -1;
      for (;;) {
        ws();
        uint32_t k = 0; uint16_t kl = 0;
        size_t koff = p;
        if (obj) {
          if (p >= n || src[p] != '"') return fail(p >= n ? "unexpected end of the file" : "expected a key (a string)");
          if (!str(k, kl)) return false;
          for (int q = nodes[id].first; q >= 0; q = nodes[q].next)  // duplicate keys: refused (JSON leaves them undefined)
            if (nodes[q].kl == kl && !memcmp(arena.data() + nodes[q].key, arena.data() + k, kl)) { p = koff; return fail("a duplicate key"); }
          ws();
          if (p >= n || src[p] != ':') return fail("expected ':'");
          p++;
        }
        int ch;
        if (!val(depth + 1, ch)) return false;
        nodes[ch].key = k; nodes[ch].kl = kl;
        if (obj) nodes[ch].off = (uint32_t) koff;  // an object member's errors point at its key
        if (last < 0) nodes[id].first = ch; else nodes[last].next = ch;
        last = ch;
        ws();
        if (p < n && src[p] == ',') { p++; ws(); if (p < n && src[p] == (obj ? '}' : ']')) return fail("a trailing comma"); continue; }
        if (p < n && src[p] == (obj ? '}' : ']')) { p++; return true; }
        return fail(p >= n ? "unexpected end of the file" : obj ? "expected ',' or '}'" : "expected ',' or ']'");
      }
    }
    if (c == '"') {
      if ((id = add(J_STR)) < 0) return fail(TOO_MANY);
      uint32_t s; uint16_t l;
      if (!str(s, l)) return false;
      nodes[id].s = s; nodes[id].sl = l;
      return true;
    }
    if (c == 't') return lit("true", J_BOOL, 1, id);
    if (c == 'f') return lit("false", J_BOOL, 0, id);
    if (c == 'n') return lit("null", J_NULL, 0, id);
    if (c == '-' || (c >= '0' && c <= '9')) {
      if ((id = add(J_NUM)) < 0) return fail(TOO_MANY);
      nodes[id].s = (uint32_t) p;  // a number: its source offset (off becomes the key's in an object)
      return num();
    }
    return fail("unexpected character");
  }
  // value accessors
  const Node &at(int i) const { return nodes[(size_t) i]; }
  const char *s_of(int i) const { return arena.data() + at(i).s; }
  bool key_is(int i, const char *k) const { return at(i).kl == strlen(k) && !memcmp(arena.data() + at(i).key, k, at(i).kl); }
  std::string key(int i) const { return std::string(arena.data() + at(i).key, at(i).kl); }
  double number(int i) const {
    char b[32];
    size_t l = 0;
    for (size_t q = at(i).s; q < n && l < 30 && (strchr("+-.eE", src[q]) || (src[q] >= '0' && src[q] <= '9')); q++) b[l++] = src[q];
    b[l] = 0;
    return strtod(b, nullptr);
  }
  int line(uint32_t off) const {
    int l = 1;
    for (size_t q = 0; q < off && q < n; q++) if (src[q] == '\n') l++;
    return l;
  }
};

// ---------------------------------------------------------------- errors
struct Errs {
  std::vector<ThemeError> &E;
  const Doc *d;
  bool full() const { return (int) E.size() >= G::MAX_ERRORS; }
  void add(const std::string &path, const std::string &msg, int node = -1) {
    if ((int) E.size() >= G::MAX_ERRORS) return;
    if ((int) E.size() == G::MAX_ERRORS - 1) { E.push_back({"", "more errors not listed (the first " + std::to_string(G::MAX_ERRORS - 1) + " are above)", 0}); return; }
    E.push_back({path, msg, node >= 0 && d ? d->line(d->at(node).off) : 0});
  }
};
std::string list(const char *const *v, int n) {
  std::string o;
  for (int i = 0; i < n; i++) o += std::string(i ? ", " : "") + v[i];
  return o;
}
int find(const char *const *v, int n, const char *s) {
  for (int i = 0; i < n; i++) if (!strcmp(v[i], s)) return i;
  return -1;
}
const char *tname(uint8_t t) {
  static const char *const N[] = {"null", "true or false", "a number", "a string", "a list", "an object"};
  return N[t];
}
std::string shown(const Doc &d, int i) {  // a value as the error message quotes it
  const Node &x = d.at(i);
  if (x.t == J_STR) { std::string s(d.s_of(i), x.sl > 40 ? 40 : x.sl); return "\"" + s + (x.sl > 40 ? "...\"" : "\""); }
  if (x.t == J_NUM) { char b[32]; snprintf(b, sizeof b, "%g", d.number(i)); return b; }
  return tname(x.t);
}
int size_index(const char *name) {
  int lo = 0, hi = G::N_SIZES - 1;  // SIZES is sorted by name
  while (lo <= hi) {
    const int m = (lo + hi) / 2, c = strcmp(G::SIZES[m].name, name);
    if (!c) return m;
    if (c < 0) lo = m + 1; else hi = m - 1;
  }
  return -1;
}
const char *const DEN[] = {"1.5x", "1x"};

// ---------------------------------------------------------------- the scratch model phase 1 fills
struct Authored {  // a part's blocks as written (presets, then the file merged over): [0] base, [1 + i] state i
  Block b[1 + N_STATES];
  int line[1 + N_STATES];
};
struct Model {
  bool mode[2] = {true, true};
  bool col_set[N_COLOURS][2] = {};
  uint32_t col[N_COLOURS][2] = {};
  int n_sets = 0;  // 0 = Facet's sets
  uint32_t role[MAX_SETS][2][N_ROLES] = {};  // the file's accent sets: [set][mode][role]
  bool size_set[N_SIZES] = {};
  int16_t size_v[N_SIZES][2] = {};
  bool type_set[N_TYPES] = {};
  int16_t type_v[N_TYPES][2] = {};
  int font[N_FONT_ROLES];
  int font_src[N_FONT_ROLES] = {}, font_fb[N_FONT_ROLES] = {};
  std::string font_url[N_FONT_ROLES], font_fam[N_FONT_ROLES];
  int shape_v[8];  // corner, cap, knob, ticks, card, indicator, icons: -1 = not set; [7] snap: -1 / 0 / 1
  Authored part[N_PARTS];
  Edge pack[MAX_EDGES];  // compile: the edge pool in canonical order (part, then state)
  Model() {
    for (int &f : font) f = -1;
    for (int &s : shape_v) s = -1;
    for (auto &p : part) for (int &l : p.line) l = 0;
  }
};
// the presets and the authored blocks keep Len in authored form until compile resolves px: px fields hold the
// given px ([1.5x, 1x]) or the size token index in px[0] with set = 2, or cells with set = 3
enum LenForm : uint8_t { L_NONE = 0, L_PX = 1, L_TOKEN = 2, L_CELLS = 3 };

struct Reader {
  const Doc &d;
  Errs &E;
  Model &M;
  Skin &S;  // ids, labels, buttons, scrollbar, edges pool land here directly
  bool need(int i, uint8_t t, const std::string &path) {
    if (d.at(i).t == t) return true;
    E.add(path, std::string("expected ") + tname(t) + ", got " + shown(d, i), i);
    return false;
  }
  bool whole(int i, const std::string &path, int lo, int hi, int &out) {
    if (!need(i, J_NUM, path)) return false;
    const double v = d.number(i);
    if (v != floor(v) || v < lo || v > hi) {
      char b[96];
      snprintf(b, sizeof b, "%s is not a whole number from %d to %d", shown(d, i).c_str(), lo, hi);
      E.add(path, b, i);
      return false;
    }
    out = (int) v;
    return true;
  }
  bool hex(int i, const std::string &path, uint32_t &out) {
    if (d.at(i).t == J_STR) {
      const char *s = d.s_of(i);
      bool ok = d.at(i).sl == 7 && s[0] == '#';
      for (int k = 1; ok && k < 7; k++) ok = isxdigit((unsigned char) s[k]);
      if (ok) { out = (uint32_t) strtoul(s + 1, nullptr, 16); return true; }
    }
    E.add(path, shown(d, i) + " is not a colour (\"#RRGGBB\")", i);
    return false;
  }
  bool ink(int i, const std::string &path, Ink &k) {
    k = Ink();
    if (d.at(i).t == J_STR) {
      const char *s = d.s_of(i);
      if (s[0] == '#') { uint32_t h; if (!hex(i, path, h)) return false; k.kind = INK_HEX; k.value = h; return true; }
      const int r = find(G::ROLES, G::N_ROLES, s);
      if (r >= 0) { k.kind = INK_ROLE; k.value = (uint8_t) r; return true; }
      const int c = find(G::COLOURS, G::N_COLOURS, s);
      if (c >= 0) { k.kind = INK_TOKEN; k.value = (uint8_t) c; return true; }
    }
    E.add(path, shown(d, i) + " is not an ink (a colour token, an accent role (" + list(G::ROLES, G::N_ROLES) + ") or \"#RRGGBB\")", i);
    return false;
  }
  // a length: cells (a whole number), a size token name, or [1.5x, 1x] px; signed = shadow offsets
  bool len(int i, const std::string &path, Len &l, bool signed_ = false) {
    l = Len();
    const Node &x = d.at(i);
    const int lo = signed_ ? -64 : 0;
    if (x.t == J_NUM) { int v; if (!whole(i, path, lo, 64, v)) return false; l.cells = (int8_t) v; l.set = L_CELLS; return true; }
    if (x.t == J_STR) {
      const int s = size_index(d.s_of(i));
      if (s < 0) { E.add(path, shown(d, i) + " is not a size token", i); return false; }
      l.px[0] = (int16_t) s; l.set = L_TOKEN;
      return true;
    }
    if (x.t == J_ARR) {
      int a = x.first, b = a >= 0 ? d.at(a).next : -1;
      if (a >= 0 && b >= 0 && d.at(b).next < 0) {
        int va, vb;
        if (!whole(a, path + "[0]", signed_ ? -256 : 0, 1024, va) || !whole(b, path + "[1]", signed_ ? -256 : 0, 1024, vb)) return false;
        l.px[0] = (int16_t) va; l.px[1] = (int16_t) vb; l.set = L_PX;
        return true;
      }
    }
    E.add(path, shown(d, i) + " is not a length (cells as a whole number, a size token, or [1.5x, 1x] px)", i);
    return false;
  }
  // a face: an ink, {"from", "to", "dir"}, {"checker": [ink, ink]} or {"blocks": [ink, ink], "run", "gap"}
  bool face(int i, const std::string &path, Face &f, bool solid_only) {
    f = Face();
    const char *help = "is not a face (an ink, {\"from\", \"to\", \"dir\"}, {\"checker\": [ink, ink]} or {\"blocks\": [ink, ink], \"run\", \"gap\"})";
    if (d.at(i).t == J_STR) return ink(i, path, f.a);
    if (d.at(i).t != J_OBJ || solid_only) { E.add(path, shown(d, i) + (solid_only ? " is not an ink (this face is one ink)" : std::string(" ") + help), i); return false; }
    bool ok = true, grad = false, pat = false, from = false, to = false, dir_set = false;
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next) {
      const std::string k = d.key(c), at = path + "." + k;
      if (k == "from" || k == "to") { grad = true; (k == "from" ? from : to) = true; ok &= ink(c, at, k == "from" ? f.a : f.b); f.fill = FILL_GRADIENT; }
      else if (k == "dir") {
        dir_set = true;
        int v = d.at(c).t == J_STR ? find(G::DIRS, G::N_DIRS, d.s_of(c)) : -1;
        if (v < 0) { E.add(at, shown(d, c) + " is not a direction (" + list(G::DIRS, G::N_DIRS) + ")", c); ok = false; }
        else f.down = (uint8_t) v;
      } else if (k == "checker" || k == "blocks") {
        if (pat) { E.add(at, "a face is one of a gradient, a checker or blocks", c); ok = false; continue; }
        pat = true;
        f.fill = k == "checker" ? FILL_CHECKER : FILL_BLOCKS;
        const int a = d.at(c).t == J_ARR ? d.at(c).first : -1, b = a >= 0 ? d.at(a).next : -1;
        if (a < 0 || b < 0 || d.at(b).next >= 0) { E.add(at, "expected [ink, ink]", c); ok = false; continue; }
        ok &= ink(a, at + "[0]", f.a);
        ok &= ink(b, at + "[1]", f.b);
      } else if (k == "run" || k == "gap") { ok &= len(c, at, k == "run" ? f.run : f.gap); }
      else { E.add(at, "unknown key (from, to, dir, checker, blocks, run, gap)", c); ok = false; }
    }
    if (grad && pat) { E.add(path, "a face is one of a gradient, a checker or blocks", i); return false; }
    if (grad && !(from && to)) { E.add(path, "a gradient needs \"from\" and \"to\"", i); return false; }
    if (!grad && !pat && ok) { E.add(path, help, i); return false; }
    if ((f.run.set || f.gap.set) && f.fill != FILL_BLOCKS) { E.add(path, "\"run\" and \"gap\" belong to a blocks face", i); return false; }
    if (dir_set && f.fill != FILL_GRADIENT) { E.add(path + ".dir", "a direction belongs to a gradient", i); return false; }
    if (f.fill == FILL_BLOCKS && !f.run.set) { f.run.cells = 1; f.run.set = L_CELLS; }
    if (f.fill == FILL_BLOCKS && !f.gap.set) { f.gap.cells = 1; f.gap.set = L_CELLS; }
    return ok;
  }
  bool edge_list(int i, const std::string &path, Block &b) {
    if (!need(i, J_ARR, path)) return false;
    int n = 0;
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next) n++;
    if (n > G::MAX_EDGES) { E.add(path, std::to_string(n) + " layers: at most " + std::to_string(G::MAX_EDGES) + " (each is drawn on every redraw, C48)", i); return false; }
    if (S.n_edges + n > G::MAX_EDGES_TOTAL) { E.add(path, "more than " + std::to_string(G::MAX_EDGES_TOTAL) + " edge layers in the theme (memory cap, C48)", i); return false; }
    b.edge0 = S.n_edges; b.n_edges = 0;
    bool ok = true;
    int k = 0;
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next, k++) {
      const std::string at = path + "[" + std::to_string(k) + "]";
      if (!need(c, J_OBJ, at)) { ok = false; continue; }
      Edge e;
      e.w.cells = 1; e.w.set = L_CELLS;
      bool has_ink = false;
      for (int q = d.at(c).first; q >= 0; q = d.at(q).next) {
        const std::string kk = d.key(q), aq = at + "." + kk;
        if (kk == "side") {
          const int v = d.at(q).t == J_STR ? find(G::SIDES, G::N_SIDES, d.s_of(q)) : -1;
          static const uint8_t SV[] = {SIDE_ALL, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM, SIDE_LEFT, SIDE_TL, SIDE_BR};
          static_assert(sizeof SV == G::N_SIDES, "sides");
          if (v < 0) { E.add(aq, shown(d, q) + " is not a side (" + list(G::SIDES, G::N_SIDES) + ")", q); ok = false; }
          else e.side = SV[v];
        } else if (kk == "w" || kk == "inset") ok &= len(q, aq, kk == "w" ? e.w : e.inset);
        else if (kk == "ink") ok &= (has_ink = ink(q, aq, e.ink));
        else if (kk == "style") {
          const int v = d.at(q).t == J_STR ? find(G::EDGE_STYLES, G::N_EDGE_STYLES, d.s_of(q)) : -1;
          if (v < 0) { E.add(aq, shown(d, q) + " is not an edge style (" + list(G::EDGE_STYLES, G::N_EDGE_STYLES) + ")", q); ok = false; }
          else e.dotted = (uint8_t) v;
        } else { E.add(aq, "unknown key (side, w, inset, ink, style)", q); ok = false; }
      }
      if (!has_ink && ok) { E.add(at + ".ink", "missing (every edge layer names its ink)", c); ok = false; }
      if (e.w.set == L_CELLS && e.w.cells < 1) { E.add(at + ".w", "an edge is at least 1 cell wide", c); ok = false; }
      if (ok) { S.edges[S.n_edges++] = e; b.n_edges++; }
    }
    return ok;
  }
  // a style block of part p: state st (0 = base); states inside the base are read into their own blocks
  void block(int i, int p, int st, const std::string &path) {
    if (!need(i, J_OBJ, path)) return;
    const G::Part &P = G::PARTS[p];
    Block &b = M.part[p].b[st];
    M.part[p].line[st] = d.line(d.at(i).off);
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next) {
      if (E.full()) return;
      const std::string k = d.key(c), at = path + "." + k;
      const int si = find(G::STATES, G::N_STATES, k.c_str());
      if (si >= 0) {
        if (st) { E.add(at, "a state inside a state (states merge over the base only)", c); continue; }
        block(c, p, 1 + si, at);
        continue;
      }
      const int ki = find(G::KEYS, G::N_KEYS, k.c_str());
      std::string keys;
      for (int q = 0; q < G::N_KEYS; q++) if (P.keys >> q & 1) keys += std::string(keys.empty() ? "" : ", ") + G::KEYS[q];
      if (ki < 0) { E.add(at, "unknown key (" + std::string(P.name) + " takes " + keys + ", or a state: " + list(G::STATES, G::N_STATES) + ")", c); continue; }
      if (!(P.keys >> ki & 1)) { E.add(at, "not a key of " + std::string(P.name) + " (it takes " + keys + ")", c); continue; }
      if (st && (G::BASE_ONLY >> ki & 1)) { E.add(at, "in the base block only", c); continue; }
      const uint16_t bit = (uint16_t) (1u << ki);
      bool ok = false;
      if (bit == K_FACE) ok = face(c, at, b.face, !strcmp(P.name, "scrollbar"));
      else if (bit == K_RADIUS) ok = len(c, at, b.radius);
      else if (bit == K_PAD) {
        if (!P.pad) E.add(at, "padding is layout (C45): a theme may pad only a part with no content (track, track-fill) or a flush bar (header, card-head)", c);
        else ok = len(c, at, b.pad);
      }
      else if (bit == K_EDGES) ok = edge_list(c, at, b);
      else if (bit == K_TEXT) ok = ink(c, at, b.text);
      else if (bit == K_INK) ok = ink(c, at, b.ink);
      else if (bit == K_WEIGHT) {
        const int v = d.at(c).t == J_STR ? find(G::WEIGHTS, G::N_WEIGHTS, d.s_of(c)) : -1;
        if (v < 0) E.add(at, shown(d, c) + " is not a weight (" + list(G::WEIGHTS, G::N_WEIGHTS) + ")", c);
        else { b.bold = (uint8_t) v; ok = true; }
      } else if (bit == K_SHAPE) {
        const bool knob = !strcmp(P.kind, "knob"), sw = !strcmp(P.kind, "switch");
        const char *const *v = knob ? G::KNOB_SHAPES : sw ? G::SWITCH_SHAPES : G::TICK_SHAPES;
        const int nv = knob ? G::N_KNOB_SHAPES : sw ? G::N_SWITCH_SHAPES : G::N_TICK_SHAPES, x = d.at(c).t == J_STR ? find(v, nv, d.s_of(c)) : -1;
        if (x < 0) E.add(at, shown(d, c) + " is not a " + (knob ? "knob" : sw ? "switch" : "tick") + " shape (" + list(v, nv) + ")", c);
        else { b.shape = (uint8_t) x; ok = true; }
      } else if (bit == K_SHADOW) {
        if (need(c, J_OBJ, at)) {
          bool has_ink = false;
          ok = true;
          for (int q = d.at(c).first; q >= 0; q = d.at(q).next) {
            const std::string kk = d.key(q), aq = at + "." + kk;
            if (kk == "dx" || kk == "dy") ok &= len(q, aq, kk == "dx" ? b.shadow.dx : b.shadow.dy, true);
            else if (kk == "ink") ok &= (has_ink = ink(q, aq, b.shadow.ink));
            else { E.add(aq, "unknown key (dx, dy, ink)", q); ok = false; }
          }
          if (ok && !has_ink) { E.add(at + ".ink", "missing (a shadow names its ink)", c); ok = false; }
        }
      } else if (bit == K_BUTTONS) {
        if (need(c, J_OBJ, at)) {
          Buttons &bt = S.buttons;
          bt = Buttons();
          bt.w.px[0] = 16; bt.w.px[1] = 12; bt.h.px[0] = 14; bt.h.px[1] = 10; bt.gap.px[0] = 2; bt.gap.px[1] = 2;  // Win98 caption buttons at 1.5x / 1x
          bt.w.set = bt.h.set = bt.gap.set = L_PX;
          ok = true;
          for (int q = d.at(c).first; q >= 0; q = d.at(q).next) {
            const std::string kk = d.key(q), aq = at + "." + kk;
            if (kk == "names") {
              if (!need(q, J_ARR, aq)) { ok = false; continue; }
              int k2 = 0;
              for (int g = d.at(q).first; g >= 0; g = d.at(g).next, k2++) {
                const int x = d.at(g).t == J_STR ? find(G::GLYPHS, G::N_GLYPHS, d.s_of(g)) : -1;
                if (x < 0) { E.add(aq + "[" + std::to_string(k2) + "]", shown(d, g) + " is not a caption button (" + list(G::GLYPHS, G::N_GLYPHS) + ")", g); ok = false; }
                else if (bt.n >= G::MAX_BUTTONS) { E.add(aq, "at most " + std::to_string(G::MAX_BUTTONS) + " caption buttons", q); ok = false; break; }
                else bt.names[bt.n++] = (uint8_t) x;
              }
            } else if (kk == "w" || kk == "h" || kk == "gap") ok &= len(q, aq, kk == "w" ? bt.w : kk == "h" ? bt.h : bt.gap);
            else { E.add(aq, "unknown key (names, w, h, gap)", q); ok = false; }
          }
        }
      } else if (bit == K_W) ok = len(c, at, S.scrollbar.w);
      else if (bit == K_LANE) { ok = face(c, at, S.scrollbar.lane, false); S.scrollbar.has_lane = ok; }
      else if (bit == K_ARROWS) { if (need(c, J_BOOL, at)) { S.scrollbar.arrows = d.at(c).b; ok = true; } }
      else if (bit == K_FLUSH) { if (need(c, J_BOOL, at)) { b.flush = d.at(c).b ? 1 : 0; ok = true; } }
      if (ok) b.has |= bit;
    }
  }
  bool id_text(int i, const std::string &path, char *out, int cap, bool slug) {
    if (!need(i, J_STR, path)) return false;
    const char *s = d.s_of(i);
    const int l = d.at(i).sl;
    if (slug) {
      bool ok = l >= 1 && l <= G::ID_CHARS;
      for (int k = 0; ok && k < l; k++) ok = (s[k] >= 'a' && s[k] <= 'z') || (s[k] >= '0' && s[k] <= '9') || s[k] == '-';
      if (!ok) { E.add(path, shown(d, i) + " is not an id (1 to " + std::to_string(G::ID_CHARS) + " of a-z, 0-9, -)", i); return false; }
    } else {
      int chars = 0;
      for (int k = 0; k < l; k++) if (((unsigned char) s[k] & 0xC0) != 0x80) chars++;
      if (chars > cap) { E.add(path, "longer than " + std::to_string(cap) + " characters", i); return false; }
    }
    if (out) { memcpy(out, s, (size_t) l); out[l] = 0; }  // the buffers hold the longest id / label (4 bytes a character)
    return true;
  }
  void accents(int i) {
    if (!need(i, J_ARR, "accents")) return;
    int n = 0;
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next) n++;
    if (n < G::ACCENTS_MIN || n > G::ACCENTS_MAX) { E.add("accents", std::to_string(n) + " sets: a theme has " + std::to_string(G::ACCENTS_MIN) + " to " + std::to_string(G::ACCENTS_MAX), i); return; }
    M.n_sets = n;
    S.n_sets = (uint8_t) n;
    int a = 0;
    for (int c = d.at(i).first; c >= 0; c = d.at(c).next, a++) {
      const std::string at = "accents[" + std::to_string(a) + "]";
      if (!need(c, J_OBJ, at)) continue;
      bool has[2] = {false, false}, has_id = false;
      for (int q = d.at(c).first; q >= 0; q = d.at(q).next) {
        const std::string k = d.key(q), aq = at + "." + k;
        if (k == "id") {
          if ((has_id = id_text(q, aq, S.set_id[a], G::ID_CHARS, true)))
            for (int b = 0; b < a; b++) if (!strcmp(S.set_id[b], S.set_id[a])) { E.add(aq, "\"" + std::string(S.set_id[a]) + "\" is already the id of accents[" + std::to_string(b) + "]", q); break; }
        } else if (k == "label") id_text(q, aq, S.set_label[a], G::LABEL_CHARS, false);
        else if (k == "dark" || k == "light") {
          const int m = k == "light";
          if (!M.mode[m]) { E.add(aq, "this theme has no " + k + " mode (modes)", q); continue; }
          has[m] = true;
          if (!need(q, J_OBJ, aq)) continue;
          bool got[N_ROLES] = {};
          for (int r = d.at(q).first; r >= 0; r = d.at(r).next) {
            const std::string rk = d.key(r);
            const int ri = find(G::ROLES, G::N_ROLES, rk.c_str());
            if (ri < 0) { E.add(aq + "." + rk, "not an accent role (" + list(G::ROLES, G::N_ROLES) + ")", r); continue; }
            uint32_t h;
            if (hex(r, aq + "." + rk, h)) { got[ri] = true; M.role[a][m][ri] = h; }
          }
          for (int ri = 0; ri < N_ROLES; ri++)
            if (!got[ri]) E.add(aq + "." + G::ROLES[ri], "missing (every accent set names all " + std::to_string(N_ROLES) + " roles: " + list(G::ROLES, G::N_ROLES) + ")", q);
        } else E.add(aq, "unknown key (id, label, dark, light)", q);
      }
      if (!has_id) E.add(at + ".id", "missing (every accent set has an id)", c);
      for (int m = 0; m < 2; m++) if (M.mode[m] && !has[m]) E.add(at + "." + G::MODES[m], "missing (this theme has the " + std::string(G::MODES[m]) + " mode)", c);
      if (!S.set_label[a][0]) memcpy(S.set_label[a], S.set_id[a], sizeof S.set_id[a]);
    }
  }
};

// ---------------------------------------------------------------- phase 1: the file against the schema
bool phase1(const Doc &d, Errs &E, Model &M, Reader &R) {
  Skin &S = R.S;
  const int root = 0;
  if (d.at(root).t != J_OBJ) { E.add("", "a theme is a JSON object", root); return false; }
  // schema first: a newer file is refused on that alone (its other keys may mean things this build does not know)
  int schema = -1, modes = -1;
  for (int c = d.at(root).first; c >= 0; c = d.at(c).next) {
    if (d.key_is(c, "schema")) schema = c;
    if (d.key_is(c, "modes")) modes = c;
  }
  if (schema < 0) { E.add("schema", "missing (a theme names its schema: " + std::to_string(G::SCHEMA) + ")", root); return false; }
  if (d.at(schema).t != J_NUM || d.number(schema) != floor(d.number(schema))) { E.add("schema", shown(d, schema) + " is not a schema number", schema); return false; }
  const double v = d.number(schema);  // whole but maybe huge: compared as a double, shown as the file wrote it
  if (v > G::SCHEMA) { E.add("schema", shown(d, schema) + " is newer than this panel (" + std::to_string(G::SCHEMA) + ")", schema); return false; }
  if (v < 1) { E.add("schema", shown(d, schema) + " is not a theme schema (1 .. " + std::to_string(G::SCHEMA) + ")", schema); return false; }
  // (schema 1 is the first: no upgrade functions yet. An older schema is upgraded here, one pure function per step,
  // upgrade_v<N>_to_v<N+1>(Doc&), each with its own test, before anything below reads the file.)
  if (modes >= 0 && R.need(modes, J_ARR, "modes")) {
    bool has[2] = {false, false};
    int k = 0;
    for (int c = d.at(modes).first; c >= 0; c = d.at(c).next, k++) {
      const int m = d.at(c).t == J_STR ? find(G::MODES, 2, d.s_of(c)) : -1;
      if (m < 0) E.add("modes[" + std::to_string(k) + "]", shown(d, c) + " is not a mode (dark, light)", c);
      else if (has[m]) E.add("modes[" + std::to_string(k) + "]", "\"" + std::string(G::MODES[m]) + "\" twice", c);
      else has[m] = true;
    }
    if (!k) E.add("modes", "empty (a theme has dark, light or both)", modes);
    else if (has[0] || has[1]) { M.mode[0] = has[0]; M.mode[1] = has[1]; }
  }
  S.modes = (uint8_t) ((M.mode[0] ? 1 : 0) | (M.mode[1] ? 2 : 0));
  bool has_id = false, id_seen = false;
  for (int c = d.at(root).first; c >= 0; c = d.at(c).next) {
    if (E.full()) break;
    const std::string k = d.key(c);
    if (k == "schema" || k == "modes") continue;
    if (k == "id") { id_seen = true; has_id = R.id_text(c, "id", S.id, G::ID_CHARS, true); }
    else if (k == "label") R.id_text(c, "label", S.label, G::LABEL_CHARS, false);
    else if (k == "about") R.id_text(c, "about", nullptr, G::ABOUT_CHARS, false);
    else if (k == "colour") {
      if (!R.need(c, J_OBJ, "colour")) continue;
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t), at = "colour." + tk;
        const int ci = find(G::COLOURS, G::N_COLOURS, tk.c_str());
        if (find(G::ROLES, G::N_ROLES, tk.c_str()) >= 0) { E.add(at, "an accent role: set it in accents (each set names all " + std::to_string(N_ROLES) + " roles)", t); continue; }
        if (ci < 0) { E.add(at, "not a colour token", t); continue; }
        if (!R.need(t, J_OBJ, at)) continue;
        for (int m = d.at(t).first; m >= 0; m = d.at(m).next) {
          const std::string mk = d.key(m);
          const int mi = find(G::MODES, 2, mk.c_str());
          if (mi < 0) { E.add(at + "." + mk, "not a mode (dark, light)", m); continue; }
          if (!M.mode[mi]) { E.add(at + "." + mk, "this theme has no " + mk + " mode (modes)", m); continue; }
          uint32_t h;
          if (R.hex(m, at + "." + mk, h)) { M.col_set[ci][mi] = true; M.col[ci][mi] = h; }
        }
      }
    } else if (k == "accents") R.accents(c);
    else if (k == "size") {
      if (!R.need(c, J_OBJ, "size")) continue;
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t), at = "size." + tk;
        const int si = size_index(tk.c_str());
        if (si < 0) { E.add(at, "not a size token", t); continue; }
        const G::Size &z = G::SIZES[si];
        if (!z.look) { E.add(at, tk == "radius:round" ? "radius:round is fixed (a pill, not a length) (C18)" : "layout sizes are fixed by Facet (C18)", t); continue; }
        const int a = d.at(t).t == J_ARR ? d.at(t).first : -1, b = a >= 0 ? d.at(a).next : -1;
        if (a < 0 || b < 0 || d.at(b).next >= 0) { E.add(at, shown(d, t) + " is not [1.5x, 1x] px", t); continue; }
        int px[2];
        if (!R.whole(a, at + "[0]", -32768, 32767, px[0]) || !R.whole(b, at + "[1]", -32768, 32767, px[1])) continue;
        bool ok = true;
        for (int q = 0; q < 2; q++)
          if (px[q] < z.lo[q] || px[q] > z.hi[q]) {
            E.add(at, std::to_string(px[q]) + " at " + DEN[q] + " is outside " + std::to_string(z.lo[q]) + " .. " + std::to_string(z.hi[q]) + " (C19: a theme value outside the range is refused)", t);
            ok = false;
          }
        if (ok) { M.size_set[si] = true; M.size_v[si][0] = (int16_t) px[0]; M.size_v[si][1] = (int16_t) px[1]; }
      }
    } else if (k == "type") {
      if (!R.need(c, J_OBJ, "type")) continue;
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t), at = "type." + tk;
        int ti = -1;
        for (int q = 0; q < G::N_TYPES; q++) if (tk == G::TYPES[q].name) ti = q;
        if (ti < 0) {
          std::string o;
          for (int q = 0; q < G::N_TYPES; q++) o += std::string(q ? ", " : "") + G::TYPES[q].name;
          E.add(at, "not a type style a theme may size (" + o + ")", t);
          continue;
        }
        const G::Type &y = G::TYPES[ti];
        const int a = d.at(t).t == J_ARR ? d.at(t).first : -1, b = a >= 0 ? d.at(a).next : -1;
        if (a < 0 || b < 0 || d.at(b).next >= 0) { E.add(at, shown(d, t) + " is not [1.5x, 1x] px", t); continue; }
        int px[2];
        if (!R.whole(a, at + "[0]", 0, 1024, px[0]) || !R.whole(b, at + "[1]", 0, 1024, px[1])) continue;
        bool ok = true;
        for (int q = 0; q < 2; q++) {
          if (px[q] < y.floor[q]) { E.add(at, std::to_string(px[q]) + " px at " + DEN[q] + " is below its floor " + std::to_string(y.floor[q]), t); ok = false; }
          else if (px[q] > y.box[q]) { E.add(at, std::to_string(px[q]) + " px at " + DEN[q] + " is above its line box " + std::to_string(y.box[q]) + " (the box is fixed: no line moves, C45)", t); ok = false; }
        }
        if (ok) { M.type_set[ti] = true; M.type_v[ti][0] = (int16_t) px[0]; M.type_v[ti][1] = (int16_t) px[1]; }
      }
    } else if (k == "font") {
      if (!R.need(c, J_OBJ, "font")) continue;
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t), at = "font." + tk;
        const int ri = find(G::FONT_ROLES, G::N_FONT_ROLES, tk.c_str());
        if (ri < 0) { E.add(at, "not a font role (" + list(G::FONT_ROLES, G::N_FONT_ROLES) + ")", t); continue; }
        // a family id (built in), or {family, source: builtin | download, url, fallback}
        int fv = t;
        bool dl = false;
        if (d.at(t).t == J_OBJ) {
          int src = -1, url = -1, fb = -1;
          fv = -1;
          for (int q = d.at(t).first; q >= 0; q = d.at(q).next) {
            const std::string qk = d.key(q);
            if (qk == "family") fv = q;
            else if (qk == "source") src = q;
            else if (qk == "url") url = q;
            else if (qk == "fallback") fb = q;
            else E.add(at + "." + qk, "unknown key (family, source, url, fallback)", q);
          }
          if (fv < 0) { E.add(at, "needs a family", t); continue; }
          if (src >= 0 && R.need(src, J_STR, at + ".source")) {
            const std::string sv = d.s_of(src);
            if (sv == "download") dl = true;
            else if (sv != "builtin") { E.add(at + ".source", shown(d, src) + " is not a source (builtin, download)", src); continue; }
          }
          if (dl) {
            if (url < 0) { E.add(at, "a downloaded font needs its url", t); continue; }
            if (!R.need(url, J_STR, at + ".url")) continue;
            const std::string u = d.s_of(url);
            bool ok = u.rfind("https://", 0) == 0 && u.size() > 8 && u.size() <= 160;
            for (char ch : u) if ((unsigned char) ch <= ' ' || ch == '"' || ch == '\\' || (unsigned char) ch >= 127) ok = false;
            if (!ok) { E.add(at + ".url", shown(d, url) + " is not an https URL of at most 160 characters", url); continue; }
            M.font_url[ri] = u;
            if (fb >= 0 && R.need(fb, J_BOOL, at + ".fallback")) M.font_fb[ri] = d.at(fb).b ? 1 : 0;
          } else if (fb >= 0) { E.add(at + ".fallback", "only a downloaded font has a fallback setting (a built-in one's is in fonts/fonts.json)", fb); continue; }
          else if (url >= 0) { E.add(at + ".url", "only a downloaded font has a url", url); continue; }
        }
        if (!R.need(fv, J_STR, at + (fv == t ? "" : ".family"))) continue;
        if (dl) {  // any family id: the panel fetches the file; the built-in fallback draws until then
          const std::string fam = d.s_of(fv);
          bool ok = !fam.empty() && fam.size() <= 32;
          for (char ch : fam) if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')) ok = false;
          if (!ok) { E.add(at + ".family", shown(d, fv) + " is not a family id (1 to 32 of a-z 0-9 -)", fv); continue; }
          M.font_src[ri] = 1; M.font_fam[ri] = fam;
          continue;
        }
        const int fi = find(G::FAMILIES, G::N_FAMILIES, d.s_of(fv));
        if (fi < 0) { E.add(at + (fv == t ? "" : ".family"), shown(d, fv) + " is not in this panel's build (" + list(G::FAMILIES, G::N_FAMILIES) + "; or source: download)", fv); continue; }
        M.font[ri] = fi;
      }
    } else if (k == "shape") {
      if (!R.need(c, J_OBJ, "shape")) continue;
      static const char *const KEYS[] = {"corner", "cap", "knob", "ticks", "card", "indicator", "icons", "snap"};
      static const char *const *const VALS[] = {G::SHAPE_CORNER, G::SHAPE_CAP, G::SHAPE_KNOB, G::SHAPE_TICKS, G::SHAPE_CARD, G::SHAPE_INDICATOR, G::SHAPE_ICONS};
      static const int NV[] = {G::N_SHAPE_CORNER, G::N_SHAPE_CAP, G::N_SHAPE_KNOB, G::N_SHAPE_TICKS, G::N_SHAPE_CARD, G::N_SHAPE_INDICATOR, G::N_SHAPE_ICONS};
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t), at = "shape." + tk;
        const int si = find(KEYS, 8, tk.c_str());
        if (si < 0) { E.add(at, "unknown key (" + list(KEYS, 8) + ")", t); continue; }
        if (si == 7) { if (R.need(t, J_BOOL, at)) M.shape_v[7] = d.at(t).b; continue; }
        const int vi = d.at(t).t == J_STR ? find(VALS[si], NV[si], d.s_of(t)) : -1;
        if (vi < 0) { E.add(at, shown(d, t) + " is not a " + tk + " (" + list(VALS[si], NV[si]) + ")", t); continue; }
        M.shape_v[si] = vi;
      }
    } else if (k == "parts") {
      if (!R.need(c, J_OBJ, "parts")) continue;
      for (int t = d.at(c).first; t >= 0; t = d.at(t).next) {
        const std::string tk = d.key(t);
        int pi = -1;
        for (int q = 0; q < G::N_PARTS; q++) if (tk == G::PARTS[q].name) pi = q;
        if (pi < 0) {
          std::string o;
          for (int q = 0; q < G::N_PARTS; q++) o += std::string(q ? ", " : "") + G::PARTS[q].name;
          E.add("parts." + tk, "not a part (" + o + ")", t);
          continue;
        }
        R.block(t, pi, 0, "parts." + tk);
      }
    } else E.add(k, "unknown key (schema, id, label, about, modes, colour, accents, size, type, font, shape, parts)", c);
  }
  if (!id_seen) E.add("id", "missing (a theme has an id)", root);
  (void) has_id;
  if (!S.label[0]) memcpy(S.label, S.id, sizeof S.id);
  return E.E.empty();
}

// ---------------------------------------------------------------- phase 2: compile
void resolve(Len &l, const Skin &S) {
  if (l.set == L_TOKEN) { const int i = l.px[0]; l.px[0] = S.size[0][i]; l.px[1] = S.size[1][i]; return; }
  if (l.set == L_CELLS) for (int d = 0; d < 2; d++) l.px[d] = (int16_t) (l.cells * S.size[d][G::SZ_PIXEL]);
}
void resolve(Face &f, const Skin &S) { resolve(f.run, S); resolve(f.gap, S); }
void resolve(Block &b, const Skin &S) {
  resolve(b.face, S); resolve(b.radius, S); resolve(b.pad, S); resolve(b.shadow.dx, S); resolve(b.shadow.dy, S);
}
void overlay(Block &b, const Block &o) {  // a state block over a merged block: key by key; a list (edges) replaces whole
  if (o.has & K_FACE) b.face = o.face;
  if (o.has & K_RADIUS) b.radius = o.radius;
  if (o.has & K_PAD) b.pad = o.pad;
  if (o.has & K_EDGES) { b.edge0 = o.edge0; b.n_edges = o.n_edges; }
  if (o.has & K_SHADOW) b.shadow = o.shadow;
  if (o.has & K_TEXT) b.text = o.text;
  if (o.has & K_INK) b.ink = o.ink;
  if (o.has & K_WEIGHT) b.bold = o.bold;
  if (o.has & K_SHAPE) b.shape = o.shape;
  b.has |= o.has;
}
// "parts.card", "parts.card.pressed" (an authored state), "parts.card.on+pressed" (a combination)
std::string state_path(int p, int st) { return std::string("parts.") + G::PARTS[p].name + (st ? std::string(".") + G::STATES[st - 1] : ""); }
std::string combo_name(int mask) {
  std::string o;
  for (int i = 0; i < N_STATES; i++) if (mask >> i & 1) o += std::string(o.empty() ? "" : "+") + G::STATES[i];
  return o;
}
std::string combo_path(int p, int mask) { return std::string("parts.") + G::PARTS[p].name + (mask ? "." + combo_name(mask) : ""); }

// the presets (shape.*): a value other than Facet's (index 0) fills the part keys the file left unset
bool preset_edges(Skin &S, Block &b, std::initializer_list<Edge> es) {
  if (b.has & K_EDGES) return true;
  if (S.n_edges + (int) es.size() > G::MAX_EDGES_TOTAL) return false;
  b.edge0 = S.n_edges; b.n_edges = (uint8_t) es.size(); b.has |= K_EDGES;
  for (const Edge &e : es) S.edges[S.n_edges++] = e;
  return true;
}
Edge mk_edge(uint8_t side, int w_size, Ink k, int inset_w = 0) {  // w = a size token; inset = inset_w x that width
  Edge e;
  e.side = side;
  e.w.set = L_TOKEN; e.w.px[0] = (int16_t) w_size;
  if (inset_w) { e.inset.set = L_TOKEN; e.inset.px[0] = (int16_t) w_size; e.inset.cells = (int8_t) inset_w; }  // x inset_w in compile
  e.ink = k;
  return e;
}
Ink tok_ink(int c) { Ink k; k.kind = INK_TOKEN; k.value = (uint32_t) c; return k; }
Ink role_ink(int r) { Ink k; k.kind = INK_ROLE; k.value = (uint32_t) r; return k; }

bool presets(Model &M, Skin &S, Errs &E) {
  Block &card = M.part[G::P_CARD].b[0], &ctl = M.part[G::P_CONTROL].b[0], &on = M.part[G::P_CARD].b[1 + G::ST_ON];
  if (S.corner == 1) for (Block *b : {&card, &ctl}) if (!(b->has & K_RADIUS)) { b->radius = Len(); b->radius.set = L_PX; b->has |= K_RADIUS; }
  bool ok = true;
  const Ink outline = tok_ink(G::C_TEXT_DIM), acc = role_ink(G::R_ACCENT);  // an outline is a graphic (3.0 on bg, C47): not the decor line-hi
  if (S.card >= 1) {
    if (S.card == 1) ok &= preset_edges(S, card, {mk_edge(SIDE_ALL, G::SZ_EDGE, outline)});
    else ok &= preset_edges(S, card, {mk_edge(SIDE_ALL, G::SZ_EDGE, outline), mk_edge(SIDE_ALL, G::SZ_EDGE, outline, 2)});
    ok &= preset_edges(S, ctl, {mk_edge(SIDE_ALL, G::SZ_EDGE, outline)});
  }
  if (S.indicator == 1) {
    if (S.card == 2) ok &= preset_edges(S, on, {mk_edge(SIDE_ALL, G::SZ_EDGE, acc), mk_edge(SIDE_ALL, G::SZ_EDGE, acc, 2)});
    else ok &= preset_edges(S, on, {mk_edge(SIDE_ALL, G::SZ_EDGE, acc)});
  } else if (S.indicator == 2) ok &= preset_edges(S, on, {mk_edge(SIDE_BOTTOM, G::SZ_INDICATOR, acc)});
  if (S.knob) { Block &k = M.part[G::P_KNOB].b[0]; if (!(k.has & K_SHAPE)) { k.shape = S.knob; k.has |= K_SHAPE; } }
  if (S.ticks) for (int p : {G::P_TICK, G::P_TICK_MAJOR}) { Block &k = M.part[p].b[0]; if (!(k.has & K_SHAPE)) { k.shape = S.ticks; k.has |= K_SHAPE; } }
  if (!ok) E.add("shape", "the presets need more edge layers than the theme has left (memory cap, C48)");
  return ok;
}

// the defaults + the file's values into the Skin; presets; lengths; the authored checks (ranges, caps, the grid)
void compile(Model &M, Skin &S, Errs &E) {
  S.modes = (uint8_t) ((M.mode[0] ? 1 : 0) | (M.mode[1] ? 2 : 0));
  if (!M.n_sets) {
    S.n_sets = (uint8_t) G::N_SETS;
    for (int s = 0; s < G::N_SETS; s++) { snprintf(S.set_id[s], sizeof S.set_id[s], "%s", G::SETS[s].id); snprintf(S.set_label[s], sizeof S.set_label[s], "%s", G::SETS[s].label); }
  }
  for (int m = 0; m < 2; m++) {
    if (!M.mode[m]) continue;
    for (int s = 0; s < S.n_sets; s++) {
      for (int c = 0; c < N_COLOURS; c++) S.palette[m][s][c] = M.col_set[c][m] ? M.col[c][m] : G::COLOUR_V[c][m];
      for (int r = 0; r < N_ROLES; r++) if (G::ROLE_COLOUR[r] >= 0) S.palette[m][s][G::ROLE_COLOUR[r]] = M.n_sets ? M.role[s][m][r] : G::SETS[s].v[m][r];
    }
  }
  for (int i = 0; i < N_SIZES; i++) for (int d = 0; d < 2; d++) S.size[d][i] = M.size_set[i] ? M.size_v[i][d] : G::SIZES[i].v[d];
  for (int i = 0; i < N_TYPES; i++) for (int d = 0; d < 2; d++) S.type[d][i] = M.type_set[i] ? M.type_v[i][d] : G::TYPES[i].size[d];
  for (int r = 0; r < N_FONT_ROLES; r++) {
    S.font[r] = (uint8_t) (M.font[r] >= 0 ? M.font[r] : G::FONT_DEFAULT[r]);
    S.font_src[r] = (uint8_t) M.font_src[r]; S.font_fallback[r] = (uint8_t) M.font_fb[r];
    snprintf(S.font_url[r], sizeof S.font_url[r], "%s", M.font_url[r].c_str());
    snprintf(S.font_family[r], sizeof S.font_family[r], "%s", M.font_fam[r].c_str());
  }
  uint8_t *sh[] = {&S.corner, &S.cap, &S.knob, &S.ticks, &S.card, &S.indicator, &S.icons, &S.snap};
  for (int i = 0; i < 8; i++) *sh[i] = (uint8_t) (M.shape_v[i] > 0 ? M.shape_v[i] : 0);
  if (!presets(M, S, E)) return;
  // the edge pool in canonical order (part, then state), so key order in the file never changes the Skin
  int ne = 0;
  for (auto &p : M.part)
    for (Block &b : p.b)
      if (b.has & K_EDGES) { memcpy((void *) &M.pack[ne], (const void *) &S.edges[b.edge0], sizeof(Edge) * b.n_edges); b.edge0 = (uint16_t) ne; ne += b.n_edges; }
  memset((void *) S.edges, 0, sizeof S.edges);
  memcpy((void *) S.edges, (const void *) M.pack, sizeof(Edge) * (size_t) ne);
  S.n_edges = (uint16_t) ne;
  // lengths to px at both densities (cells = whole cells of the theme's stroke:pixel)
  for (int i = 0; i < S.n_edges; i++) {
    Edge &e = S.edges[i];
    const int mul = e.inset.set == L_TOKEN && e.inset.cells ? e.inset.cells : 1;
    if (e.inset.set == L_TOKEN) e.inset.cells = 0;
    resolve(e.w, S); resolve(e.inset, S);
    for (int d = 0; d < 2; d++) e.inset.px[d] = (int16_t) (e.inset.px[d] * mul);
  }
  for (auto &p : M.part) for (Block &b : p.b) resolve(b, S);
  resolve(S.buttons.w, S); resolve(S.buttons.h, S); resolve(S.buttons.gap, S); resolve(S.scrollbar.w, S); resolve(S.scrollbar.lane, S);
  // how far a part's solid edge layers reach in, over its blocks (a flush bar sits just inside its frame's edges, C45)
  auto solid_reach = [&](int p, int d) {
    int r = 0;
    for (const Block &b : M.part[p].b)
      for (int k = 0; (b.has & K_EDGES) && k < b.n_edges; k++) { const Edge &e = S.edges[b.edge0 + k]; if (!e.dotted) r = std::max(r, e.inset.px[d] + e.w.px[d]); }
    return r;
  };
  for (int d = 0; d < 2; d++)  // a flush header: the page's edges fit inside the inset that moves into the bar (the geometry guard proves it on the Skin too)
    if (M.part[G::P_HEADER].b[0].flush && solid_reach(G::P_PAGE, d) > S.size[d][G::SZ_INSET]) E.add("parts.header.flush", "the page's edges reach past layout:inset at " + std::string(DEN[d]));
  // authored checks, with the path the author wrote
  for (int p = 0; p < N_PARTS; p++) {
    const G::Part &P = G::PARTS[p];
    for (int st = 0; st <= N_STATES; st++) {
      const Block &b = M.part[p].b[st];
      const std::string at = state_path(p, st);
      for (int d = 0; d < 2; d++) {
        if ((b.has & K_RADIUS) && P.radius >= 0) {  // the part's radius token range (C19), and at most half its height
          const G::Size &z = G::SIZES[P.radius];
          const int v = b.radius.px[d];
          if (v < z.lo[d] || v > z.hi[d]) E.add(at + ".radius", std::to_string(v) + " at " + DEN[d] + " is outside " + std::to_string(z.lo[d]) + " .. " + std::to_string(z.hi[d]) + " (the range of " + z.name + ", C19)");
          else if (P.height >= 0 && 2 * v > S.size[d][P.height]) E.add(at + ".radius", std::to_string(v) + " at " + DEN[d] + " is more than half of " + G::SIZES[P.height].name + " " + std::to_string(S.size[d][P.height]));
        }
        if (b.has & K_PAD) {  // a track's padding insets its fill; a bar's is moved padding (C45): whole cells, within the edge cap
          const int v = b.pad.px[d], cell = S.size[d][G::SZ_PIXEL];
          const bool bar = p == G::P_HEADER || p == G::P_CARD_HEAD;
          int most = G::EDGE_PX[d];
          std::string why = "px (C48)";
          if (bar && !M.part[p].b[0].flush) { E.add(at + ".pad", "a bar is padded only with flush: true (its padding is moved out of its frame, C45)"); break; }
          if (p == G::P_HEADER) { most = (S.size[d][G::SZ_TOP] - S.size[d][G::SZ_INSET] - S.size[d][G::SZ_ROW]) / 2; why = "px: half the header gap it is taken from (layout:top less inset and row, C45)"; }
          if (p == G::P_CARD_HEAD) { most = (S.size[d][G::SZ_CARD_PAD] - S.size[d][G::SZ_HAIR] - solid_reach(G::P_CARD, d)) / 2; why = "px: half the card padding left inside the card's edges (C45)"; }
          if (v % cell) E.add(at + ".pad", std::to_string(v) + " at " + DEN[d] + " is not whole pixel cells (" + std::to_string(cell) + ")");
          else if (v < 0 || v > most) E.add(at + ".pad", std::to_string(v) + " at " + DEN[d] + " is more than " + std::to_string(most) + " " + why);
        }
        if (b.has & K_SHADOW) {
          const int dx = b.shadow.dx.px[d], dy = b.shadow.dy.px[d], c = G::SHADOW_PX[d];
          if (dx < -c || dx > c || dy < -c || dy > c) E.add(at + ".shadow", "offset " + std::to_string(dx) + ", " + std::to_string(dy) + " at " + DEN[d] + ": a shadow is at most " + std::to_string(c) + " px off its box (it is drawn outside it)");
          else if (!dx && !dy) { E.add(at + ".shadow", "dx and dy are 0 at " + std::string(DEN[d]) + ": a shadow needs an offset"); break; }
        }
        if ((b.has & K_FACE) && b.face.fill == FILL_BLOCKS && b.face.run.px[d] < 1) { E.add(at + ".face.run", "0 px at " + std::string(DEN[d]) + ": a block is at least 1 px"); break; }
      }
      if (b.has & K_EDGES)
        for (int k = 0; k < b.n_edges; k++)
          for (int d = 0; d < 2; d++)
            if (S.edges[b.edge0 + k].w.px[d] < 1) { E.add(at + ".edges[" + std::to_string(k) + "].w", "0 px at " + std::string(DEN[d]) + ": an edge is at least 1 px wide"); break; }
    }
  }
  for (int d = 0; d < 2; d++) {  // the base-only geometry: caption buttons inside the header row, the scrollbar at most a touch target wide
    const Buttons &bt = S.buttons;
    if (bt.n && (bt.w.px[d] < 1 || bt.h.px[d] < 1 || bt.w.px[d] > S.size[d][G::SZ_ROW] || bt.h.px[d] > S.size[d][G::SZ_ROW] || bt.gap.px[d] > S.size[d][G::SZ_ROW]))
      E.add("parts.header.buttons", "w, h and gap at " + std::string(DEN[d]) + " are 1 .. " + std::to_string(S.size[d][G::SZ_ROW]) + " px (the header row, layout:row)");
    if ((M.part[G::P_SCROLLBAR].b[0].has & K_W) && (S.scrollbar.w.px[d] < 1 || S.scrollbar.w.px[d] > S.size[d][G::SZ_TOUCH]))
      E.add("parts.scrollbar.w", std::to_string(S.scrollbar.w.px[d]) + " at " + DEN[d] + " is outside 1 .. " + std::to_string(S.size[d][G::SZ_TOUCH]) + " (at most touch:min)");
    if (S.scrollbar.has_lane && S.scrollbar.lane.fill == FILL_BLOCKS && S.scrollbar.lane.run.px[d] < 1) E.add("parts.scrollbar.lane.run", "a block is at least 1 px");
  }
  for (int p = 0; p < N_PARTS; p++) for (int st = 0; st <= N_STATES; st++) S.state[p][st] = M.part[p].b[st];
}

// ---------------------------------------------------------------- the guards (C47) on a compiled Skin
double lin(double v) { const double s = v / 255; return s <= 0.03928 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4); }
double lum(uint32_t c) { return 0.2126 * lin(c >> 16 & 255) + 0.7152 * lin(c >> 8 & 255) + 0.0722 * lin(c & 255); }
double ratio(uint32_t a, uint32_t b) { const double x = lum(a), y = lum(b); return (fmax(x, y) + 0.05) / (fmin(x, y) + 0.05); }
uint32_t q565(uint32_t c) {  // RGB565 as LVGL stores it (truncate) and the panel shows it (bit replication)
  const uint32_t r = c >> 19 & 31, g = c >> 10 & 63, b = c >> 3 & 31;
  return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}
uint32_t mixc(uint32_t a, uint32_t b, int mix) {  // lv_color_mix(a, b, mix): a x mix + b x (255 - mix), as contrast.mjs
  uint32_t o = 0;
  for (int sh = 16; sh >= 0; sh -= 8) o |= ((((a >> sh & 255) * (uint32_t) mix + (b >> sh & 255) * (uint32_t) (255 - mix)) * 0x8081u) >> 23) << sh;
  return o;
}
int room(const Skin &S, int p, const Block &b, int d, std::string &why) {  // how far in an edge layer may reach (theme.json parts room)
  const G::Part &P = G::PARTS[p];
  switch (P.room) {
    case G::ROOM_PAD: why = std::string("its padding ") + G::SIZES[P.room_size].name; return S.size[d][P.room_size];
    case G::ROOM_HEIGHT:  // half the height, less the line box of the text it holds (if any): edges never reach the content
      why = std::string("half of ") + G::SIZES[P.room_size].name + (P.room_less >= 0 ? std::string(" less the ") + G::TYPES[P.room_less].name + " line box" : "");
      return (S.size[d][P.room_size] - (P.room_less >= 0 ? G::TYPES[P.room_less].box[d] : 0)) / 2;
    case G::ROOM_STROKE: why = std::string("leave a pixel cell of ") + G::SIZES[P.room_size].name; return (S.size[d][P.room_size] - S.size[d][G::SZ_PIXEL]) / 2;
    case G::ROOM_BUTTONS: if (S.buttons.n) { why = "half its caption button height"; return S.buttons.h.px[d] / 2; } break;
    case G::ROOM_WIDTH: if (S.state[G::P_SCROLLBAR][0].has & K_W) { why = "half the scrollbar width"; return S.scrollbar.w.px[d] / 2; } break;
    default: break;
  }
  (void) b;
  why = "the edge cap (C48)";
  return G::EDGE_PX[d];
}
struct Guard {
  const Skin &S;
  const Model *M;
  Errs *E;
  std::vector<ContrastRow> *rows;
  uint32_t checks = 0;
  std::vector<std::string> seen = {};  // one error per failing pair and mode (accent sets that do not change it repeat it)
  uint32_t val(int ink, int m, int s) const {  // a colour token or a mix (colour.json mixes) in mode m, set s
    if (ink < N_COLOURS) return S.palette[m][s][ink];
    const G::Mix &x = G::MIXES[ink - N_COLOURS];
    uint32_t t = val(x.tint, m, s);
    if (x.toward >= 0) t = mixc(val(x.toward, m, s), t, x.desat);
    return mixc(t, val(x.over, m, s), x.opa);
  }
  static const char *ink_name(int ink) { return ink < N_COLOURS ? G::COLOURS[ink] : G::MIXES[ink - N_COLOURS].name; }
  static std::string ink_name(const Ink &k) {
    if (k.kind == INK_TOKEN) return G::COLOURS[k.value];
    if (k.kind == INK_ROLE) return G::ROLES[k.value];
    char b[12]; snprintf(b, sizeof b, "#%06X", (unsigned) k.value); return b;
  }
  uint32_t rgb(const Ink &k, int m, int s) const { return S.ink(k, m, s); }
  // key = what makes two failures the same one (the same colours in another accent set, the same block inherited by
  // another state combination): listed once, at the first path it shows up at
  void fail(const std::string &path, const std::string &msg, int line_part = -1, const std::string &key = "") {
    if (!E) return;
    const std::string k = key.empty() ? path + msg : key;
    for (const auto &x : seen) if (x == k) return;
    seen.push_back(k);
    E->add(path, msg, -1);
    if (line_part >= 0 && M && !E->E.empty() && E->E.back().path == path) E->E.back().line = M->part[line_part].line[0];
  }
  // one contrast check: fg on bg in mode m, set s; the worse of the exact and the RGB565 ratio decides
  bool check(const std::string &where, const std::string &fg, const std::string &bg, const char *kind, double min, int m, int s,
             uint32_t f, uint32_t b, int line_part = -1, const std::string &same = "") {
    checks++;
    const double ex = ratio(f, b), pn = ratio(q565(f), q565(b)), r = fmin(ex, pn);
    const bool pass = r >= min;
    if (rows) rows->push_back({where, fg, bg, kind, m, s, ex, pn, min, pass});
    if (!pass) {
      char t[200], key[160];
      snprintf(t, sizeof t, "%s on %s %.2f < %.1f (%s, %s, %s)", fg.c_str(), bg.c_str(), r, min, kind, G::MODES[m], S.set_label[s]);
      snprintf(key, sizeof key, "|%s|%s|%s|%.4f", fg.c_str(), bg.c_str(), G::MODES[m], r);
      fail(where, t, line_part, (same.empty() ? where : same) + key);
    }
    return pass;
  }
  int stops(const Face &f, int m, int s, uint32_t out[2]) const {
    out[0] = rgb(f.a, m, s);
    if (f.fill == FILL_SOLID) return 1;
    out[1] = rgb(f.b, m, s);
    return 2;
  }
  // the strongest of a drawn part's own ink and its outer edge layers (inset 0) on `under`
  double strongest(const Block &b, int p, int m, int s, uint32_t under, uint32_t &bc, std::string &what, bool own_ink) const {
    const G::Part &P = G::PARTS[p];
    double best = -1;
    auto take = [&](uint32_t c, const std::string &w) { const double r = fmin(ratio(c, under), ratio(q565(c), q565(under))); if (r > best) { best = r; bc = c; what = w; } };
    if (own_ink && (b.has & K_INK)) take(rgb(b.ink, m, s), ink_name(b.ink));
    else if (own_ink && P.face >= 0 && strcmp(P.kind, "box")) take(S.palette[m][s][P.face], G::COLOURS[P.face]);  // a drawn part: the component's own ink
    if (b.has & K_EDGES)
      for (int k = 0; k < b.n_edges; k++) { const Edge &e = S.edges[b.edge0 + k]; if (!e.inset.px[0] && !e.inset.px[1]) take(rgb(e.ink, m, s), "outer edge " + ink_name(e.ink)); }
    return best;
  }
  void contrast() {
    for (int m = 0; m < 2; m++) {
      if (!(S.modes >> m & 1)) continue;
      for (int s = 0; s < S.n_sets; s++) {
        // 1. the declared pairs of the theme scope, on the theme's palette
        for (int i = 0; i < G::N_PAIRS; i++) {
          const G::Pair &p = G::PAIRS[i];
          check(std::string("contrast.") + p.section, ink_name(p.fg), ink_name(p.bg), G::KINDS[p.kind], p.min, m, s, val(p.fg, m, s), val(p.bg, m, s));
        }
        // 2. the part pairs, in every combination of states
        for (int p = 0; p < N_PARTS; p++) {
          const G::Part &P = G::PARTS[p];
          const bool text_part = !strcmp(P.kind, "text") || !strcmp(P.kind, "icon"), outer = find(G::OUTER, G::N_OUTER, P.name) >= 0;
          for (int mask = 0; mask < N_COMBOS; mask++) {
            // a disabled part takes no press (C36: a blocked tap changes nothing), so pressed + disabled is never shown
            if ((mask & DISABLED) && (mask & PRESSED)) continue;
            const Block b = S.merged(p, mask);
            const bool off = mask & DISABLED;
            if (text_part) {  // a text part's own ink on the canvas and a card: 4.5 (an icon: 3.0)
              if (!(b.has & K_TEXT)) continue;
              const double min = off ? G::MIN_DISABLED : !strcmp(P.kind, "icon") ? G::MIN_UI : G::MIN_TEXT;
              for (int bg : {G::C_BG, G::C_SURFACE})
                check(combo_path(p, mask) + ".text", ink_name(b.text), G::COLOURS[bg], off ? "disabled" : "text", min, m, s, rgb(b.text, m, s), S.palette[m][s][bg], p, std::string(P.name) + ".text");
              continue;
            }
            if ((b.has & (K_FACE | K_TEXT)) && (P.n_texts || (b.has & K_TEXT))) {  // text on every face stop
              uint32_t st[2];
              int ns = 1;
              if (b.has & K_FACE) ns = stops(b.face, m, s, st);
              else if (P.face >= 0) st[0] = S.palette[m][s][P.face];
              else ns = 0;
              for (int k = 0; k < ns; k++) {
                const std::string fp = combo_path(p, mask) + ".face" + (ns > 1 ? (k ? " (2nd stop)" : " (1st stop)") : "");
                const bool caption = p == G::P_CAPTION;
                const std::string same = std::string(P.name) + ".face" + std::to_string(k);
                if (b.has & K_TEXT) check(fp, ink_name(b.text), "its face", off ? "disabled" : caption ? "ui" : "text", off ? G::MIN_DISABLED : caption ? G::MIN_UI : G::MIN_TEXT, m, s, rgb(b.text, m, s), st[k], p, same);
                else if (off) check(fp, "text-mute", "its face", "disabled", G::MIN_DISABLED, m, s, S.palette[m][s][G::C_TEXT_MUTE], st[k], p, same);
                else for (int t = 0; t < P.n_texts; t++) check(fp, G::COLOURS[P.texts[t]], "its face", "text", G::MIN_TEXT, m, s, S.palette[m][s][P.texts[t]], st[k], p, same);
              }
            }
            if (outer && !off) {  // outer edges and drawn inks on what the part sits on: 3.0, the strongest layer counts
              const uint32_t under = S.palette[m][s][P.sits_on];
              uint32_t bc = 0;
              std::string what;
              if (strongest(b, p, m, s, under, bc, what, true) < 0) continue;
              const std::string at = combo_path(p, mask) + ((b.has & K_INK) ? ".ink" : ".edges");
              check(at, what, G::COLOURS[P.sits_on], "ui", G::MIN_UI, m, s, bc, under, p, std::string(P.name) + ".outer");
            }
          }
        }
        // the scrollbar thumb on every stop of its lane (or on the canvas when it has none): 3.0
        const Block &sb = S.state[G::P_SCROLLBAR][0];
        if (S.scrollbar.has_lane || (sb.has & (K_FACE | K_EDGES))) {
          uint32_t ln[2];
          int nl = 1;
          if (S.scrollbar.has_lane) nl = stops(S.scrollbar.lane, m, s, ln);
          else ln[0] = S.palette[m][s][G::PARTS[G::P_SCROLLBAR].sits_on];
          for (int k = 0; k < nl; k++) {
            uint32_t bc = 0;
            std::string what;
            double best = strongest(sb, G::P_SCROLLBAR, m, s, ln[k], bc, what, false);
            const uint32_t face = (sb.has & K_FACE) ? rgb(sb.face.a, m, s) : S.palette[m][s][G::PARTS[G::P_SCROLLBAR].face];
            if (fmin(ratio(face, ln[k]), ratio(q565(face), q565(ln[k]))) > best) { bc = face; what = "the thumb"; }
            const char *under = !S.scrollbar.has_lane ? "the canvas (bg)" : nl > 1 ? (k ? "its lane (2nd stop)" : "its lane (1st stop)") : "its lane";
            check(S.scrollbar.has_lane ? "parts.scrollbar.lane" : "parts.scrollbar.face", what, under, "ui", G::MIN_UI, m, s, bc, ln[k], G::P_SCROLLBAR);
          }
        }
      }
    }
  }
  // what a face looks like on the panel: RGB565 stops, a two-stop face with equal stops = solid
  struct Look { int fill; uint32_t a, b; int16_t run[2], gap[2]; uint8_t down; };
  Look face_look(const Face &f, int m, int s) const {
    Look l{f.fill, q565(rgb(f.a, m, s)), 0, {0, 0}, {0, 0}, 0};
    if (f.fill != FILL_SOLID) l.b = q565(rgb(f.b, m, s));
    if (f.fill != FILL_SOLID && l.a == l.b) { l.fill = FILL_SOLID; l.b = 0; }
    if (l.fill == FILL_GRADIENT) l.down = f.down;
    if (l.fill == FILL_BLOCKS) { memcpy(l.run, f.run.px, 4); memcpy(l.gap, f.gap.px, 4); }
    return l;
  }
  bool same_look(const Look &x, const Look &y) const { return x.fill == y.fill && x.a == y.a && x.b == y.b && x.down == y.down && !memcmp(x.run, y.run, 4) && !memcmp(x.gap, y.gap, 4); }
  bool same_edges(const Block &a, const Block &b, int m, int s) const {
    if (a.n_edges != b.n_edges) return false;
    for (int k = 0; k < a.n_edges; k++) {
      const Edge &x = S.edges[a.edge0 + k], &y = S.edges[b.edge0 + k];
      if (x.side != y.side || x.dotted != y.dotted || memcmp(x.w.px, y.w.px, sizeof x.w.px) || memcmp(x.inset.px, y.inset.px, sizeof x.inset.px) || q565(rgb(x.ink, m, s)) != q565(rgb(y.ink, m, s))) return false;
    }
    return true;
  }
  // C36: a pressable part whose pressed look fixes the face must look different from the same states unpressed
  void press() {
    for (int p = 0; p < N_PARTS; p++) {
      const G::Part &P = G::PARTS[p];
      if (!P.press) continue;
      for (int rest = 0; rest < N_COMBOS; rest++) {
        if (rest & PRESSED) continue;
        const Block R = S.merged(p, rest), D = S.merged(p, rest | PRESSED);
        if (!(D.has & K_FACE)) continue;  // the component's own pressed face shows through
        for (int m = 0; m < 2; m++) {
          if (!(S.modes >> m & 1)) continue;
          for (int s = 0; s < S.n_sets; s++) {
            checks++;
            Face rf = R.face;
            if (!(R.has & K_FACE)) { rf = Face(); rf.a.kind = INK_TOKEN; rf.a.value = (uint32_t) (P.face >= 0 ? P.face : 0); }
            bool same = same_look(face_look(rf, m, s), face_look(D.face, m, s));
            if (same) same = (R.has & K_EDGES) == (D.has & K_EDGES) && (!(D.has & K_EDGES) || same_edges(R, D, m, s));
            if (same) same = (R.has & K_TEXT) == (D.has & K_TEXT) && (!(D.has & K_TEXT) || q565(rgb(R.text, m, s)) == q565(rgb(D.text, m, s)));
            if (same) same = (R.has & K_SHADOW) == (D.has & K_SHADOW) && (!(D.has & K_SHADOW) || (q565(rgb(R.shadow.ink, m, s)) == q565(rgb(D.shadow.ink, m, s)) && !memcmp(R.shadow.dx.px, D.shadow.dx.px, 4) && !memcmp(R.shadow.dy.px, D.shadow.dy.px, 4)));
            if (same) {
              char t[220];
              snprintf(t, sizeof t, "looks the same as %s (%s, %s): a pressed %s needs a different face, edges or text (C36)",
                       rest ? combo_name(rest).c_str() : "the rest state", G::MODES[m], S.set_label[s], P.name);
              fail(combo_path(p, rest | PRESSED), t, p);
            }
          }
        }
      }
    }
  }
  // geometry: edges inside the part's room (the padding; the page: the inset); a patterned face needs square corners
  void geometry() {
    for (int p = 0; p < N_PARTS; p++) {
      const G::Part &P = G::PARTS[p];
      for (int mask = 0; mask < N_COMBOS; mask++) {
        const Block b = S.merged(p, mask);
        for (int d = 0; d < 2; d++) {
          if ((b.has & K_EDGES) && b.n_edges) {
            checks++;
            int ext = 0;
            for (int k = 0; k < b.n_edges; k++) { const Edge &e = S.edges[b.edge0 + k]; ext = ext > e.inset.px[d] + e.w.px[d] ? ext : e.inset.px[d] + e.w.px[d]; }
            std::string why;
            const int r = room(S, p, b, d, why);
            if (ext > r) fail(combo_path(p, mask) + ".edges", std::to_string(ext) + " px in at " + DEN[d] + " is more than its room " + std::to_string(r < 0 ? 0 : r) + " (" + why + ")", p,
                              std::string(P.name) + ".edges " + std::to_string(b.edge0) + "/" + std::to_string(b.n_edges) + "/" + std::to_string(d) + "/" + std::to_string(r));
          }
          if ((b.has & K_FACE) && (b.face.fill == FILL_CHECKER || b.face.fill == FILL_BLOCKS)) {
            checks++;
            const int r = (b.has & K_RADIUS) ? b.radius.px[d] : P.radius >= 0 ? S.size[d][P.radius] : 0;
            if (r > 0) fail(combo_path(p, mask) + ".face", std::string("a ") + (b.face.fill == FILL_CHECKER ? "checker" : "blocks") + " face needs square corners (radius " + std::to_string(r) + " at " + DEN[d] + "): rounded patterned faces are not drawn", p,
                            std::string(P.name) + ".pattern " + std::to_string(r) + "/" + std::to_string(d));
          }
        }
      }
    }
  }
};

// ---------------------------------------------------------------- load
struct ModelBox {  // the scratch model in PSRAM on the panel, freed on every path
  Alloc<Model> a;
  Model *m;
  ModelBox() : m(a.allocate(1)) { new (m) Model(); }
  ~ModelBox() { m->~Model(); a.deallocate(m, 1); }
};
std::shared_ptr<Skin> new_skin() {
  auto s = std::allocate_shared<Skin>(Alloc<Skin, false>());
  memset((void *) s.get(), 0, sizeof(Skin));
  return s;
}
#ifndef ESP_PLATFORM
bool g_break_partial = false;
#endif
// an upper bound of the values a file holds (strings incl. keys, numbers, literals, objects, lists): the node array is
// reserved once at this size (capped), so the parse never reallocates and its peak memory is known in advance
size_t count_values(const char *s, size_t n) {
  size_t c = 1;
  for (size_t i = 0; i < n; i++) {
    const char ch = s[i];
    if (ch == '"') { c++; for (i++; i < n && s[i] != '"'; i++) if (s[i] == '\\') i++; }
    else if (ch == '{' || ch == '[') c++;
    else if (ch == '-' || (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) { c++; while (i + 1 < n && (isalnum((unsigned char) s[i + 1]) || s[i + 1] == '.' || s[i + 1] == '+' || s[i + 1] == '-')) i++; }
  }
  return c;
}

std::shared_ptr<const Skin> load_impl(const char *json, size_t n, std::vector<ThemeError> &err, std::shared_ptr<Skin> *partial) {
  err.clear();
  g_base = g_peak = g_now;
  Errs E{err, nullptr};
  if (!json || !n) { E.add("", "an empty file (a theme is a JSON object)"); return nullptr; }
  if (n > (size_t) G::MAX_BYTES) { E.add("", std::to_string(n) + " bytes: a theme file is at most " + std::to_string(G::MAX_BYTES / 1024) + " KB"); return nullptr; }
  Doc d;
  d.src = json; d.n = n;
  const size_t vals = count_values(json, n), cap = vals < (size_t) MAX_NODES ? vals : (size_t) MAX_NODES;
  d.nodes.reserve(cap);          // never grows: an upper bound of the values the file can hold
  d.arena.reserve(n + cap + 1);  // unescaped strings are never longer than their source, + a NUL each
  int root;
  bool ok = d.val(0, root);
  if (ok) { d.ws(); if (d.p < n) { d.fail("text after the theme"); ok = false; } }
  if (!ok) {
    int line = 1, col = 1;
    for (size_t q = 0; q < d.err_off && q < n; q++) { if (json[q] == '\n') { line++; col = 1; } else col++; }
    err.push_back({"", "line " + std::to_string(line) + ", column " + std::to_string(col) + ": " + d.err, line});
    return nullptr;
  }
  E.d = &d;
  ModelBox mb;
  std::shared_ptr<Skin> S = new_skin();
  Reader R{d, E, *mb.m, *S};
  if (partial) *partial = S;
  if (!phase1(d, E, *mb.m, R)) return nullptr;
  compile(*mb.m, *S, E);
  if (!err.empty()) return nullptr;
  Guard g{*S, mb.m, &E, nullptr};
  g.contrast(); g.press(); g.geometry();
  if (!err.empty()) return nullptr;
  S->checks = g.checks;
  if (partial) partial->reset();
  return S;
}

std::shared_ptr<const Skin> g_live;   // accessed only through std::atomic_load / std::atomic_store
std::atomic<uint32_t> g_generation{1};
}  // namespace

uint32_t skin::Skin::ink(const Ink &k, int light, int set_i) const {
  if (k.kind == INK_TOKEN) return rgb(light, set_i, (int) k.value);
  if (k.kind == INK_ROLE) return rgb(light, set_i, G::ROLE_COLOUR[k.value]);
  return k.kind == INK_HEX ? k.value : 0;
}
skin::Block skin::Skin::merged(int part, int states) const {
  Block b = state[part][0];
  for (int i = 0; i < N_STATES; i++) if (states >> i & 1) overlay(b, state[part][1 + i]);
  return b;
}
uint64_t skin::Skin::digest() const {
  const unsigned char *p = reinterpret_cast<const unsigned char *>(this);
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < sizeof(Skin); i++) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

std::shared_ptr<const Skin> theme_load(const char *json, size_t n, std::vector<ThemeError> &err) { return load_impl(json, n, err, nullptr); }

std::shared_ptr<const Skin> theme_facet() {
  static const std::shared_ptr<const Skin> facet = [] {  // the generated defaults, compiled like any theme (no file read)
    std::vector<ThemeError> err;
    Errs E{err, nullptr};
    ModelBox mb;
    std::shared_ptr<Skin> S = new_skin();
    snprintf(S->id, sizeof S->id, "facet");
    snprintf(S->label, sizeof S->label, "Facet");
    compile(*mb.m, *S, E);
    Guard g{*S, mb.m, nullptr, nullptr};
    g.contrast(); g.press(); g.geometry();
    S->checks = g.checks;  // tools/theme_check.mjs proves at build time that facet.json compiles to exactly this, guards clean
    return std::shared_ptr<const Skin>(S);
  }();
  return facet;
}
int theme_builtins() { return G::N_BUILTINS; }
const char *theme_builtin_id(int i) { return i >= 0 && i < G::N_BUILTINS ? G::BUILTINS[i].id : nullptr; }
const char *theme_builtin_json(int i) { return i >= 0 && i < G::N_BUILTINS ? G::BUILTINS[i].json : nullptr; }

std::shared_ptr<const Skin> theme_live() {
  std::shared_ptr<const Skin> s = std::atomic_load(&g_live);
  return s ? s : theme_facet();
}
uint32_t theme_generation() { return g_generation.load(); }
bool theme_apply(const char *json, size_t n, std::vector<ThemeError> &err) {
  std::shared_ptr<Skin> partial;
#ifndef ESP_PLATFORM
  auto s = load_impl(json, n, err, g_break_partial ? &partial : nullptr);
  if (!s && g_break_partial && partial) { std::atomic_store(&g_live, std::shared_ptr<const Skin>(partial)); g_generation++; }  // the break-it fault
#else
  auto s = load_impl(json, n, err, nullptr);
#endif
  if (!s) return false;
  std::atomic_store(&g_live, s);
  g_generation++;
  return true;
}
void theme_reset() { std::atomic_store(&g_live, theme_facet()); g_generation++; }
void theme_contrast(const Skin &s, std::vector<ContrastRow> &rows) {
  rows.clear();
  Guard g{s, nullptr, nullptr, &rows};
  g.contrast();
}
#ifndef ESP_PLATFORM
void theme_break_partial(bool on) { g_break_partial = on; }
#endif
size_t theme_peak_bytes() { return g_peak - g_base; }
}  // namespace fx
