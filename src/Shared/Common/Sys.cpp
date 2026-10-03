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

    size_t start = 0;
    while (start < iso.size() && (iso[start] == ' ' || iso[start] == '\t' || iso[start] == '\r' || iso[start] == '\n')) {
        start++;
    }
    if (start >= iso.size()) return std::chrono::system_clock::time_point{};

    std::tm tm{};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    char sep = 0;
    int charsRead = 0;

    int matched = std::sscanf(iso.c_str() + start, "%d-%d-%d%c%d:%d:%d%n",
                              &year, &month, &day, &sep, &hour, &minute, &second, &charsRead);

    if (matched < 7 || (sep != 'T' && sep != 't' && sep != ' ')) {
        return std::chrono::system_clock::time_point{};
    }

    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        return std::chrono::system_clock::time_point{};
    }

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

    if (t == static_cast<std::time_t>(-1)) {
        return std::chrono::system_clock::time_point{};
    }

    const char* ptr = iso.c_str() + start + charsRead;

    // Skip fractional seconds (e.g. .123456)
    if (*ptr == '.') {
        ptr++;
        while (*ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
    }

    // Skip whitespace before timezone
    while (*ptr == ' ') {
        ptr++;
    }

    // Timezone offset
    if (*ptr == 'Z' || *ptr == 'z') {
        // UTC, no adjustment
    } else if (*ptr == '+' || *ptr == '-') {
        char sign = *ptr++;
        int tzHour = 0, tzMin = 0;
        if (std::sscanf(ptr, "%d:%d", &tzHour, &tzMin) >= 1) {
            // parsed tzHour and optional tzMin
        } else if (std::sscanf(ptr, "%2d%2d", &tzHour, &tzMin) >= 1) {
            // parsed 2-digit hour
        }
        long offsetSec = tzHour * 3600 + tzMin * 60;
        if (sign == '+') {
            t -= offsetSec;
        } else {
            t += offsetSec;
        }
    }

    return std::chrono::system_clock::from_time_t(t);
}

}  // namespace ecf::sys
