// engine_link_stub.h - test access to the host stub's command log.
#pragma once
#include <string>
#include <vector>

namespace mdnative {
// Every text line the UI sent to the stub engine, in order (without '\n').
std::vector<std::string>& stubCommandLog();
}  // namespace mdnative
