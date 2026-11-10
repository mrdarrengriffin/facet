#pragma once
// HA "Firmware" update entity (ESPHome http_request update, packages/workbench.yaml) with the OTA probation folded in
// (no separate "Firmware on probation" entity). While the running image is not confirmed yet
// (fw::device::probation_left() >= 0, firmware/fw_device.h) the entity reads "installing" with the
// settling countdown as its progress and the summary "Update settling: don't unplug (m:ss)"; a power cut now rolls
// back. When confirmed it checks the manifest again (normal "up to date" / "update available"). Installs only when
// someone presses Install in HA: nothing here or in the YAML installs on its own.
// tick() runs every second (YAML interval); what it does when is fw_settle.h (host-tested): after the restart the entity
// reads up to date with this build's version the moment the image is confirmed (or at once on a boot with no
// probation), never blank or Installing while a manifest check is pending; the manifest is then asked at once, and
// again every minute until a check lands (one already running, no network or a failed fetch no longer leaves it to
// the hourly poll); a countdown at 0:00 that safe_mode never confirms is confirmed here after a minute.
// The installed version is the build's (fw::bundle::VERSION, firmware/bundle.mjs --version, in its own small .cpp), not
// ESPHOME_PROJECT_VERSION: that one sits in defines.h, which ~135 ESPHome objects include, so a version stamped per
// build recompiled all of them. The project version stays the release's (facet_version in the YAML); the entity
// reports the build's, fixed in its state callback, which runs before Home Assistant is told. No --version = the
// project version.
// Release notes: the manifest's summary is the offered version's only, so a panel that skips versions would never see
// the ones between. When an update is offered, notes() fetches the manifest once more (a task, as the entity's own
// check does), joins the notes of every version newer than this build's (fw_notes.h) and keeps that text as the
// entity's summary until the offer, the channel or the installed version changes. Nothing newer listed, or a manifest
// without notes (older releases): the manifest's ota.summary stays. Buffers (manifest, JSON, text) are plain heap,
// which prefers PSRAM on both boards (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 0); the task's stack is internal, 8 KB,
// only while it runs, after the entity's own check task has ended.
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>
#include <ctime>
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/components/http_request/update/http_request_update.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/preferences.h"
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "fw_notes.h"
#include "fw_settle.h"

namespace fw::bundle {
extern const char *const VERSION;  // facet_wb fw_bundle_info.cpp
}
namespace fw::dev {
void update_checked(const char *why);  // renderer core/device.h: Settings > Update's check ended
}

