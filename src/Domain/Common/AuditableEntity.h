#pragma once

#include <optional>
#include <string>

namespace ecf::domain {

struct AuditableEntity {
    std::string id;
    std::string createdAt;
    std::optional<std::string> createdBy;
    std::optional<std::string> updatedAt;
    std::optional<std::string> updatedBy;
    std::optional<std::string> deletedAt;
    std::optional<std::string> deletedBy;
    bool isDeleted = false;

    virtual ~AuditableEntity() = default;
};

}
