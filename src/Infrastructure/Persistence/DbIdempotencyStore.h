#pragma once

#include <string>
#include "Domain/Interfaces/IIdempotencyStore.h"

namespace ecf::infra {

class DbIdempotencyStore : public domain::IIdempotencyStore {
public:
    explicit DbIdempotencyStore(std::string connectionString)
        : connectionString_(std::move(connectionString)) {}

    domain::IdempotencyReservationResult reserveOrGet(
        const std::string& key, 
        const std::string& payloadHash, 
        const std::string& workerKeyId) override;

    void complete(
        const std::string& key, 
        const domain::IdempotentResult& result) override;

private:
    std::string connectionString_;
};

} // namespace ecf::infra
