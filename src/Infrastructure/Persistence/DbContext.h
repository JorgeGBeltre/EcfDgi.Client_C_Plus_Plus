#pragma once
// Per-request database context.
// Auditing is applied via a pending-operations queue committed atomically by
// UnitOfWork. Reads honour the soft-delete filter (is_deleted = false);
// deletes are turned into soft deletes.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <pqxx/pqxx>

#include "Domain/Interfaces/ISecurity.h"

namespace ecf::infra {

class DbContext {
public:
    DbContext(const std::string& connectionString,
              std::shared_ptr<domain::ICurrentUserService> currentUser);

    pqxx::connection& connection() { return conn_; }

    // Username used to stamp audit columns (falls back to "System").
    std::string auditUsername() const;

    // Queue a mutation to run inside the next saveChanges() transaction.
    void stage(std::function<void(pqxx::work&)> op) { pending_.push_back(std::move(op)); }

    // Executes and commits all staged mutations; returns the affected count.
    int saveChanges();

private:
    pqxx::connection conn_;
    std::shared_ptr<domain::ICurrentUserService> currentUser_;
    std::vector<std::function<void(pqxx::work&)>> pending_;
};

}  // namespace ecf::infra