namespace panel::fwu {
using esphome::http_request::HttpRequestUpdate;

// reach the entity's protected state (no other way to show progress without installing). HttpRequestUpdate is final,
// so: a class derived from its base may name the base's protected members as pointers-to-member (&Peek::state_ has
// type UpdateState UpdateEntity::*), which then apply to any UpdateEntity. Peek is never instantiated.
using esphome::update::UpdateEntity;
struct Peek : UpdateEntity {
  static esphome::update::UpdateState UpdateEntity::*state() { return &Peek::state_; }
  static esphome::update::UpdateInfo UpdateEntity::*info() { return &Peek::update_info_; }
};
// whether the entity's manifest check is running: its task handle (protected in a final class, so no Peek). An
// explicit instantiation may name a member it can't access ([temp.spec] access rule); the friend hands the pointer
// out. Read on the main loop only, where the entity sets and clears it.
struct TaskTag {
  using type = TaskHandle_t HttpRequestUpdate::*;
  friend type task_member(TaskTag);
};
template <class Tag, typename Tag::type M> struct Rob {
  friend typename Tag::type task_member(Tag) { return M; }
};
template struct Rob<TaskTag, &HttpRequestUpdate::update_task_handle_>;
inline bool checking(HttpRequestUpdate *u) { return u->*task_member(TaskTag{}) != nullptr; }
inline const char *version() {
  if (fw::bundle::VERSION[0]) return fw::bundle::VERSION;
#ifdef ESPHOME_PROJECT_VERSION
  return ESPHOME_PROJECT_VERSION;
#else
  return "";
#endif
}
// the throwaway query of a manifest URL (fw_settle.h fresh_url): wall time once known, else a per-boot random base +
// uptime, so no two checks ask the same URL
inline uint32_t stamp() {
  const time_t t = ::time(nullptr);
  if (t > 1600000000) return (uint32_t) t;
  static const uint32_t base = esp_random() & 0x3fffffff;
  return base + (uint32_t) (esp_timer_get_time() / 1000000);
}
inline std::string fresh(const std::string &url) { return url.empty() ? url : fw::settle::fresh_url(url, stamp()); }
// the release notes of the versions between this build and the offered one (see the top)
struct Notes {
  esphome::http_request::HttpRequestComponent *http = nullptr;
  std::string url;           // the manifest the entity checks (the Update channel's)
  std::string key, text;     // url|offered|installed the joined notes are for; "" = keep ota.summary
  std::string want, result;  // the key being fetched, its text
  std::atomic<int> busy{0};  // 1 fetching, 2 result ready (tick() takes it on the main loop)
  bool failed = false;
  int64_t tried = -1;
};
inline Notes &notes() {
  static Notes n;
  return n;
}
static constexpr size_t NOTES_MANIFEST_MAX = 64 * 1024;
struct NotesJob {
  std::string url, running;
};
inline void notes_task(void *arg) {
  auto *job = static_cast<NotesJob *>(arg);
  auto &n = notes();
  bool ok = false;
  std::string out;
  auto c = n.http->get(job->url);
  if (c && c->status_code == 200 && c->content_length > 0 && c->content_length <= NOTES_MANIFEST_MAX) {
    const size_t len = c->content_length;
    std::vector<uint8_t> d(len);  // heap: PSRAM first
    auto r = esphome::http_request::http_read_fully(c.get(), d.data(), len, 256, n.http->get_timeout());
    const size_t got = c->get_bytes_read();
    c->end();
    c.reset();
    if (r.status == esphome::http_request::HttpReadStatus::OK) {
      std::vector<fw::notes::Entry> es;
      ok = esphome::json::parse_json(d.data(), got, [&es](JsonObject root) -> bool {
        for (JsonObject e : root["notes"].as<JsonArray>())
          if (e["version"].is<const char *>() && e["summary"].is<const char *>())
            es.emplace_back(e["version"].as<std::string>(), e["summary"].as<std::string>());
        return true;
      });
      if (ok) out = fw::notes::join(es, job->running.c_str());
    }
  }
  if (c) c->end();
  c.reset();
  delete job;
  n.result = std::move(out);
  n.failed = !ok;
  n.busy = 2;
  vTaskDelete(nullptr);
}
inline void notes_check(UpdateEntity *u) {
  auto &n = notes();
  auto &i = u->*Peek::info();
  if (!n.http || n.url.empty() || u->*Peek::state() != esphome::update::UPDATE_STATE_AVAILABLE || i.latest_version.empty())
    return;
  std::string key = n.url + '|' + i.latest_version + '|' + i.current_version;
  if (key == n.key) {
    if (!n.text.empty() && i.summary != n.text) i.summary = n.text;  // the entity's hourly check put ota.summary back
    return;
  }
  const int64_t now = esp_timer_get_time();
  if (n.busy || (key == n.want && n.tried >= 0 && now - n.tried < 600000000LL)) return;  // a failed fetch: again in 10 min
  n.want = key;
  n.tried = now;
  n.busy = 1;
  auto *job = new NotesJob{fresh(n.url), i.current_version};  // past the CDN's cache, as the entity's check
  if (xTaskCreate(notes_task, "fw_notes", 8192, job, 1, nullptr) != pdPASS) {
    delete job;
    n.busy = 0;
  }
}
// tick(): a finished fetch's text becomes the summary
inline void notes_take(UpdateEntity *u) {
  auto &n = notes();
  if (n.busy != 2) return;
  if (!n.failed) {
    n.key = n.want;
    n.text = std::move(n.result);
  }
  n.result.clear();
  n.busy = 0;
  if (!n.failed && !n.text.empty() && u->*Peek::state() == esphome::update::UPDATE_STATE_AVAILABLE) u->publish_state();
}

inline fw::settle::Clock &clock() {
  static fw::settle::Clock c;
  return c;
}
// the entity's check compares the manifest with ESPHOME_PROJECT_VERSION: compare with the build's version instead
inline void fix(UpdateEntity *u) {
  auto &i = u->*Peek::info();
  const auto st0 = u->*Peek::state();
  // a manifest check landed (only a check fills the title): the settle clock stops checking
  if (!i.title.empty() && (st0 == esphome::update::UPDATE_STATE_NO_UPDATE || st0 == esphome::update::UPDATE_STATE_AVAILABLE)) clock().landed();
  // offered only when strictly newer than this build (fw_notes.h newer()): the entity's own check offers any other
  // version, so a dev build on Beta / Stable (or a channel switched back) was offered an older one. No "switch back"
  // install: the channel moves on with its next release.
  if (version()[0]) {
    i.current_version = version();
    auto &st = u->*Peek::state();
    if (st == esphome::update::UPDATE_STATE_NO_UPDATE || st == esphome::update::UPDATE_STATE_AVAILABLE)
      st = fw::notes::newer(i.latest_version.c_str(), version()) ? esphome::update::UPDATE_STATE_AVAILABLE
                                                                  : esphome::update::UPDATE_STATE_NO_UPDATE;
  }
  notes_check(u);
}
// http: the request component the notes are fetched with; url: the manifest (the Update channel hook sets it again)
inline void watch(UpdateEntity *u, esphome::http_request::HttpRequestComponent *http, const char *url) {
  notes().http = http;
  notes().url = url;
  u->add_on_state_callback([u]() { fix(u); });
}
inline void set_url(const std::string &url) { notes().url = url; }
// the Update channel (workbench.yaml channel_hook, fw_settle.h Channel): the hook only records the manifest; tick()
// writes the stored settings to flash, then points the entity at it and checks it
inline fw::settle::Channel &channel() {
  static fw::settle::Channel c;
  return c;
}
inline void set_channel(const char *ch, const char *base) { channel().want(fw::settle::channel_url(base, ch)); }
inline void settling(UpdateEntity *u, int left, int total) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_INSTALLING;
  auto &i = u->*Peek::info();
  i.has_progress = true;
  i.progress = total > 0 ? 100.0f * (float) (total - left) / (float) total : 0.0f;
  i.current_version = version();
  if (i.latest_version.empty()) i.latest_version = version();
  char b[96];
  snprintf(b, sizeof b, "Update settling: don't unplug (%d:%02d). A restart from Home Assistant is fine.", left / 60, left % 60);
  i.summary = b;
  u->publish_state();
}
// confirmed (or no probation): up to date with this build, until the manifest check says otherwise
inline void up_to_date(UpdateEntity *u) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_NO_UPDATE;
  auto &i = u->*Peek::info();
  i.has_progress = false;
  i.progress = 0.0f;
  i.current_version = version();
  i.latest_version = version();
  i.summary.clear();
  u->publish_state();
}

