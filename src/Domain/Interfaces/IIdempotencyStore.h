#pragma once

#include <string>
#include <optional>
#include <memory>

namespace ecf::domain {

struct IdempotentResult {
    int statusCode = 200;
    std::string contentType = "application/json";
    std::string body;
};

enum class IdempotencyReservationStatus {
    Reserved,
    AlreadyCompleted,
    AlreadyProcessing,
    PayloadMismatch
};

struct IdempotencyReservationResult {
    IdempotencyReservationStatus status;
    std::optional<IdempotentResult> completedResult;
};

class IIdempotencyStore {
public:
    virtual ~IIdempotencyStore() = default;

    virtual IdempotencyReservationResult reserveOrGet(
        const std::string& key, 
        const std::string& payloadHash, 
        const std::string& workerKeyId) = 0;

    virtual void complete(
        const std::string& key, 
        const IdempotentResult& result) = 0;
};

} // namespace ecf::domain
