#pragma once
// Ports of the Application.Common.Interfaces security abstractions:
// IPasswordHasher, ITokenService, ICurrentUserService.
// Consolidated here to keep the C++ dependency graph acyclic.

#include <optional>
#include <string>

#include "Domain/Entities/User.h"

namespace ecf::domain {

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;
    virtual std::string hashPassword(const std::string& password) = 0;
    virtual bool verifyPassword(const std::string& password,
                                const std::string& hashedPassword) = 0;
};

class ITokenService {
public:
    virtual ~ITokenService() = default;
    virtual std::string generateToken(const User& user) = 0;
};

class ICurrentUserService {
public:
    virtual ~ICurrentUserService() = default;
    virtual std::optional<std::string> userId() const = 0;
    virtual std::optional<std::string> username() const = 0;
};

}  // namespace ecf::domain
