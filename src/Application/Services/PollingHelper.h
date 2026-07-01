#pragma once
// Polls fn() with exponential backoff until isComplete() is true, or until the
// retry/timeout limits are hit.

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>

#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Exceptions/EcfException.h"

namespace ecf::app {

class PollingHelper {
public:
    template <typename T>
    static T pollUntilComplete(const std::function<T()>& fn,
                               const std::function<bool(const T&)>& isComplete,
                               const domain::PollingOptions& options = {}) {
        int delay = options.initialDelayMs;
        int retries = 0;
        const auto startTime = std::chrono::steady_clock::now();

        while (true) {
            T result = fn();
            if (isComplete(result)) return result;

            ++retries;
            if (retries >= options.maxRetries)
                throw domain::PollingMaxRetriesException(options.maxRetries);

            if (options.timeoutMs.has_value()) {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                if (elapsed >= *options.timeoutMs)
                    throw domain::PollingTimeoutException();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = static_cast<int>(
                std::min(delay * options.backoffMultiplier,
                         static_cast<double>(options.maxDelayMs)));
        }
    }
};

}  // namespace ecf::app
