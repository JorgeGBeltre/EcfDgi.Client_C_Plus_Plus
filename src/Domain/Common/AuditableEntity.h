#pragma once
// Base for entities that carry audit + soft-delete metadata. The auditing
// fields are populated centrally by the persistence layer (see DbContext).

#include <optional>
#include <string>

namespace ecf::domain {

struct AuditableEntity {
    std::string id;                        // uuid (assigned on insert)
    std::string createdAt;                 // ISO-8601 UTC
    std::optional<std::string> createdBy;
    std::optional<std::string> updatedAt;
    std::optional<std::string> updatedBy;
    std::optional<std::string> deletedAt;
    std::optional<std::string> deletedBy;
    bool isDeleted = false;

    virtual ~AuditableEntity() = default;
};

}  // namespace ecf::domain
