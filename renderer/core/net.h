#pragma once
// A web read for the feeds' direct sources (CONTRACT.md C51a: a source the panel calls itself, such as a public API
// that needs no key). get(url, done) asks the platform for one HTTPS GET; the body comes back in pieces (chunk) and
// the end (finish), on the LVGL task; done(ok, body, why) runs once. Bodies are capped (max bytes, then the read
// fails "too much data"): a feed asks only for what it shows (a small box round home). One read per url at a time
// (asking again while one is open returns the open one's id, its done replaced).
// The transport is the platform's: on_get(sink) starts the GET (WASM: the host fetches it, core/main.cpp
// fw_net_gets / fw_net_chunk / fw_net_got; the panels: the firmware's HTTP task). No sink = no transport: get() fails
// at once with why "no web access on this build".
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fw::net {
using Done = std::function<void(bool ok, const std::string &body, const std::string &why)>;
constexpr size_t MAX_BODY = 48 * 1024;  // the most a feed reads in one go (a busy sky within 100 km is ~25 KB)
constexpr uint32_t TIMEOUT_MS = 15000;  // a read open longer fails "timeout"
uint32_t get(const std::string &url, Done done, size_t max = MAX_BODY);  // 0 = failed at once (done already called)
void cancel(uint32_t id);  // done is never called for it
bool ready();              // a transport is set
using Sink = void (*)(uint32_t id, const std::string &url);
void on_get(Sink f);
void chunk(uint32_t id, const char *p, size_t n);
void finish(uint32_t id, bool ok, const std::string &why);
int open_reads();  // reads open now (tests)
}  // namespace fw::net
