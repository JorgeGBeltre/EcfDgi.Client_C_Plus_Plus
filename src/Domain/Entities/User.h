#pragma once

#include <string>

#include "Domain/Common/AuditableEntity.h"

namespace ecf::domain {

struct User : AuditableEntity {
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string role;
};

}  // namespace ecf::domain
