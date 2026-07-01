#pragma once
// Built per request from the validated JWT claims.

#include <optional>
#include <string>

#include "Domain/Interfaces/ISecurity.h"

namespace ecf::api {

class CurrentUserService : public domain::ICurrentUserService {
public:
    CurrentUserService(std::optional<std::string> userId,
                       std::optional<std::string> username)
        : userId_(std::move(userId)), username_(std::move(username)) {}

    std::optional<std::string> userId() const override { return userId_; }
    std::optional<std::string> username() const override { return username_; }

private:
    std::optional<std::string> userId_;
    std::optional<std::string> username_;
};

}  // namespace ecf::api
