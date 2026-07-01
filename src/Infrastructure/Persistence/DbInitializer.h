#pragma once
// Applies the database schema at startup and seeds the default admin user.
// Applies db/schema.sql (idempotent) and seeds the default admin user.

#include <memory>
#include <string>

#include "Domain/Interfaces/ISecurity.h"

namespace ecf::infra {

class DbInitializer {
public:
    // Applies the schema and seeds admin/AdminPassword123! if missing.
    static void initialize(const std::string& connectionString,
                           const std::string& schemaPath,
                           domain::IPasswordHasher& hasher);
};

}  // namespace ecf::infra
