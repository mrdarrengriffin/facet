#pragma once
// HA "Firmware" update entity (ESPHome http_request update, packages/workbench.yaml) with the OTA probation folded in
// (no separate "Firmware on probation" entity). While the running image is not confirmed yet
// (fw::device::probation_left() >= 0, firmware/fw_device.h) the entity reads "installing" with the
// settling countdown as its progress and the summary "Update settling: don't unplug (m:ss)"; a power cut now rolls
// back. When confirmed it checks the manifest again (normal "up to date" / "update available"). Installs only when
// someone presses Install in HA: nothing here or in the YAML installs on its own.
// tick() runs every second (YAML interval). Also does the first manifest check 30 s after boot, because the entity's
// own check at setup runs before Wi-Fi is up and its next poll is hours away.
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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/components/http_request/update/http_request_update.h"
#include "esphome/components/json/json_util.h"
#include "fw_notes.h"

namespace fw::bundle {
extern const char *const VERSION;  // facet_wb fw_bundle_info.cpp
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
inline const char *version() {
  if (fw::bundle::VERSION[0]) return fw::bundle::VERSION;
#ifdef ESPHOME_PROJECT_VERSION
  return ESPHOME_PROJECT_VERSION;
#else
  return "";
#endif
}
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
  auto *job = new NotesJob{n.url, i.current_version};
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

// the entity's check compares the manifest with ESPHOME_PROJECT_VERSION: compare with the build's version instead
inline void fix(UpdateEntity *u) {
  auto &i = u->*Peek::info();
  if (version()[0] && i.current_version != version()) {
    i.current_version = version();
    auto &st = u->*Peek::state();
    if (st == esphome::update::UPDATE_STATE_NO_UPDATE || st == esphome::update::UPDATE_STATE_AVAILABLE)
      st = i.latest_version.empty() || i.latest_version == version() ? esphome::update::UPDATE_STATE_NO_UPDATE
                                                                      : esphome::update::UPDATE_STATE_AVAILABLE;
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
inline void settled(UpdateEntity *u) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_UNKNOWN;
  auto &i = u->*Peek::info();
  i.has_progress = false;
  i.summary.clear();
  u->publish_state();
}

inline void tick(HttpRequestUpdate *u, int left, int total) {
  static int was = -2;
  static bool checked = false;
  notes_take(u);
  if (left >= 0) {
    if (left != was) settling(u, left, total);
    was = left;
    return;
  }
  if (was >= 0) {  // just confirmed
    was = -1;
    settled(u);
    u->check();
    checked = true;
    return;
  }
  was = -1;
  if (!checked && esp_timer_get_time() > 30000000LL) {
    checked = true;
    u->check();
  }
}

}  // namespace panel::fwu
