#pragma once

#include <chrono>
#include <string>

namespace ecf::sys {

std::string newUuid();

std::string utcNowIso();

std::string toIsoUtc(std::chrono::system_clock::time_point tp);
std::chrono::system_clock::time_point parseIsoUtc(const std::string& iso);

}
