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

        // Migrations for existing deployments
        w.exec("ALTER TABLE ecf_documents ADD COLUMN IF NOT EXISTS edit_sequence varchar(50) NOT NULL DEFAULT '';");
        w.exec("ALTER TABLE ecf_documents ADD COLUMN IF NOT EXISTS sent_to_dgii_at timestamptz;");
        w.exec("ALTER TABLE ecf_documents ADD COLUMN IF NOT EXISTS last_status_check_at timestamptz;");
        w.exec("ALTER TABLE ecf_documents ADD COLUMN IF NOT EXISTS status_check_attempts integer NOT NULL DEFAULT 0;");
        w.exec("ALTER TABLE ecf_documents ALTER COLUMN rnc_comprador TYPE varchar(100);");
        w.exec("DROP INDEX IF EXISTS uq_ecf_documents_tenant_source_txn;");
        w.exec("CREATE UNIQUE INDEX IF NOT EXISTS uq_ecf_documents_tenant_source_txn ON ecf_documents (tenant_id, source_txn_id, ambiente);");
        w.exec("DROP INDEX IF EXISTS uq_ecf_documents_rnc_emisor_encf;");
        w.exec("CREATE UNIQUE INDEX IF NOT EXISTS uq_ecf_documents_rnc_emisor_encf ON ecf_documents (rnc_emisor, ambiente, e_ncf);");
        w.exec("CREATE INDEX IF NOT EXISTS ix_ecf_documents_state_last_status_check_at ON ecf_documents (state, last_status_check_at);");

        w.commit();
    }

    // 2. SEC-053: Purge legacy seeded admin user and insecure accounts. Users are managed via SaaS-Ecf-Back.
    {
        pqxx::work w(conn);
        w.exec("DELETE FROM users WHERE id = '9f3c7e09-8b2a-4c1d-9e3f-1a2b3c4d5e6f' OR username = 'admin' OR email LIKE '%@ecfdgii.client.com';");
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
