#pragma once
// Cross-cutting request behaviors:
//  - validateOrThrow: runs the request validator and throws on failures.
//  - LoggingScope:    logs request start/completion with elapsed time.
// These are invoked explicitly at the top of each controller action.

#include <chrono>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Application/Common/Exceptions/ValidationException.h"
#include "Application/Common/RequestValidators.h"

namespace ecf::app {

// validateOrThrow<TRequest>: throws ValidationException if the request fails.
template <typename TRequest>
void validateOrThrow(const TRequest& request) {
    std::vector<ValidationFailure> failures = validators::validate(request);
    if (!failures.empty()) throw ValidationException(failures);
}

// LoggingScope: RAII scope that logs start on construction and the elapsed
// time on destruction.
class LoggingScope {
public:
    explicit LoggingScope(std::string requestName)
        : requestName_(std::move(requestName)),
          start_(std::chrono::steady_clock::now()) {
        spdlog::info("Starting request {}", requestName_);
    }

    ~LoggingScope() {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_)
                            .count();
        spdlog::info("Completed request {} in {}ms", requestName_, ms);
    }

private:
    std::string requestName_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace ecf::app
