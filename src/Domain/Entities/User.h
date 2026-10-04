#pragma once

#include <string>

#include "Domain/Common/AuditableEntity.h"

namespace ecf::domain {

struct User : AuditableEntity {
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string role;
    std::string tenantId;

    User() = default;
    User(std::string u, std::string r, std::string t = "")
        : username(std::move(u)), role(std::move(r)), tenantId(std::move(t)) {}
};

}  // namespace ecf::domain
