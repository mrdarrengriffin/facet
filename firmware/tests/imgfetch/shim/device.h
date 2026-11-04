#pragma once
// host shim for renderer core/device.h: the panel's Home Assistant address (a picture path's host)
#include <string>
namespace fw::dev::ha_link {
inline const std::string &url() { static const std::string u = "http://ha.test:8123"; return u; }
}  // namespace fw::dev::ha_link
