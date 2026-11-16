#pragma once

#include <memory>
#include <string>

#include "Domain/Interfaces/ISecurity.h"

namespace ecf::infra {

class DbInitializer {
public:

    static void initialize(const std::string& connectionString,
                           const std::string& schemaPath,
                           domain::IPasswordHasher& hasher);
};

}
