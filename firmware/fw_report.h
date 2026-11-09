// The problem report (GET /fw/report, #125): what is kept across restarts and how it is written out. Pure C++ (no
// ESP-IDF): the device side (fw_device.h "problem report") puts one Kept in memory that a restart doesn't clear (RTC
// no-init; power loss clears it) and feeds it; the host test (fw_report.test.cpp, renderer/check.sh fw-report) feeds
// a plain one.
//   log ring     the last log lines as text, oldest overwritten; a boot writes a "--- boot N ---" line into it
//   boots        the last BOOTS boots: reset reason, how long each ran, wall time at start
//   connections  the last CONNS Wi-Fi / Home Assistant drops and reconnects: when, for how long, why, signal
// Everything that leaves the panel goes through redact() (tokens, keys, passwords) and scrub() (addresses, network
// names, entity ids, URL queries, user screen ids), so a report is safe to paste into a public issue.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace fw::report {

constexpr uint32_t MAGIC = 0x46525031;  // "FRP1": bump when Kept's layout changes
constexpr int BOOTS = 8, CONNS = 32;

enum Link : uint8_t { WIFI = 0, API = 1, HA = 2, LINKS = 3 };  // Wi-Fi, ESPHome API (HA's integration), the panel's own HA link
inline const char *link_name(int k) { return k == WIFI ? "Wi-Fi" : k == API ? "HA API" : k == HA ? "HA link" : "?"; }

struct Boot {
  uint32_t n;      // boot number (counts while the memory is kept)
  uint32_t epoch;  // wall time when it was first known this boot (0 = never)
  uint32_t ran_s;  // uptime at the last tick: how long it ran
  uint8_t reason;  // esp_reset_reason_t of this boot
  uint8_t pad[3];
};
struct Conn {
  uint32_t boot, at_s, epoch;  // when it dropped (boot number, uptime, wall time if known)
  uint32_t down_s;             // how long it was down (0 = still down / didn't come back this boot)
  uint8_t link;                // Link
  uint8_t reason;              // Wi-Fi disconnect reason (IDF wifi_err_reason_t, 0 = unknown)
  int8_t rssi;                 // signal when it dropped (0 = unknown)
  uint8_t open;                // 1 = not back yet
};

template<size_t LOG>
struct Kept {
  uint32_t magic, boot, log_head, log_used;
  uint32_t boot_next, conn_next;
  Boot boots[BOOTS];
  Conn conns[CONNS];
  char log[LOG];

  // on boot: keep what is there if it is whole (a software, watchdog or crash restart), else start empty (power on)
  bool begin() {
    const bool ok = magic == MAGIC && log_head < LOG && log_used <= LOG && boot_next < BOOTS && conn_next < CONNS;
    if (!ok) {
      memset(this, 0, sizeof *this);
      magic = MAGIC;
    }
    return ok;
  }
  Boot &new_boot(uint8_t reason) {
    boot++;
    Boot &b = boots[boot_next];
    b = Boot{};
    b.n = boot, b.reason = reason;
    boot_next = (boot_next + 1) % BOOTS;
    return b;
  }
  Boot *this_boot() { return boot ? &boots[(boot_next + BOOTS - 1) % BOOTS] : nullptr; }
  // a log line (no newline needed); a line over a quarter of the ring keeps only its end
  void put(const char *s, size_t n) {
    if (n > LOG / 4) s += n - LOG / 4, n = LOG / 4;
    for (size_t i = 0; i <= n; i++) {
      log[log_head] = i < n ? (s[i] == '\n' ? ' ' : s[i]) : '\n';
      log_head = (log_head + 1) % LOG;
    }
    log_used = log_used + n + 1 > LOG ? LOG : log_used + n + 1;
  }
  // the kept lines, oldest first, one call per line (without the newline); a line the ring cut is skipped
  template<typename F> void lines(F &&emit) const {
    size_t start = (log_head + LOG - log_used) % LOG, n = log_used;
    if (log_used == LOG) {  // wrapped: the oldest line lost its start
      while (n && log[start] != '\n') start = (start + 1) % LOG, n--;
      if (n) start = (start + 1) % LOG, n--;
    }
    std::string line;
    for (size_t i = 0; i < n; i++) {
      const char c = log[(start + i) % LOG];
      if (c == '\n') { emit(line); line.clear(); }
      else line += c;
    }
  }
  Conn &drop(uint8_t link, uint32_t at_s, uint32_t epoch, uint8_t reason, int8_t rssi) {
    Conn &c = conns[conn_next];
    c = Conn{boot, at_s, epoch, 0, link, reason, rssi, 1};
    conn_next = (conn_next + 1) % CONNS;
    return c;
  }
  // the link is back: close its open drop of this boot
  void back(uint8_t link, uint32_t at_s) {
    for (int i = 0; i < CONNS; i++) {
      Conn &c = conns[(conn_next + CONNS - 1 - i) % CONNS];
      if (c.link == link && c.open && c.boot == boot && c.at_s) { c.open = 0, c.down_s = at_s - c.at_s; return; }
    }
  }
  template<typename F> void each_conn(F &&f) const {
    for (int i = 0; i < CONNS; i++) {
      const Conn &c = conns[(conn_next + i) % CONNS];
      if (c.boot) f(c);
    }
  }
  template<typename F> void each_boot(F &&f) const {
    for (int i = 0; i < BOOTS; i++) {
      const Boot &b = boots[(boot_next + i) % BOOTS];
      if (b.n) f(b);
    }
  }
};

