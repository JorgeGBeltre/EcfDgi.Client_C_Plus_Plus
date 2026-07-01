#pragma once

#include <string>

#include "Domain/Interfaces/ISecurity.h"
#include "Infrastructure/Security/JwtSettings.h"

namespace ecf::infra {

class TokenService : public domain::ITokenService {
public:
    explicit TokenService(JwtSettings settings) : settings_(std::move(settings)) {}

    std::string generateToken(const domain::User& user) override;

private:
    JwtSettings settings_;
};

}  // namespace ecf::infra
