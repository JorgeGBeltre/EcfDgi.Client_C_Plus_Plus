#pragma once

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

    std::string auditUsername() const;

    void stage(std::function<void(pqxx::work&)> op) { pending_.push_back(std::move(op)); }

    void discardChanges() noexcept { pending_.clear(); }

    int saveChanges();

private:
    pqxx::connection conn_;
    std::shared_ptr<domain::ICurrentUserService> currentUser_;
    std::vector<std::function<void(pqxx::work&)>> pending_;
};

}
