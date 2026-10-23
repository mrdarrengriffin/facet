// Match picker of the live sport screens (tennis_live, football_live; WIP 2026-10-23): the Media speaker pattern.
//   button   the Speaker button component in the screen's Header in place of the time (prop icon = the sport's mark,
//            accent while the shown match is live): the pick ("Ningbo · WTA"), "Sample" with no pick
//   sheet    the Bottom sheet over the whole screen, host rows (lay::sheet_row_h each), two levels:
//     tournaments  a Picker row "Sample match" (the simulator; selected with no pick), then one List row per
//                  tournament on today (title = name · tour, sub = "2 live · 8 today", chevron): a tap opens its matches
//     matches      the first row = Icon button back (square, to the tournaments) + Segmented Live / Upcoming /
//                  Results (default: Live when any is live, else Upcoming, else Results), then a Picker row per match
//                  of that tab (name "Jovic v Shnaider"; status "live · 3-4 · 15-30" in the working tone / "starts
//                  14:00" / "final · 6-4 6-3"; the picked match's radio on); a pick closes the sheet
//   Rows move in place while the sheet is open (the host polls): same rows = each row's update path (status text,
//   tone, radio), other rows = the list again.
// Data in (screen props, from the host = the hub's /api/sport/<sport>/*; nothing is fetched here):
//   pick    the button text; match = the picked match id ("" = the simulator)
//   picker  JSON (text or object) {tournaments:[{id, name, tour, live, count}], event:{id, name}, matches:[{id,
//           state, players:[a, b], sets?:[[a,b]..], points?:[a,b], score?:[a,b], clock?:{min, added?}, starts?,
//           winner?}], error?}: what the host has (missing = "Loading")
//   delay   seconds the TV runs behind the data (0-300): the host sends the feed that late (hub delay buffer)
// TV sync (Darren 2026-10-23): the sheet's first row "Sync with TV" (sub "TV +38 s", List row, chevron) closes the
//   sheet and starts sync mode: a slim bar along the bottom over the screen (over the court, never the hero score),
//   bg + line-hi hairline on top, button-h + 2 x space 2 tall (80 / 52): Pause / Play (primary Button, width 88), the
//   readout "TV +38 s" (label, centred in the room left), -5 / -1 / +1 / +5 s (outline Buttons; 480: -1 / +1) and Done (secondary). Pause holds the
//   screen's feed and shows a Status chip "paused +12 s" (warn, counting) in the header before the button; Play adds the
//   time spent paused to the delay. A tap on the hero toggles Pause / Play too. Done (or 30 s untouched, never while
//   paused) hides the bar; Done while paused plays first.
// Events out (fw::emit on the screen root; the host acts): "browse" "tournaments" (the sheet opened on the list) |
//   "browse" "event <id>" (a tournament's matches wanted), "pick" "<match id>" ("" = back to the simulator),
//   "delay" "<s>" (a step or Play: keep it, ask for the feed that late), "pause" / "play", "sync" "on" | "off".
#pragma once
#include "fw.h"

namespace fw::sport {
struct Picker;
// sport: 't' tennis | 'm' football. Builds the button into `header` (the Header component's row; its chrome = the time
// is hidden and kept last, so the Header's own update path still finds it) and the sheet over `root`.
Picker *picker_attach(lv_obj_t *root, lv_obj_t *header, char sport, fw::Props p);
void picker_update(Picker *k, fw::Props p);      // new props: pick, match, picker
void picker_live(Picker *k, bool live);          // the shown match is live (the button's icon tone)
bool picker_paused(const Picker *k);             // TV sync paused: the screen holds its feed (push nothing)
void picker_hero(Picker *k, lv_obj_t *hero);     // in sync mode a tap on the hero toggles Pause / Play
void picker_free(Picker *k);
}  // namespace fw::sport
