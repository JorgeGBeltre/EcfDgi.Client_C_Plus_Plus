#pragma once

#include <string>

#include "Domain/Common/AuditableEntity.h"

namespace ecf::domain {

struct Customer : AuditableEntity {
    std::string name;
    std::string email;
    std::string rnc;
};

}  // namespace ecf::domain
