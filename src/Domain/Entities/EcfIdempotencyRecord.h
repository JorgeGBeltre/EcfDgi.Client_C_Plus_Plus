#pragma once

#include <string>
#include <optional>

namespace ecf::domain {

enum class IdempotencyStatus {
    Processing,
    Completed,
    Failed
};

inline std::string idempotencyStatusToString(IdempotencyStatus s) {
    switch (s) {
        case IdempotencyStatus::Processing: return "Processing";
        case IdempotencyStatus::Completed: return "Completed";
        case IdempotencyStatus::Failed: return "Failed";
    }
    return "Processing";
}

inline IdempotencyStatus idempotencyStatusFromString(const std::string& s) {
    if (s == "Completed") return IdempotencyStatus::Completed;
    if (s == "Failed") return IdempotencyStatus::Failed;
    return IdempotencyStatus::Processing;
}

struct EcfIdempotencyRecord {
    std::string key; // Format: tenant_id:idempotency_key
    std::string createdByWorkerKeyId;
    std::string payloadHash;
    IdempotencyStatus status = IdempotencyStatus::Processing;
    int statusCode = 0;
    std::string contentType = "application/json";
    std::string responseBody;
    std::string createdAt; // ISO-8601 UTC
    std::optional<std::string> updatedAt; // ISO-8601 UTC
    std::string expiresAt; // ISO-8601 UTC
};

} // namespace ecf::domain
