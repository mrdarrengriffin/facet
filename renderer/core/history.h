#pragma once
// History (core/history.cpp, #200): a sensor's last 24 hourly means on panel.history.<sensor>, for a Home Sparkline.
#include <string>
namespace fw::dev::history {
void track(const std::string &e);  // a sensor a card shows: read with the others while panel.history is shown
}
