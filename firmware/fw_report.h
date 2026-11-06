// The problem report (GET /fw/report, #125): what is kept across restarts and how it is written out. Pure C++ (no
// ESP-IDF): the device side (fw_device.h "problem report") puts one Kept in memory that a restart doesn't clear (RTC
// no-init; power loss clears it) and feeds it; the host test (fw_report.test.cpp, renderer/check.sh fw-report) feeds
// a plain one.
//   log ring     the last log lines as text, oldest overwritten; a boot writes a "--- boot N ---" line into it
//   boots        the last BOOTS boots: reset reason, how long each ran, wall time at start
//   connections  the last CONNS Wi-Fi / Home Assistant drops and reconnects: when, for how long, why, signal
// Everything that leaves the panel goes through redact(): tokens, keys and passwords never appear in a report.
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

// the report's first lines (GET /fw/report.txt and the downloaded file): what to do with it, before the report itself
constexpr const char *HOWTO =
    "# To report a problem: open https://github.com/mrdarrengriffin/facet/issues/new?template=bug.yml\n"
    "# and paste this whole file into the Report box (or attach it), then say what happened.\n\n";

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
