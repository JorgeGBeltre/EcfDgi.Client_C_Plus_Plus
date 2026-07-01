#pragma once

#include <optional>
#include <string>

#include "Domain/Entities/User.h"

namespace ecf::domain {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<User> getById(const std::string& id) = 0;
    virtual std::optional<User> getByUsername(const std::string& username) = 0;
    virtual std::optional<User> getByEmail(const std::string& email) = 0;
    virtual void add(const User& user) = 0;
    virtual void update(const User& user) = 0;
};

}  // namespace ecf::domain
