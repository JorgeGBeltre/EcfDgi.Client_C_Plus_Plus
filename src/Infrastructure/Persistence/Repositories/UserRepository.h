#pragma once

#include <memory>

#include "Domain/Interfaces/IUserRepository.h"
#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::infra {

class UserRepository : public domain::IUserRepository {
public:
    explicit UserRepository(std::shared_ptr<DbContext> db) : db_(std::move(db)) {}

    std::optional<domain::User> getById(const std::string& id) override;
    std::optional<domain::User> getByUsername(const std::string& username) override;
    std::optional<domain::User> getByEmail(const std::string& email) override;
    void add(const domain::User& user) override;
    void update(const domain::User& user) override;

private:
    std::shared_ptr<DbContext> db_;
};

}  // namespace ecf::infra
