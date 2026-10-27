// The panel's look and its one sent-theme slot (theme engine T4, design notes "Device and delivery"; CONTRACT.md §10).
//   look       setting "look" (tokens/settings.json, the Settings screen + the ESPHome select): facet | a built-in theme
//              id (themes/*.json) | sent. theme_select makes it live through the T1 parser and guards (fx::theme_apply):
//              a look that cannot be made live (no sent theme, a refused file) shows Facet and says why.
//   send       theme_send(json) = Home Assistant's facet_theme action (firmware/fw_device.h): the file is loaded and
//              guarded ON THE PANEL; refused = the answer lists the errors (path, message, line), the live look and
//              the slot stay exactly as they were. Accepted = it becomes the slot's last-good copy (stored), and the
//              look moves to "sent" (the select follows). theme_send("") drops the sent theme (look back to Facet if it
//              showed it).
//   storage    the host's store (the panel: NVS "facet_theme"/"sent"; tests: memory) holds ONE record:
//              "FXT1" + CRC32 (4 bytes, little endian) + the JSON. theme_storage(io) installs it and restores at boot:
//              a missing record = no sent theme; a bad record (magic, length, CRC) = erased, Facet, reported; a record
//              the parser refuses (e.g. a newer firmware dropped a family) = kept, Facet, reported. Never a crash or a
//              boot loop: nothing here can fail harder than "show Facet".
//   budgets    SENT_MAX file bytes (= the parser's limit), SLOT_MAX record bytes, PEAK_MAX bytes held while loading
//              (fx::theme_peak_bytes: scratch + the Skin); over a budget = refused with the numbers.
// Main loop / LVGL thread only (ESPHome actions and LVGL share it).
#pragma once
#include <stddef.h>
#include <functional>
#include <string>
#include "fw.h"

namespace fx {
constexpr size_t SENT_MAX = 16 * 1024, SLOT_MAX = SENT_MAX + 8, PEAK_MAX = 96 * 1024;
struct SlotIO {
  std::function<bool(std::string &)> load;        // the record, false = none
  std::function<bool(const std::string &)> save;  // false = not stored (the theme still applies; the answer says so)
  std::function<void()> erase;
};
void theme_storage(SlotIO io);                                         // install + restore (boot)
void theme_send(const char *json, size_t n, ArduinoJson::JsonObject out);  // {ok, id, label, bytes, stored, errors[..4], n_errors, live, gen}
bool theme_select(const std::string &look, std::string &why);          // make it live; false = Facet shown, why says
void theme_status(ArduinoJson::JsonObject out);                         // {look, live, gen, sent: {id, label, bytes} | null, boot}
std::string theme_record(const std::string &json);                      // the stored form (tests)
std::string theme_lab_slot(const std::string &cmd, const std::string &arg);  // fw_lab theme_slot_* (tests, workbench)
}  // namespace fx