// ---------------------------------------------------------------- redaction
inline bool word_ch(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
inline bool token_ch(char c) { return word_ch(c) || c == '-' || c == '+' || c == '/' || c == '=' || c == '.'; }
inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char) (c - 'A' + 'a') : c; }
// Removes secrets from a line before it leaves the panel:
//   the value after a secret's name (token, password, psk, secret, key, api_key, bearer, authorization, ...) up to the
//   next space, quote, comma, ampersand or brace -> [redacted]
//   any run of 24+ token characters with a digit and a letter (a key or token printed on its own) -> [redacted],
//   except a version (it starts with a digit and has a dot; no key or token does)
inline std::string redact(const std::string &in) {
  static const char *const NAMES[] = {"token", "password", "passwd", "pass", "psk", "secret", "key", "apikey",
                                      "api_key", "bearer", "authorization", "auth", "noise_psk", "encryption"};
  std::string s = in, out;
  for (size_t i = 0; i < s.size();) {
    bool hit = false;
    if (i == 0 || !word_ch(s[i - 1])) {
      for (const char *nm : NAMES) {
        const size_t L = strlen(nm);
        if (i + L > s.size()) continue;
        size_t k = 0;
        while (k < L && lower(s[i + k]) == nm[k]) k++;
        if (k < L) continue;
        size_t j = i + L;
        if (j < s.size() && word_ch(s[j])) continue;  // "keys", "tokens_ok": not a secret's name
        // separators: quotes, ':', '=', spaces (at least one)
        size_t v = j;
        while (v < s.size() && (s[v] == '"' || s[v] == '\'' || s[v] == ':' || s[v] == '=' || s[v] == ' ')) v++;
        if (v == j || v >= s.size()) continue;
        size_t e = v;
        while (e < s.size() && s[e] != ' ' && s[e] != '"' && s[e] != '\'' && s[e] != ',' && s[e] != '&' &&
               s[e] != '}' && s[e] != ';')
          e++;
        if (e == v) continue;
        out.append(s, i, v - i);
        out += "[redacted]";
        i = e;
        hit = true;
        break;
      }
    }
    if (hit) continue;
    if (token_ch(s[i]) && (i == 0 || !token_ch(s[i - 1]))) {
      size_t e = i;
      bool dig = false, alpha = false;
      while (e < s.size() && token_ch(s[e])) {
        dig |= s[e] >= '0' && s[e] <= '9';
        alpha |= (s[e] >= 'a' && s[e] <= 'z') || (s[e] >= 'A' && s[e] <= 'Z');
        e++;
      }
      // a version ("0.5.0-beta6-dev202611061147+9d9ef80": starts with a digit, has a dot) is not a secret
      const bool version = s[i] >= '0' && s[i] <= '9' && s.find('.', i) < e;
      if (e - i >= 24 && dig && alpha && !version) {
        out += "[redacted]";
        i = e;
        continue;
      }
      out.append(s, i, e - i);
      i = e;
      continue;
    }
    out += s[i++];
  }
  return out;
}

