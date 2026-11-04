// Sax: a streaming JSON reader for one reply (a WS result read in pieces, never held whole): the Energy feed's
// reads and the HA link's registry reads (core/ha_link.cpp). Header-only (core/*.h is bundled as is).
#pragma once
#include <functional>
#include <string>

namespace fw::json {
// leaf() for every scalar (strings capped at CAP), close() at an
// object's end; the path = each open container's key (object) / index (array), held per depth, nothing else
struct Sax {
  static constexpr int DEPTH = 10;
  static constexpr size_t CAP = 200;
  struct F { bool obj; bool want_key; int idx; std::string key; };
  F st[DEPTH];
  int depth = 0;
  bool in_str = false, esc = false, is_key = false, bad = false, ok = true;
  int hex = -1;
  std::string buf, lit;
  std::function<void(Sax &, const std::string &, bool)> leaf;  // (value, a string?)
  std::function<void(Sax &)> close;                             // the object at depth - 1 ends
  const std::string &key(int d) const { static const std::string none; return d < depth && st[d].obj ? st[d].key : none; }
  int idx(int d) const { return d < depth && !st[d].obj ? st[d].idx : -1; }
  void scalar(const std::string &v, bool str) {
    if (depth == 1 && key(0) == "success" && v == "false") ok = false;
    if (leaf) leaf(*this, v, str);
  }
  void end_lit() { if (!lit.empty()) { scalar(lit, false); lit.clear(); } }
  void feed(const char *p, size_t n) {
    for (size_t i = 0; i < n && !bad; i++) {
      const char c = p[i];
      if (in_str) {
        if (hex >= 0) { if (++hex == 4) hex = -1; continue; }  // \uXXXX: ids and keys are ASCII (dropped)
        if (esc) { esc = false; if (c == 'u') hex = 0; else if (buf.size() < CAP) buf += c; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c != '"') { if (buf.size() < CAP) buf += c; continue; }
        in_str = false;
        if (is_key) st[depth - 1].key = buf;
        else scalar(buf, true);
        continue;
      }
      switch (c) {
        case '"':
          in_str = true, buf.clear();
          is_key = depth > 0 && st[depth - 1].obj && st[depth - 1].want_key;
          break;
        case '{': case '[':
          if (depth >= DEPTH) { bad = true; break; }
          st[depth] = {c == '{', c == '{', 0, std::string()};
          depth++;
          break;
        case '}': case ']':
          end_lit();
          if (!depth || st[depth - 1].obj != (c == '}')) { bad = true; break; }
          if (c == '}' && close) close(*this);
          depth--;
          break;
        case ':':
          if (depth) st[depth - 1].want_key = false;
          break;
        case ',':
          end_lit();
          if (depth) { if (st[depth - 1].obj) st[depth - 1].want_key = true; else st[depth - 1].idx++; }
          break;
        case ' ': case '\n': case '\r': case '\t':
          break;
        default:
          if (lit.size() < 32) lit += c;
      }
    }
  }
};
}  // namespace fw::json
