#include "Shared/Common/Sys.h"

#include <openssl/rand.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace ecf::sys {

std::string newUuid() {
    std::array<unsigned char, 16> b{};
    RAND_bytes(b.data(), static_cast<int>(b.size()));

    // Set version (4) and variant (RFC 4122) bits.
    b[6] = static_cast<unsigned char>((b[6] & 0x0F) | 0x40);
    b[8] = static_cast<unsigned char>((b[8] & 0x3F) | 0x80);

    char out[37];
    std::snprintf(out, sizeof(out),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                  b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return std::string(out);
}

std::string utcNowIso() {
    return toIsoUtc(std::chrono::system_clock::now());
}

std::string toIsoUtc(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::chrono::system_clock::time_point parseIsoUtc(const std::string& iso) {
    if (iso.empty()) return std::chrono::system_clock::time_point{};
    std::tm tm{};
    int year, month, day, hour, minute, second;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) >= 6) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;
#if defined(_WIN32)
        std::time_t t = _mkgmtime(&tm);
#else
        std::time_t t = timegm(&tm);
#endif
        return std::chrono::system_clock::from_time_t(t);
    }
    return std::chrono::system_clock::time_point{};
}

}  // namespace ecf::sys