// ---------------------------------------------------------------- scrub (#176)
// Takes out what identifies a home, after redact() took out the secrets, so the report is safe to paste in public:
//   URLs           scheme://[host]/path, no query or fragment (a picture's cache= id, a token); github.com stays
//   paths          /a/b?x=1 -> /a/b (Home Assistant's relative picture URLs)
//   IPv4 [+port]   [address]; MAC addresses (a BSSID) [address]
//   ssid=...       ssid=[name] (ESPHome's Wi-Fi warnings name the network)
//   entity ids     <domain>.[entity] for Home Assistant's domains (media_player.spotify_<account> etc.)
//   spec boot ids  "spec boot: <id>: ..." -> "spec boot: [screen]: ..." unless the id is built in (builtin(id))
// Kept: firmware, board, uptime, boot and reset reasons, heap, settings values, error words.
inline const char *const HA_DOMAINS[] = {
    "alarm_control_panel", "assist_satellite", "automation", "binary_sensor", "button", "calendar", "camera",
    "climate", "conversation", "counter", "cover", "date", "datetime", "device_tracker", "event", "fan", "group",
    "humidifier", "image", "input_boolean", "input_button", "input_datetime", "input_number", "input_select",
    "input_text", "lawn_mower", "light", "lock", "media_player", "notify", "number", "person", "remote", "scene",
    "schedule", "script", "select", "sensor", "siren", "stt", "sun", "switch", "text", "time", "timer", "todo", "tts",
    "update", "vacuum", "valve", "wake_word", "water_heater", "weather", "zone"};
