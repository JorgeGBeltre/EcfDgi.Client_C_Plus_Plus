#pragma once

#include <chrono>

namespace ecf::infra {

class CertificateExpiryPolicy {
public:
    enum class ExpiryUrgency { Ok, Warning, Critical };

    static constexpr int WarningDays = 30;
    static constexpr int CriticalDays = 7;

    static ExpiryUrgency classify(std::chrono::system_clock::time_point now,
                                  std::chrono::system_clock::time_point notAfter) {
        auto remaining = notAfter - now;
        if (remaining <= std::chrono::hours(CriticalDays * 24)) {
            return ExpiryUrgency::Critical;
        }
        if (remaining <= std::chrono::hours(WarningDays * 24)) {
            return ExpiryUrgency::Warning;
        }
        return ExpiryUrgency::Ok;
    }
};

}
