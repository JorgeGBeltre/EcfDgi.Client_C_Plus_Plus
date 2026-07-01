#pragma once

#include <string>

namespace ecf::infra {

struct JwtSettings {
    std::string secret;
    int expirationMinutes = 60;
    std::string issuer;
    std::string audience;
};

}  // namespace ecf::infra