inline bool hex_ch(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
inline bool dig_ch(char c) { return c >= '0' && c <= '9'; }
// the end of a URL or path: space, quote, brackets, comma, angle brackets
inline bool url_end(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\'' || c == ',' || c == '<' || c == '>' || c == ')' ||
         c == '}' || c == ']' || c == '\\' || c == ';';
}
// an IPv4 address at i (four numbers 0..255, not part of a longer dotted run): its length, else 0
inline size_t ipv4_at(const std::string &s, size_t i) {
  if (i > 0 && (dig_ch(s[i - 1]) || s[i - 1] == '.' || word_ch(s[i - 1]))) return 0;
  size_t j = i;
  for (int part = 0; part < 4; part++) {
    if (part) { if (j >= s.size() || s[j] != '.') return 0; j++; }
    size_t k = j;
    int v = 0;
    while (k < s.size() && dig_ch(s[k]) && k - j < 4) v = v * 10 + (s[k] - '0'), k++;
    if (k == j || k - j > 3 || v > 255) return 0;
    j = k;
  }
  if (j < s.size() && (word_ch(s[j]) || (s[j] == '.' && j + 1 < s.size() && dig_ch(s[j + 1])))) return 0;
  if (j + 1 < s.size() && s[j] == ':' && dig_ch(s[j + 1])) {  // :port
    j++;
    while (j < s.size() && dig_ch(s[j])) j++;
  }
  return j - i;
}
// a MAC address at i (six hex pairs with ':' or '-'): its length, else 0
inline size_t mac_at(const std::string &s, size_t i) {
  if (i > 0 && (word_ch(s[i - 1]) || s[i - 1] == ':')) return 0;
  if (i + 17 > s.size()) return 0;
  const char sep = s[i + 2];
  if (sep != ':' && sep != '-') return 0;
  for (int k = 0; k < 6; k++) {
    if (!hex_ch(s[i + 3 * k]) || !hex_ch(s[i + 3 * k + 1])) return 0;
    if (k < 5 && s[i + 3 * k + 2] != sep) return 0;
  }
  if (i + 17 < s.size() && (word_ch(s[i + 17]) || s[i + 17] == sep)) return 0;
  return 17;
}
inline std::string scrub(const std::string &in, bool (*builtin)(const std::string &) = nullptr) {
  // 1. URLs and paths: host out, query and fragment out
  std::string a;
  a.reserve(in.size());
  bool in_path = false;  // inside a token that has a '/' (a URL's path or a bare path)
  for (size_t i = 0; i < in.size();) {
    const char c = in[i];
    if (c == ':' && in.compare(i, 3, "://") == 0 && i > 0 && word_ch(in[i - 1])) {
      size_t h = i + 3;
      while (h < in.size() && !url_end(in[h]) && in[h] != '/' && in[h] != '?' && in[h] != '#') h++;
      if (in.compare(i + 3, h - i - 3, "github.com") == 0) {  // Facet's own public pages (the how-to's form) stay
        while (i < in.size() && !url_end(in[i])) a += in[i++];
        continue;
      }
      a += "://[host]";
      i = h;
      in_path = true;
      continue;
    }
    if (url_end(c)) in_path = false;
    else if (c == '/') in_path = true;
    else if (in_path && (c == '?' || c == '#')) {  // the query / fragment of a URL or path: out
      while (i < in.size() && !url_end(in[i])) i++;
      in_path = false;
      continue;
    }
    a += c;
    i++;
  }
  // 2. addresses, network names, entity ids, user screen ids
  std::string o;
  o.reserve(a.size());
  for (size_t i = 0; i < a.size();) {
    if (size_t n = ipv4_at(a, i)) { o += "[address]"; i += n; continue; }
    if (size_t n = mac_at(a, i)) { o += "[address]"; i += n; continue; }
    if ((i == 0 || !word_ch(a[i - 1])) && (a.compare(i, 5, "ssid=") == 0 || a.compare(i, 5, "SSID=") == 0 ||
                                           a.compare(i, 6, "ssid: ") == 0 || a.compare(i, 6, "SSID: ") == 0)) {
      const size_t v = i + (a[i + 4] == '=' ? 5 : 6);
      o.append(a, i, v - i);
      size_t e = v;
      if (e < a.size() && (a[e] == '\'' || a[e] == '"')) {
        const char q = a[e++];
        while (e < a.size() && a[e] != q && a[e] != '\n') e++;
        if (e < a.size() && a[e] == q) e++;
      } else {
        while (e < a.size() && !url_end(a[e])) e++;
      }
      o += "[name]";
      i = e;
      continue;
    }
    if (a.compare(i, 11, "spec boot: ") == 0) {
      const size_t v = i + 11;
      size_t e = v;
      while (e < a.size() && a[e] != ':' && a[e] != '\n') e++;
      const std::string id = a.substr(v, e - v);
      o += "spec boot: ";
      o += id == "deck" || (builtin && builtin(id)) ? id : "[screen]";
      i = e;
      continue;
    }
    if ((i == 0 || (!word_ch(a[i - 1]) && a[i - 1] != '.')) && a[i] >= 'a' && a[i] <= 'z') {
      bool hit = false;
      for (const char *d : HA_DOMAINS) {
        const size_t L = strlen(d);
        if (a.compare(i, L, d) != 0 || i + L + 1 >= a.size() || a[i + L] != '.') continue;
        size_t e = i + L + 1;
        while (e < a.size() && ((a[e] >= 'a' && a[e] <= 'z') || dig_ch(a[e]) || a[e] == '_')) e++;
        if (e == i + L + 1) continue;
        o.append(a, i, L + 1);
        o += "[entity]";
        i = e;
        hit = true;
        break;
      }
      if (hit) continue;
    }
    o += a[i++];
  }
  return o;
}
// what the report may say about itself once scrub() and redact() ran (the page says the same)
constexpr const char *SAFE =
    "Safe to paste: no passwords, keys, tokens, addresses or names of your devices and entities.";

// the report's first lines (GET /fw/report.txt and the downloaded file): what to do with it, before the report itself
constexpr const char *HOWTO =
    "# To report a problem: open https://github.com/mrdarrengriffin/facet/issues/new?template=bug.yml\n"
    "# and paste this whole file into the Report box (or attach it), then say what happened.\n"
    "# Safe to paste: no passwords, keys, tokens, addresses or names of your devices and entities.\n\n";

// ---------------------------------------------------------------- text helpers
// strips ANSI colour codes (ESPHome's log lines carry them)
inline std::string plain(const char *s, size_t n) {
  std::string o;
  o.reserve(n);
  for (size_t i = 0; i < n; i++) {
    if (s[i] == 0x1b) {
      while (i < n && s[i] != 'm') i++;
      continue;
    }
    if (s[i] != '\r') o += s[i];
  }
  return o;
}
// "1 h 02 min", "3 min 05 s", "12 s"
inline std::string dur(uint32_t s) {
  char b[32];
  if (s >= 3600) snprintf(b, sizeof b, "%u h %02u min", (unsigned) (s / 3600), (unsigned) (s / 60 % 60));
  else if (s >= 60) snprintf(b, sizeof b, "%u min %02u s", (unsigned) (s / 60), (unsigned) (s % 60));
  else snprintf(b, sizeof b, "%u s", (unsigned) s);
  return b;
}
// the line written into the ring when a link drops or comes back
inline std::string conn_line(const Conn &c, bool back) {
  char b[96];
  if (back) snprintf(b, sizeof b, "%s back after %s", link_name(c.link), dur(c.down_s).c_str());
  else if (c.link == WIFI) snprintf(b, sizeof b, "Wi-Fi dropped (reason %u, signal %d dBm)", c.reason, c.rssi);
  else snprintf(b, sizeof b, "%s dropped", link_name(c.link));
  return b;
}

}  // namespace fw::report
