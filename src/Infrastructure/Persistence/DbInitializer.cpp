#include "Infrastructure/Persistence/DbInitializer.h"

#include <pqxx/pqxx>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Shared/Common/Sys.h"

namespace ecf::infra {

namespace {
std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("No se encontró el schema SQL: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

void DbInitializer::initialize(const std::string& connectionString,
                               const std::string& schemaPath,
                               domain::IPasswordHasher& hasher) {
    pqxx::connection conn(connectionString);

    // 1. Apply schema (idempotent CREATE TABLE IF NOT EXISTS ...).
    {
        const std::string sql = readAll(schemaPath);
        pqxx::work w(conn);
        w.exec(sql);
        w.commit();
    }

    // 2. Seed the default admin user (admin / AdminPassword123!).
    //    Uses a fixed id/email/date so the seed is stable across runs;
    //    the password hash is Argon2id (crypto_pwhash).
    {
        pqxx::work w(conn);
        pqxx::result r =
            w.exec_params("SELECT 1 FROM users WHERE username = $1", std::string("admin"));
        if (r.empty()) {
            const std::string hash = hasher.hashPassword("AdminPassword123!");
            w.exec_params(
                "INSERT INTO users (id, username, email, password_hash, role, "
                "created_at, created_by, is_deleted) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, false)",
                std::string("9f3c7e09-e85d-452f-9877-c93d90fcb32d"), std::string("admin"),
                std::string("admin@ecfdgii.client.com"), hash, std::string("Admin"),
                std::string("2026-06-26T00:00:00Z"), std::string("System"));
        }
        w.commit();
    }

    // 3. Seed the default "Consumidor Final" customer with a fixed
    //    id/RNC/date so the seed is stable across runs.
    {
        pqxx::work w(conn);
        pqxx::result r = w.exec_params(
            "SELECT 1 FROM customers WHERE id = $1",
            std::string("f98f6d61-d24f-4a0b-967b-1d7c0f135b5a"));
        if (r.empty()) {
            w.exec_params(
                "INSERT INTO customers (id, name, email, rnc, created_at, created_by, "
                "is_deleted) VALUES ($1, $2, $3, $4, $5, $6, false)",
                std::string("f98f6d61-d24f-4a0b-967b-1d7c0f135b5a"),
                std::string("Consumidor Final Gen\xC3\xA9rico"),
                std::string("consumidorfinal@ecfdgii.client.com"),
                std::string("22400013743"), std::string("2026-06-26T00:00:00Z"),
                std::string("System"));
        }
        w.commit();
    }
}

}  // namespace ecf::infra