// Settings > Update's tap (core/device.h update_check, workbench.yaml on_update_check): the entity's own check at once,
// the one Home Assistant's "check for updates" runs (a fresh manifest URL, so Home Assistant's entity shows the
// result too). tick() tells the screen when it ends: fine, or why not (no Wi-Fi, the server did not answer, no answer
// in a minute). A check already running (the hourly one, Home Assistant's) is waited for instead of a second.
struct Ask {
  bool on = false;
  int64_t at = 0;
};
inline Ask &ask() {
  static Ask a;
  return a;
}
inline void check_now(HttpRequestUpdate *u) {
  Ask &a = ask();
#ifdef USE_WIFI
  if (!esphome::wifi::global_wifi_component || !esphome::wifi::global_wifi_component->is_connected()) {
    fw::dev::update_checked("no Wi-Fi");
    return;
  }
#endif
  a.on = true, a.at = esp_timer_get_time();
  if (checking(u)) return;
  if (!notes().url.empty()) u->set_source_url(fresh(notes().url));
  u->update();
}
inline void ask_step(HttpRequestUpdate *u) {
  Ask &a = ask();
  if (!a.on) return;
  if (checking(u)) {
    if (esp_timer_get_time() - a.at < 60000000LL) return;
    a.on = false;
    fw::dev::update_checked("no answer in a minute");
    return;
  }
  a.on = false;
  fw::dev::update_checked(u->status_has_error() ? "the update server did not answer" : "");
}

inline void tick(HttpRequestUpdate *u, int left, int total) {
  ask_step(u);
  notes_take(u);
  channel().step(checking(u), [] { esphome::global_preferences->sync(); }, [u](const std::string &url) {
    u->set_source_url(fresh(url));
    set_url(url);
    u->update();
  });
  // every check asks a new manifest URL (?t=, past GitHub Pages' 10 min cache): the entity's hourly poll, Home
  // Assistant's "check for updates" and ours all start from whatever source_url holds then. Only between checks: the
  // entity's task reads the string for the whole fetch.
  if (!checking(u) && !notes().url.empty()) u->set_source_url(fresh(notes().url));
  const unsigned a = clock().step(left, esp_timer_get_time());
  if (a & fw::settle::SETTLING) settling(u, left, total);
  if (a & fw::settle::CONFIRM) fw::device::probation_confirm();
  if (a & fw::settle::UP_TO_DATE) up_to_date(u);
  if (a & fw::settle::CHECK) u->check();
}

}  // namespace panel::fwu
