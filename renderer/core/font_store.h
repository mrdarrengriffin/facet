// font_store.h: theme fonts the PANEL downloads itself (renderer/CONTRACT.md C49, Appendix A `font`; theme engine
// design "Fonts: built in OR downloaded"). Core only: components never include it (they name type tokens, C16).
//
// A theme font with `source: download` names an https URL of a TTF. The first time a theme needs it, the panel
// fetches the file once, checks it (TrueType structure, SIL OFL in the file's own name table, size cap, glyph
// coverage of the styles that draw it), keeps it in flash and draws it with LVGL TinyTTF; after that it is read from
// flash at boot: no delay, works offline, survives reboots. Until the file is there (offline, slow, refused) the
// theme's built-in fallback font draws; when the file arrives every look repaints live (fx::restyle, C44), and a
// refused file keeps the fallback and reports why (status()). The product path never depends on a hub: the panel
// does all of this itself. The I/O is behind two small interfaces so the WASM workbench and the tests simulate the
// network and the flash (missing, corrupt, slow, oversize, offline, reboot) with the same code the panel runs.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <functional>
#include <string>
#include <vector>
#include "lvgl.h"

namespace fx {
namespace fonts {
// GET url; done(http status, body) exactly once, later, on the LVGL task. Status 0 = no network / timed out; a body
// over `cap` bytes is not kept: status 413. A panel = HTTPS GET; WASM = the simulation (sim_*).
struct Fetch {
  virtual ~Fetch() = default;
  virtual void get(const std::string &url, size_t cap, std::function<void(int, std::vector<uint8_t> &&)> done) = 0;
};
// the flash: whole files or nothing (write = temp file + rename on a panel, LittleFS)
struct Store {
  virtual ~Store() = default;
  virtual bool read(const std::string &key, std::vector<uint8_t> &out) = 0;
  virtual bool write(const std::string &key, const std::vector<uint8_t> &data) = 0;
  virtual void remove(const std::string &key) = 0;
};
void use(Fetch *f, Store *s);  // the I/O (nullptr = none: every download font stays on its fallback)

constexpr size_t MAX_BYTES = 512 * 1024;   // the size cap of one file (flash + the RAM TinyTTF reads it from)
constexpr uint32_t RETRY_MS = 30000;       // offline / timed out: try again after this, doubling up to RETRY_MAX_MS
constexpr uint32_t RETRY_MAX_MS = 3600000;

enum State : uint8_t { NONE, LOADING, WAITING, READY, REFUSED };  // WAITING = offline, a retry is due
struct Status {
  std::string url, family, msg;  // msg: why it was refused / is waiting ("" when ready)
  State state = NONE;
  size_t bytes = 0;
  int fetches = 0, sizes = 0;    // GETs made; fonts made from it (one per px)
  bool from_store = false;       // ready from flash (no fetch since the last reset)
};

// The downloaded font for `url` at `px` px, or nullptr = draw the fallback now (the first ask starts the read from
// flash, else the fetch). need = the code points the styles that use it draw: printable ASCII must all be in the
// file; the rest too unless fallback_ok (then the fallback draws what the file lacks, LVGL font fallback).
const lv_font_t *get(const char *url, const char *family, int px, const std::vector<uint32_t> &need, bool fallback_ok,
                     const lv_font_t *fallback);
uint32_t generation();                 // bumps when a file turns ready or refused: the apply path repaints on it
void on_change(void (*cb)());          // called after a bump (the core: fx::restyle)
void status(std::vector<Status> &out);
void reset();                          // forget every font in RAM, as a reboot does: the stored files stay

// The check a downloaded file must pass (pure, no LVGL; fuzzed): a TrueType file (sfnt 1.0 / 'true', glyf outlines)
// whose tables are inside it, whose loca offsets are inside glyf and ascending, with a cmap (format 4 or 12) and a
// name record (copyright, licence or licence URL) naming the SIL Open Font License; `missing` = code points of
// `need` it has no glyph for (printable ASCII missing always refuses; others only when !allow_missing).
bool check(const uint8_t *p, size_t n, const std::vector<uint32_t> &need, bool allow_missing, std::string &why,
           int *missing = nullptr);
std::string key_of(const std::string &url);  // the flash file name of a URL ("f" + 16 hex of its FNV-1a)

#ifndef ESP_PLATFORM
// the simulation (WASM workbench + tests): the network and the flash in memory, time = lv_tick
void sim_install();                                                      // use() the simulated pair
void sim_serve(const std::string &url, int status, std::vector<uint8_t> body, uint32_t delay_ms);  // a response
void sim_offline(bool on);                                               // every GET fails with 0 after its delay
void sim_store_clear();                                                  // a new panel: no files
int sim_store_files();
bool sim_store_corrupt(const std::string &url);                          // flip bytes of a stored file
#endif
}  // namespace fonts
}  // namespace fx
