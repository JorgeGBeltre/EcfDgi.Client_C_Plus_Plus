#pragma once
// Small cross-cutting helpers used across layers (UUID v4, UTC timestamps).

#include <chrono>
#include <string>

namespace ecf::sys {

// Generates an RFC-4122 v4 UUID string (lowercase, hyphenated).
std::string newUuid();

// Current UTC time formatted as ISO-8601 "yyyy-MM-ddTHH:mm:ssZ".
std::string utcNowIso();

std::string toIsoUtc(std::chrono::system_clock::time_point tp);
std::chrono::system_clock::time_point parseIsoUtc(const std::string& iso);

}  // namespace ecf::sys
